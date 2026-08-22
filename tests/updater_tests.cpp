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
#include <QFile>
#include <QFrame>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMetaObject>
#include <QProgressBar>
#include <QSignalSpy>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
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

void writeDownloadRecord(
    const QString& directory,
    const UpdateArtifact& artifact,
    const QByteArray& partialData,
    const QString& status = QStringLiteral("partial"),
    const QString& etag = QStringLiteral("\"test-etag\"")
    )
{
    const QString finalPath =
        QDir(directory).filePath(
            QFileInfo(artifact.fileName).fileName()
            );
    const QString dataPath =
        status == QStringLiteral("completed")
            ? finalPath
            : finalPath + QStringLiteral(".part");

    QFile dataFile(dataPath);
    QVERIFY(dataFile.open(QIODevice::WriteOnly));
    QCOMPARE(dataFile.write(partialData), partialData.size());
    dataFile.close();

    QJsonObject record;
    record.insert(QStringLiteral("schemaVersion"), 1);
    record.insert(QStringLiteral("platform"), artifact.platformKey);
    record.insert(QStringLiteral("url"), artifact.url.toString());
    record.insert(QStringLiteral("fileName"), artifact.fileName);
    record.insert(QStringLiteral("sha256"), artifact.sha256);
    record.insert(QStringLiteral("sizeBytes"), artifact.sizeBytes);
    record.insert(QStringLiteral("etag"), etag);
    record.insert(QStringLiteral("lastModified"), QString());
    record.insert(QStringLiteral("status"), status);
    record.insert(
        QStringLiteral("createdAtUtc"),
        QStringLiteral("2026-08-01T00:00:00.000Z")
        );
    record.insert(
        QStringLiteral("updatedAtUtc"),
        QStringLiteral("2026-08-01T00:00:00.000Z")
        );

    QFile metadataFile(
        finalPath + QStringLiteral(".download.json")
        );
    QVERIFY(metadataFile.open(QIODevice::WriteOnly));
    const QByteArray metadata =
        QJsonDocument(record).toJson(
            QJsonDocument::Compact
            );
    QCOMPARE(metadataFile.write(metadata), metadata.size());
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

    void setRangeSupport(
        bool enabled
        )
    {
        m_rangeSupport =
            enabled;
    }

    void setRejectRanges(
        bool enabled
        )
    {
        m_rejectRanges =
            enabled;
    }

    [[nodiscard]] QByteArray lastRequest() const
    {
        return m_lastRequest;
    }

    [[nodiscard]] int requestCount() const
    {
        return m_requestCount;
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
                const QByteArray request =
                    socket->readAll();
                if (!request.contains(QByteArrayLiteral("\r\n\r\n")))
                {
                    return;
                }

                if (socket->property("responded").toBool())
                {
                    return;
                }
                socket->setProperty("responded", true);
                m_lastRequest =
                    request;
                ++m_requestCount;

                QByteArray responseBody =
                    m_body;
                QByteArray status =
                    QByteArrayLiteral("HTTP/1.1 200 OK\r\n");
                QByteArray rangeHeaders;

                const QRegularExpression rangePattern(
                    QStringLiteral(
                        R"((?:^|\r\n)Range: bytes=(\d+)-\r\n)"
                        ),
                    QRegularExpression::CaseInsensitiveOption
                    );
                const QRegularExpressionMatch rangeMatch =
                    rangePattern.match(
                        QString::fromLatin1(request)
                        );
                if (m_rejectRanges && rangeMatch.hasMatch())
                {
                    responseBody.clear();
                    status =
                        QByteArrayLiteral(
                            "HTTP/1.1 416 Range Not Satisfiable\r\n"
                            );
                    rangeHeaders =
                        QByteArrayLiteral("Content-Range: bytes */")
                        + QByteArray::number(m_body.size())
                        + QByteArrayLiteral("\r\n");
                }
                else if (
                    m_rangeSupport
                    && rangeMatch.hasMatch()
                    )
                {
                    const qint64 offset =
                        rangeMatch.captured(1).toLongLong();
                    responseBody =
                        m_body.mid(offset);
                    status =
                        QByteArrayLiteral(
                            "HTTP/1.1 206 Partial Content\r\n"
                            );
                    rangeHeaders =
                        QByteArrayLiteral("Content-Range: bytes ")
                        + QByteArray::number(offset)
                        + QByteArrayLiteral("-")
                        + QByteArray::number(m_body.size() - 1)
                        + QByteArrayLiteral("/")
                        + QByteArray::number(m_body.size())
                        + QByteArrayLiteral("\r\nETag: \"test-etag\"\r\n");
                }

                const QByteArray headers =
                    status
                    + QByteArrayLiteral("Content-Type: application/json\r\n")
                    + rangeHeaders
                    + QByteArrayLiteral("Content-Length: ")
                    + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");

                socket->write(headers);
                socket->write(responseBody);
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
    QByteArray m_lastRequest;
    int m_requestCount = 0;
    bool m_rangeSupport = false;
    bool m_rejectRanges = false;
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
    void dialogShowsSkipAndUnskipActions();
    void dialogUsesConsistentProgramStatusLayout();
    void signatureVerifierAcceptsValidSignature();
    void signatureVerifierRejectsChangedPayload();
    void downloaderRejectsChecksumMismatch();
    void downloaderRejectsOversizedPayload();
    void downloaderCanPauseAndRetainsPartialFile();
    void downloaderResumesWithValidatedRange();
    void downloaderRestartsWhenRangeIsIgnored();
    void downloaderRestartsAfterRejectedRange();
    void downloaderReusesVerifiedCompletedPackage();
    void cleanupKeepsCurrentAndRemovesObsoleteUpdaterFiles();
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

void UpdaterTests::dialogShowsSkipAndUnskipActions()
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
    QVERIFY(service.checkForUpdates());
    QVERIFY(succeededSpy.wait(5000));

    UpdateDialog availableDialog(
        &service,
        true
        );
    availableDialog.refreshForOpen();
    QPushButton* skipButton =
        availableDialog.findChild<QPushButton*>(
            QStringLiteral("updateSecondaryButton")
            );
    QVERIFY(skipButton);
    QCOMPARE(
        skipButton->text(),
        QStringLiteral("Skip This Version")
        );

    QSignalSpy skipSpy(
        &availableDialog,
        &UpdateDialog::skipVersionRequested
        );
    skipButton->click();
    QCOMPARE(skipSpy.count(), 1);
    QCOMPARE(
        skipSpy.takeFirst().first().toString(),
        QStringLiteral("9.9.9")
        );

    UpdateDialog skippedDialog(
        &service,
        true,
        nullptr,
        QStringLiteral("9.9.9")
        );
    skippedDialog.refreshForOpen();
    QPushButton* unskipButton =
        skippedDialog.findChild<QPushButton*>(
            QStringLiteral("updateSecondaryButton")
            );
    QVERIFY(unskipButton);
    QCOMPARE(
        unskipButton->text(),
        QStringLiteral("Notify Me About This Version")
        );
}

void UpdaterTests::dialogUsesConsistentProgramStatusLayout()
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

    const auto verifyStatusHeader =
        [](UpdateDialog& dialog)
        {
            dialog.resize(640, dialog.sizeHint().height());
            dialog.show();
            QCoreApplication::processEvents();

            QLabel* indicator =
                dialog.findChild<QLabel*>(
                    QStringLiteral("programUpdateIndicator")
                    );
            QLabel* title =
                dialog.findChild<QLabel*>(
                    QStringLiteral("programUpdateTitle")
                    );
            QLabel* details =
                dialog.findChild<QLabel*>(
                    QStringLiteral("programUpdateDetails")
                    );

            QVERIFY(indicator);
            QVERIFY(title);
            QVERIFY(details);
            QCOMPARE(title->x(), details->x());
            QVERIFY(indicator->geometry().right() < title->geometry().left());
            QVERIFY(indicator->geometry().center().y() >= title->geometry().top());
            QVERIFY(indicator->geometry().center().y() <= details->geometry().bottom());
        };

    UpdateConfiguration configuration;
    configuration.releasesApiUrl =
        server.url();

    QCoreApplication::setApplicationVersion(
        QStringLiteral("9.9.9")
        );
    UpdateService currentService(configuration);
    QSignalSpy currentSucceededSpy(
        &currentService,
        &UpdateService::checkSucceeded
        );
    QVERIFY(
        currentService.checkForUpdates(
            UpdateService::CheckPolicy::Force
            )
        );
    QVERIFY(currentSucceededSpy.wait(5000));

    UpdateDialog currentDialog(
        &currentService,
        true
        );
    currentDialog.refreshForOpen();
    verifyStatusHeader(currentDialog);

    QLabel* currentTitle =
        currentDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateTitle")
            );
    QLabel* currentNotes =
        currentDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateNotes")
            );
    QProgressBar* currentProgress =
        currentDialog.findChild<QProgressBar*>(
            QStringLiteral("programUpdateProgress")
            );
    QLabel* currentProgressText =
        currentDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateProgressText")
            );
    QVERIFY(
        !currentDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateMetadata")
            )
        );
    QVERIFY(currentTitle);
    QVERIFY(currentNotes);
    QVERIFY(currentProgress);
    QVERIFY(currentProgressText);
    QVERIFY(!currentNotes->isHidden());
    QCOMPARE(currentNotes->x(), currentTitle->x());
    QVERIFY(currentProgress->isHidden());
    QVERIFY(currentProgressText->isHidden());

    QCoreApplication::setApplicationVersion(
        QStringLiteral("1.0.0")
        );
    UpdateService availableService(configuration);
    QSignalSpy availableSucceededSpy(
        &availableService,
        &UpdateService::checkSucceeded
        );
    QVERIFY(
        availableService.checkForUpdates(
            UpdateService::CheckPolicy::Force
            )
        );
    QVERIFY(availableSucceededSpy.wait(5000));

    UpdateDialog availableDialog(
        &availableService,
        true
        );
    availableDialog.refreshForOpen();
    verifyStatusHeader(availableDialog);

    QLabel* availableTitle =
        availableDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateTitle")
            );
    QLabel* availableDetails =
        availableDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateDetails")
            );
    QLabel* availableNotes =
        availableDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateNotes")
            );
    QProgressBar* availableProgress =
        availableDialog.findChild<QProgressBar*>(
            QStringLiteral("programUpdateProgress")
            );
    QLabel* availableProgressText =
        availableDialog.findChild<QLabel*>(
            QStringLiteral("programUpdateProgressText")
            );
    QVERIFY(availableTitle);
    QVERIFY(availableDetails);
    QVERIFY(availableNotes);
    QVERIFY(availableProgress);
    QVERIFY(availableProgressText);
    QVERIFY(
        availableDetails->text().contains(
            QStringLiteral("Installed Version: 1.0.0")
            )
        );
    QVERIFY(
        !availableDetails->text().contains(
            QStringLiteral("You are currently using")
            )
        );
    QCOMPARE(availableNotes->x(), availableTitle->x());
    QVERIFY(availableProgress->isHidden());
    QVERIFY(availableProgressText->isHidden());

    QVERIFY(
        QMetaObject::invokeMethod(
            &availableDialog,
            "handleDownloadStarted",
            Qt::DirectConnection
            )
        );
    QCoreApplication::processEvents();
    verifyStatusHeader(availableDialog);

    QCOMPARE(
        availableTitle->text(),
        QStringLiteral("Downloading Update")
        );
    QVERIFY(!availableProgress->isHidden());
    QVERIFY(!availableProgressText->isHidden());
    QCOMPARE(availableProgress->x(), availableTitle->x());
    QCOMPARE(availableProgressText->x(), availableTitle->x());
    QCOMPARE(
        availableDetails->text(),
        QStringLiteral("The update is being downloaded.")
        );
    QCOMPARE(
        availableProgressText->text(),
        QStringLiteral("%1 of %2")
            .arg(
                QLocale::system().formattedDataSize(0),
                QLocale::system().formattedDataSize(123)
                )
        );

    QVERIFY(
        QMetaObject::invokeMethod(
            &availableDialog,
            "handleDownloadProgress",
            Qt::DirectConnection,
            Q_ARG(qint64, 61),
            Q_ARG(qint64, 123)
            )
        );
    QCOMPARE(
        availableProgressText->text(),
        QStringLiteral("%1 of %2")
            .arg(
                QLocale::system().formattedDataSize(61),
                QLocale::system().formattedDataSize(123)
                )
        );

    QVERIFY(
        QMetaObject::invokeMethod(
            &availableDialog,
            "handleDownloadSucceeded",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("C:/temp/ClassMngr-update.exe"))
            )
        );
    QVERIFY(availableProgressText->isHidden());
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

void UpdaterTests::downloaderCanPauseAndRetainsPartialFile()
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
    QSignalSpy pausedSpy(
        &downloader,
        &UpdateDownloader::downloadPaused
        );

    downloader.download(artifact);
    downloader.pause();

    QCOMPARE(pausedSpy.count(), 1);

    const QString partialPath =
        QDir(
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation
                )
            ).filePath(
                QStringLiteral("ClassMngr/updates/%1.part")
                    .arg(fileName)
                );

    QVERIFY(QFileInfo::exists(partialPath));
    QVERIFY(
        QFileInfo::exists(
            partialPath.chopped(
                QStringLiteral(".part").size()
                )
            + QStringLiteral(".download.json")
            )
        );

    downloader.discard();
    QVERIFY(!QFileInfo::exists(partialPath));
}

void UpdaterTests::downloaderResumesWithValidatedRange()
{
    const QByteArray body =
        QByteArrayLiteral("0123456789abcdef");
    HttpServer server(body);
    server.setRangeSupport(true);
    QVERIFY(server.listen());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        QStringLiteral("ClassMngr-resume-test.bin");
    artifact.sha256 =
        sha256Hex(body);
    artifact.sizeBytes =
        body.size();

    writeDownloadRecord(
        directory.path(),
        artifact,
        body.first(5)
        );

    UpdateDownloader downloader(
        nullptr,
        directory.path()
        );
    QSignalSpy succeededSpy(
        &downloader,
        &UpdateDownloader::downloadSucceeded
        );

    downloader.download(artifact);
    QVERIFY(succeededSpy.wait(5000));

    QVERIFY(
        server.lastRequest().contains(
            QByteArrayLiteral("Range: bytes=5-")
            )
        );
    QVERIFY(
        server.lastRequest().contains(
            QByteArrayLiteral("If-Range: \"test-etag\"")
            )
        );

    QFile completed(
        QDir(directory.path()).filePath(
            artifact.fileName
            )
        );
    QVERIFY(completed.open(QIODevice::ReadOnly));
    QCOMPARE(completed.readAll(), body);
}

void UpdaterTests::downloaderRestartsWhenRangeIsIgnored()
{
    const QByteArray body =
        QByteArrayLiteral("server-sends-the-whole-file");
    HttpServer server(body);
    QVERIFY(server.listen());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        QStringLiteral("ClassMngr-range-ignored.bin");
    artifact.sha256 =
        sha256Hex(body);
    artifact.sizeBytes =
        body.size();

    writeDownloadRecord(
        directory.path(),
        artifact,
        body.first(4)
        );

    UpdateDownloader downloader(
        nullptr,
        directory.path()
        );
    QSignalSpy succeededSpy(
        &downloader,
        &UpdateDownloader::downloadSucceeded
        );

    downloader.download(artifact);
    QVERIFY(succeededSpy.wait(5000));
    QVERIFY(
        server.lastRequest().contains(
            QByteArrayLiteral("Range: bytes=4-")
            )
        );

    QFile completed(
        QDir(directory.path()).filePath(
            artifact.fileName
            )
        );
    QVERIFY(completed.open(QIODevice::ReadOnly));
    QCOMPARE(completed.readAll(), body);
}

void UpdaterTests::downloaderRestartsAfterRejectedRange()
{
    const QByteArray body =
        QByteArrayLiteral("range-retry-body");
    HttpServer server(body);
    server.setRejectRanges(true);
    QVERIFY(server.listen());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        QStringLiteral("ClassMngr-range-retry.bin");
    artifact.sha256 =
        sha256Hex(body);
    artifact.sizeBytes =
        body.size();
    writeDownloadRecord(
        directory.path(),
        artifact,
        body.first(3)
        );

    UpdateDownloader downloader(
        nullptr,
        directory.path()
        );
    QSignalSpy succeededSpy(
        &downloader,
        &UpdateDownloader::downloadSucceeded
        );
    downloader.download(artifact);
    QVERIFY(succeededSpy.wait(5000));
    QCOMPARE(server.requestCount(), 2);
}

void UpdaterTests::downloaderReusesVerifiedCompletedPackage()
{
    const QByteArray body =
        QByteArrayLiteral("already verified package");
    HttpServer server(body);
    QVERIFY(server.listen());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url(QStringLiteral("/update.bin"));
    artifact.fileName =
        QStringLiteral("ClassMngr-completed-test.bin");
    artifact.sha256 =
        sha256Hex(body);
    artifact.sizeBytes =
        body.size();

    writeDownloadRecord(
        directory.path(),
        artifact,
        body,
        QStringLiteral("completed")
        );

    UpdateDownloader downloader(
        nullptr,
        directory.path()
        );
    QVERIFY(
        downloader.hasCompletedDownload(
            artifact
            )
        );

    QSignalSpy succeededSpy(
        &downloader,
        &UpdateDownloader::downloadSucceeded
        );
    downloader.download(artifact);
    QVERIFY(succeededSpy.wait(5000));
    QCOMPARE(server.requestCount(), 0);
}

void UpdaterTests::cleanupKeepsCurrentAndRemovesObsoleteUpdaterFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    UpdateArtifact current;
    current.platformKey =
        QStringLiteral("windows-x64");
    current.url =
        QUrl(QStringLiteral("https://example.com/current.bin"));
    current.fileName =
        QStringLiteral("ClassMngr-current.bin");
    current.sha256 =
        sha256Hex(QByteArrayLiteral("current"));
    current.sizeBytes =
        7;

    UpdateArtifact old =
        current;
    old.url =
        QUrl(QStringLiteral("https://example.com/old.bin"));
    old.fileName =
        QStringLiteral("ClassMngr-old.bin");
    old.sha256 =
        sha256Hex(QByteArrayLiteral("old"));
    old.sizeBytes =
        3;

    writeDownloadRecord(
        directory.path(),
        current,
        QByteArrayLiteral("current"),
        QStringLiteral("completed")
        );
    writeDownloadRecord(
        directory.path(),
        old,
        QByteArrayLiteral("old"),
        QStringLiteral("completed")
        );

    const QDateTime now =
        QDateTime::fromString(
            QStringLiteral("2026-08-06T00:00:00Z"),
            Qt::ISODate
            );
    const QDateTime oldTimestamp =
        now.addDays(-30).addSecs(-1);

    QFile orphan(
        QDir(directory.path()).filePath(
            QStringLiteral("ClassMngr-orphan.bin.part")
            )
        );
    QVERIFY(orphan.open(QIODevice::WriteOnly));
    orphan.write("orphan");
    orphan.close();
    QVERIFY(orphan.open(QIODevice::ReadWrite));
    QVERIFY(
        orphan.setFileTime(
            oldTimestamp,
            QFileDevice::FileModificationTime
            )
        );
    orphan.close();

    QFile exactBoundary(
        QDir(directory.path()).filePath(
            QStringLiteral("ClassMngr-exact-boundary.bin.part")
            )
        );
    QVERIFY(exactBoundary.open(QIODevice::WriteOnly));
    exactBoundary.write("keep");
    exactBoundary.close();
    QVERIFY(exactBoundary.open(QIODevice::ReadWrite));
    QVERIFY(
        exactBoundary.setFileTime(
            now.addDays(-30),
            QFileDevice::FileModificationTime
            )
        );
    exactBoundary.close();

    QFile freshOrphan(
        QDir(directory.path()).filePath(
            QStringLiteral("ClassMngr-fresh-orphan.bin.part")
            )
        );
    QVERIFY(freshOrphan.open(QIODevice::WriteOnly));
    freshOrphan.write("keep");
    freshOrphan.close();
    QVERIFY(freshOrphan.open(QIODevice::ReadWrite));
    QVERIFY(
        freshOrphan.setFileTime(
            now.addDays(-30).addSecs(1),
            QFileDevice::FileModificationTime
            )
        );
    freshOrphan.close();

    QFile unrelated(
        QDir(directory.path()).filePath(
            QStringLiteral("notes.txt")
            )
        );
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.write("keep");
    unrelated.close();
    QVERIFY(unrelated.open(QIODevice::ReadWrite));
    QVERIFY(
        unrelated.setFileTime(
            oldTimestamp,
            QFileDevice::FileModificationTime
            )
        );
    unrelated.close();

    UpdateDownloader::cleanupDownloads(
        UpdateDownloader::CleanupMode::KeepOnlyArtifact,
        current,
        directory.path(),
        now
        );

    QVERIFY(
        QFileInfo::exists(
            QDir(directory.path()).filePath(
                current.fileName
                )
            )
        );
    QVERIFY(
        !QFileInfo::exists(
            QDir(directory.path()).filePath(
                old.fileName
                )
            )
        );
    QVERIFY(
        !QFileInfo::exists(
            orphan.fileName()
            )
        );
    QVERIFY(QFileInfo::exists(exactBoundary.fileName()));
    QVERIFY(QFileInfo::exists(freshOrphan.fileName()));
    QVERIFY(
        QFileInfo::exists(
            unrelated.fileName()
            )
        );
}

QTEST_MAIN(UpdaterTests)

#include "updater_tests.moc"
