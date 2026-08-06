#include "core/updater/github_release.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"
#include "core/updater/update_signature_verifier.h"
#include "core/updater/version.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QPushButton>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <utility>

namespace
{
QString sha256Hex(
    const QByteArray& data
    )
{
    return QString::fromLatin1(
        QCryptographicHash::hash(
            data,
            QCryptographicHash::Sha256
            ).toHex()
        );
}

QString assetName(
    const QString& version,
    const QString& platformKey
    )
{
    if (platformKey == QStringLiteral("windows-x64"))
    {
        return QStringLiteral("ClassMngr-%1-win-x64.exe")
            .arg(version);
    }

    if (platformKey == QStringLiteral("windows-arm64"))
    {
        return QStringLiteral("ClassMngr-%1-win-arm64.exe")
            .arg(version);
    }

    if (platformKey == QStringLiteral("macos-universal"))
    {
        return QStringLiteral("ClassMngr-%1-macos-universal.dmg")
            .arg(version);
    }

    return QStringLiteral("ClassMngr-%1-linux-x86_64.tar.gz")
        .arg(version);
}

QJsonObject releaseAsset(
    const QString& version,
    const QString& platformKey,
    const QString& digest = QStringLiteral(
        "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        ),
    const QString& nameOverride = QString()
    )
{
    const QString name =
        nameOverride.isEmpty()
            ? assetName(version, platformKey)
            : nameOverride;

    QJsonObject asset;
    asset.insert(QStringLiteral("name"), name);
    asset.insert(QStringLiteral("state"), QStringLiteral("uploaded"));
    asset.insert(QStringLiteral("size"), 123);
    asset.insert(QStringLiteral("digest"), digest);
    asset.insert(
        QStringLiteral("browser_download_url"),
        QStringLiteral("https://github.com/example/releases/download/v%1/%2")
            .arg(version, name)
        );
    return asset;
}

QJsonObject releaseObject(
    const QString& version,
    const QString& platformKey,
    bool draft = false,
    bool prerelease = false,
    bool includeAsset = true
    )
{
    QJsonArray assets;

    if (includeAsset)
    {
        assets.append(
            releaseAsset(
                version,
                platformKey
                )
            );
    }

    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), QStringLiteral("v") + version);
    release.insert(
        QStringLiteral("html_url"),
        QStringLiteral("https://github.com/example/releases/tag/v%1")
            .arg(version)
        );
    release.insert(
        QStringLiteral("published_at"),
        QStringLiteral("2026-08-05T14:49:05Z")
        );
    release.insert(QStringLiteral("draft"), draft);
    release.insert(QStringLiteral("prerelease"), prerelease);
    release.insert(QStringLiteral("assets"), assets);
    return release;
}

QByteArray releasesJson(
    const QJsonArray& releases
    )
{
    return QJsonDocument(releases).toJson(
        QJsonDocument::Compact
        );
}

QByteArray signedTestPayload()
{
    return QByteArrayLiteral(
        "ClassMngr signed update test payload\n"
        );
}

QString signedTestPublicKey()
{
    return QStringLiteral(
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAwATl2A7s6TM7zJOqBqLK\n"
        "JOdItidzZCAj3khX6ocjSpS5aYiB9rADMIp8hiIhAFl3KWnzzQP4O+1weZqFMs9V\n"
        "ZmV/5MOXJqMjgbZBGkqR2HhhZSzWGS2WGAdR+IuV8bIDuWwG0C87Yr3znrV71Jnz\n"
        "9YcK4mOVCSbuFAxlQPFDdoxsv4jlhkC0CyHc/NN/LulQ5SlNPjpmb8MtcfA8r/2p\n"
        "m+r4wQqjLvJ8DUankd6N/RhsY/98NR+ePwgv+P/KJu70dasOCepD9MLmlh+w0l5t\n"
        "XDWQo+7v88ERhUZ4jy/uRwMBxCJnCs3kXR60yTA4XOb3dv1ikjHIUDAZzh6qid14\n"
        "CwIDAQAB\n"
        "-----END PUBLIC KEY-----\n"
        );
}

QByteArray signedTestSignature()
{
    return QByteArrayLiteral(
        "nnkIFQaNkYSTdkStT8bERwohSAex57rXY8qVd13/Bw1i6Pifsl3D1/3KVufa6VNK"
        "8DIIjk4ywBpePKQBvi1hM07prQELnEEE9PCpwOMoSPKaRPmisaXMFlN4VH52sUNzE"
        "ggztWAAH+rNgo9INrqw8FC9KIQQKU+ymgay7FKNXU+QRAyJCEbp2D6J2u13FvmPG"
        "pFR4hwZGKYt6RvFomEmH4JmKkwgX0bEEagtoi36cXAuYd+RTfMjTjuKqbeGtUB0F"
        "0/QRwScJo2LZFq/n/o5JUI8bRwONK7EiK6m7va7QX4Yce81yA6wq8Q5Ek52Hfod"
        "7DuUZuy3QJ4IoN+f5cCp9A=="
        );
}

class HttpServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpServer(
        QByteArray body,
        QObject* parent = nullptr
        )
        : QObject(parent)
        , m_body(std::move(body))
    {
        connect(
            &m_server,
            &QTcpServer::newConnection,
            this,
            &HttpServer::respond
            );
    }

    bool listen()
    {
        return m_server.listen(
            QHostAddress(QStringLiteral("127.0.0.1")),
            0
            );
    }

    QUrl url(
        const QString& path = QStringLiteral("/releases")
        ) const
    {
        return QUrl(
            QStringLiteral("http://127.0.0.1:%1%2")
                .arg(m_server.serverPort())
                .arg(path)
            );
    }

    void setBody(
        QByteArray body
        )
    {
        m_body =
            std::move(body);
    }

private slots:
    void respond()
    {
        QTcpSocket* socket =
            m_server.nextPendingConnection();

        if (!socket)
        {
            return;
        }

        connect(
            socket,
            &QTcpSocket::readyRead,
            socket,
            [this, socket]()
            {
                socket->readAll();

                const QByteArray headers =
                    QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                    + QByteArrayLiteral("Content-Type: application/json\r\n")
                    + QByteArrayLiteral("Content-Length: ")
                    + QByteArray::number(m_body.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");

                socket->write(headers);
                socket->write(m_body);
                socket->disconnectFromHost();
            }
            );
        connect(
            socket,
            &QTcpSocket::disconnected,
            socket,
            &QTcpSocket::deleteLater
            );
    }

private:
    QTcpServer m_server;
    QByteArray m_body;
};
}

class UpdaterTests : public QObject
{
    Q_OBJECT

private slots:
    void versionRejectsNonStrictFormat();
    void versionComparesNumerically();
    void githubParserRejectsMalformedResponse();
    void githubParserSkipsDraftPrereleaseAndIncompleteReleases();
    void githubParserRequiresDigest();
    void githubParserSelectsArm64BeforeX64Fallback();
    void githubParserAcceptsLegacyLinuxArchiveName();
    void freshnessBoundaryIsExactlySixHours();
    void serviceCachesSuccessfulResultAndSkipsFreshCheck();
    void dialogShowsDisabledResourcePackSection();
    void dialogShowsDownloadActionForAvailableUpdate();
    void signatureVerifierAcceptsValidSignature();
    void signatureVerifierRejectsChangedPayload();
    void downloaderRejectsChecksumMismatch();
    void downloaderRejectsOversizedPayload();
    void downloaderCanCancelAndRemovesPartialFile();
};

void UpdaterTests::versionRejectsNonStrictFormat()
{
    QVERIFY(!Version::parse(QStringLiteral("1.2")).has_value());
    QVERIFY(!Version::parse(QStringLiteral("v1.2.3")).has_value());
}

void UpdaterTests::versionComparesNumerically()
{
    const auto lower =
        Version::parse(QStringLiteral("1.2.9"));
    const auto higher =
        Version::parse(QStringLiteral("1.10.0"));

    QVERIFY(lower.has_value());
    QVERIFY(higher.has_value());
    QVERIFY(*higher > *lower);
}

void UpdaterTests::githubParserRejectsMalformedResponse()
{
    QVERIFY(
        !GitHubRelease::latestCompatibleFromJson(
            QByteArrayLiteral("{bad json"),
            {QStringLiteral("windows-x64")}
            )
        );
}

void UpdaterTests::githubParserSkipsDraftPrereleaseAndIncompleteReleases()
{
    QJsonArray releases;
    releases.append(
        releaseObject(
            QStringLiteral("2.0.0"),
            QStringLiteral("windows-x64"),
            true
            )
        );
    releases.append(
        releaseObject(
            QStringLiteral("1.9.0"),
            QStringLiteral("windows-x64"),
            false,
            true
            )
        );
    releases.append(
        releaseObject(
            QStringLiteral("1.8.0"),
            QStringLiteral("windows-x64"),
            false,
            false,
            false
            )
        );
    releases.append(
        releaseObject(
            QStringLiteral("1.7.0"),
            QStringLiteral("windows-x64")
            )
        );

    const auto release =
        GitHubRelease::latestCompatibleFromJson(
            releasesJson(releases),
            {QStringLiteral("windows-x64")}
            );

    QVERIFY(release.has_value());
    QCOMPARE(release->version().toString(), QStringLiteral("1.7.0"));
}

void UpdaterTests::githubParserRequiresDigest()
{
    QJsonObject release =
        releaseObject(
            QStringLiteral("1.0.0"),
            QStringLiteral("windows-x64")
            );
    QJsonArray assets;
    assets.append(
        releaseAsset(
            QStringLiteral("1.0.0"),
            QStringLiteral("windows-x64"),
            QString()
            )
        );
    release.insert(QStringLiteral("assets"), assets);

    QVERIFY(
        !GitHubRelease::latestCompatibleFromJson(
            releasesJson({release}),
            {QStringLiteral("windows-x64")}
            )
        );
}

void UpdaterTests::githubParserSelectsArm64BeforeX64Fallback()
{
    QJsonObject release =
        releaseObject(
            QStringLiteral("1.0.0"),
            QStringLiteral("windows-x64")
            );
    QJsonArray assets =
        release.value(QStringLiteral("assets")).toArray();
    assets.append(
        releaseAsset(
            QStringLiteral("1.0.0"),
            QStringLiteral("windows-arm64")
            )
        );
    release.insert(QStringLiteral("assets"), assets);

    const auto parsed =
        GitHubRelease::latestCompatibleFromJson(
            releasesJson({release}),
            {
                QStringLiteral("windows-arm64"),
                QStringLiteral("windows-x64")
            }
            );

    QVERIFY(parsed.has_value());
    QCOMPARE(
        parsed->artifact().platformKey,
        QStringLiteral("windows-arm64")
        );
}

void UpdaterTests::githubParserAcceptsLegacyLinuxArchiveName()
{
    QJsonObject release =
        releaseObject(
            QStringLiteral("1.0.0"),
            QStringLiteral("linux-x86_64")
            );
    QJsonArray assets;
    assets.append(
        releaseAsset(
            QStringLiteral("1.0.0"),
            QStringLiteral("linux-x86_64"),
            QStringLiteral(
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                ),
            QStringLiteral("ClassMngr-linux-x86_64.tar.gz")
            )
        );
    release.insert(QStringLiteral("assets"), assets);

    const auto parsed =
        GitHubRelease::latestCompatibleFromJson(
            releasesJson({release}),
            {QStringLiteral("linux-x86_64")}
            );

    QVERIFY(parsed.has_value());
    QCOMPARE(
        parsed->artifact().fileName,
        QStringLiteral("ClassMngr-linux-x86_64.tar.gz")
        );
}

void UpdaterTests::freshnessBoundaryIsExactlySixHours()
{
    const QDateTime checkedAt =
        QDateTime::fromString(
            QStringLiteral("2026-08-06T00:00:00Z"),
            Qt::ISODate
            );

    QVERIFY(
        UpdateService::isTimestampFresh(
            checkedAt,
            checkedAt.addSecs(6 * 60 * 60)
            )
        );
    QVERIFY(
        !UpdateService::isTimestampFresh(
            checkedAt,
            checkedAt.addSecs(6 * 60 * 60).addMSecs(1)
            )
        );
}

void UpdaterTests::serviceCachesSuccessfulResultAndSkipsFreshCheck()
{
    QJsonArray releases;
    releases.append(
        releaseObject(
            QStringLiteral("9.9.9"),
            GitHubRelease::currentPlatformKeys().first()
            )
        );

    HttpServer server(
        releasesJson(releases)
        );
    QVERIFY(server.listen());

    QCoreApplication::setApplicationVersion(
        QStringLiteral("1.0.0")
        );

    UpdateConfiguration configuration;
    configuration.releasesApiUrl =
        server.url();

    UpdateService service(configuration);
    QSignalSpy succeededSpy(
        &service,
        &UpdateService::checkSucceeded
        );

    QVERIFY(
        service.checkForUpdates(
            UpdateService::CheckPolicy::Force
            )
        );
    QVERIFY(
        !service.checkForUpdates(
            UpdateService::CheckPolicy::Force
            )
        );
    QVERIFY(succeededSpy.wait(5000));
    QVERIFY(service.hasResult());
    QVERIFY(service.isResultFresh());
    QVERIFY(
        !service.checkForUpdates(
            UpdateService::CheckPolicy::IfStale
            )
    );
    QCOMPARE(succeededSpy.count(), 1);

    server.setBody(
        QByteArrayLiteral("{bad json")
        );
    QSignalSpy failedSpy(
        &service,
        &UpdateService::checkFailed
        );
    QVERIFY(
        service.checkForUpdates(
            UpdateService::CheckPolicy::Force
            )
        );
    QVERIFY(failedSpy.wait(5000));
    QVERIFY(service.hasResult());
    QVERIFY(service.lastResult()->updateAvailable);
}

void UpdaterTests::dialogShowsDisabledResourcePackSection()
{
    UpdateConfiguration configuration;
    configuration.releasesApiUrl =
        QUrl(QStringLiteral("https://example.com/releases"));
    UpdateService service(configuration);

    UpdateDialog dialog(
        &service,
        true
        );

    QFrame* resourceSection =
        dialog.findChild<QFrame*>(
            QStringLiteral("resourcePackUpdateSection")
            );

    QVERIFY(resourceSection);
    QVERIFY(!resourceSection->isEnabled());
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("updateCheckButton")
            )
        );
}

void UpdaterTests::dialogShowsDownloadActionForAvailableUpdate()
{
    QJsonArray releases;
    releases.append(
        releaseObject(
            QStringLiteral("9.9.9"),
            GitHubRelease::currentPlatformKeys().first()
            )
        );

    HttpServer server(
        releasesJson(releases)
        );
    QVERIFY(server.listen());

    QCoreApplication::setApplicationVersion(
        QStringLiteral("1.0.0")
        );

    UpdateConfiguration configuration;
    configuration.releasesApiUrl =
        server.url();
    UpdateService service(configuration);
    QSignalSpy succeededSpy(
        &service,
        &UpdateService::checkSucceeded
        );

    QVERIFY(
        service.checkForUpdates(
            UpdateService::CheckPolicy::Force
            )
        );
    QVERIFY(succeededSpy.wait(5000));

    UpdateDialog dialog(
        &service,
        true
        );
    dialog.refreshForOpen();

    QPushButton* primaryButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("updatePrimaryButton")
            );
    QPushButton* closeButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("updateCloseButton")
            );

    QVERIFY(primaryButton);
    QVERIFY(!primaryButton->isHidden());
    QCOMPARE(primaryButton->text(), QStringLiteral("Download Update"));
    QVERIFY(closeButton);
    QCOMPARE(closeButton->text(), QStringLiteral("Not Now"));
}

void UpdaterTests::signatureVerifierAcceptsValidSignature()
{
    const auto status =
        UpdateSignatureVerifier::verifyDetachedSignature(
            signedTestPayload(),
            signedTestSignature(),
            signedTestPublicKey()
            );

    if (!status)
    {
        QFAIL(qPrintable(status.error()));
    }
}

void UpdaterTests::signatureVerifierRejectsChangedPayload()
{
    QVERIFY(
        !UpdateSignatureVerifier::verifyDetachedSignature(
            signedTestPayload() + QByteArrayLiteral("changed"),
            signedTestSignature(),
            signedTestPublicKey()
            )
        );
}

void UpdaterTests::downloaderRejectsChecksumMismatch()
{
    const QByteArray body =
        QByteArrayLiteral("downloaded bytes");
    HttpServer server(body);
    QVERIFY(server.listen());

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        QStringLiteral("ClassMngr-test-update.bin");
    artifact.sha256 =
        sha256Hex(QByteArrayLiteral("different bytes"));
    artifact.sizeBytes =
        body.size();

    UpdateDownloader downloader;
    QSignalSpy failedSpy(
        &downloader,
        &UpdateDownloader::downloadFailed
        );

    downloader.download(artifact);

    QVERIFY(failedSpy.wait(5000));
    QVERIFY(
        failedSpy.takeFirst().first().toString().contains(
            QStringLiteral("checksum"),
            Qt::CaseInsensitive
            )
        );
}

void UpdaterTests::downloaderRejectsOversizedPayload()
{
    const QByteArray body =
        QByteArrayLiteral("larger than declared");
    HttpServer server(body);
    QVERIFY(server.listen());

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        QStringLiteral("ClassMngr-oversize-test.bin");
    artifact.sha256 =
        sha256Hex(body);
    artifact.sizeBytes =
        3;

    UpdateDownloader downloader;
    QSignalSpy failedSpy(
        &downloader,
        &UpdateDownloader::downloadFailed
        );

    downloader.download(artifact);

    QVERIFY(failedSpy.wait(5000));
    QVERIFY(!downloader.isBusy());
}

void UpdaterTests::downloaderCanCancelAndRemovesPartialFile()
{
    const QByteArray body(
        1024 * 1024,
        'x'
        );
    HttpServer server(body);
    QVERIFY(server.listen());

    const QString fileName =
        QStringLiteral("ClassMngr-cancel-test.bin");

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        fileName;
    artifact.sha256 =
        sha256Hex(body);
    artifact.sizeBytes =
        body.size();

    UpdateDownloader downloader;
    QSignalSpy cancelledSpy(
        &downloader,
        &UpdateDownloader::downloadCancelled
        );

    downloader.download(artifact);
    downloader.cancel();

    QCOMPARE(cancelledSpy.count(), 1);

    const QString partialPath =
        QDir(
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation
                )
            ).filePath(
                QStringLiteral("ClassMngr/updates/%1.part")
                    .arg(fileName)
                );

    QVERIFY(!QFileInfo::exists(partialPath));
}

QTEST_MAIN(UpdaterTests)

#include "updater_tests.moc"
