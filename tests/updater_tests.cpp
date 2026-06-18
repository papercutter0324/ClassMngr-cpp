#include "core/updater/update_downloader.h"
#include "core/updater/update_manifest.h"
#include "core/updater/version.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

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

QByteArray manifestJson(
    const QString& version = QStringLiteral("0.1.1"),
    const QString& platformKey = QStringLiteral("windows-x64"),
    const QString& url = QStringLiteral("https://example.com/ClassMngr.exe"),
    const QString& sha256 = QString(64, QLatin1Char('a')),
    qint64 sizeBytes = 123
    )
{
    QJsonObject artifact;
    artifact.insert(
        QStringLiteral("url"),
        url
        );
    artifact.insert(
        QStringLiteral("fileName"),
        QStringLiteral("ClassMngr.exe")
        );
    artifact.insert(
        QStringLiteral("sha256"),
        sha256
        );
    artifact.insert(
        QStringLiteral("sizeBytes"),
        sizeBytes
        );

    QJsonObject platforms;
    platforms.insert(
        platformKey,
        artifact
        );

    QJsonObject manifest;
    manifest.insert(
        QStringLiteral("schemaVersion"),
        1
        );
    manifest.insert(
        QStringLiteral("channel"),
        QStringLiteral("stable")
        );
    manifest.insert(
        QStringLiteral("latestVersion"),
        version
        );
    manifest.insert(
        QStringLiteral("minimumSupportedVersion"),
        QStringLiteral("0.1.0")
        );
    manifest.insert(
        QStringLiteral("releaseDate"),
        QStringLiteral("2026-06-18")
        );
    manifest.insert(
        QStringLiteral("notesUrl"),
        QStringLiteral("https://example.com/notes")
        );
    manifest.insert(
        QStringLiteral("platforms"),
        platforms
        );

    return QJsonDocument(manifest).toJson(
        QJsonDocument::Compact
        );
}

class OneShotHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit OneShotHttpServer(
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
            &OneShotHttpServer::respond
            );
    }

    bool listen()
    {
        return m_server.listen(
            QHostAddress(QStringLiteral("127.0.0.1")),
            0
            );
    }

    QUrl url() const
    {
        return QUrl(
            QStringLiteral("http://127.0.0.1:%1/update.bin")
                .arg(m_server.serverPort())
            );
    }

    QString errorString() const
    {
        return m_server.errorString();
    }

private slots:
    void respond()
    {
        auto* socket =
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
                    QByteArray("HTTP/1.1 200 OK\r\n")
                    + "Content-Type: application/octet-stream\r\n"
                    + "Content-Length: "
                    + QByteArray::number(m_body.size())
                    + "\r\nConnection: close\r\n\r\n";

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
    void manifestRejectsInvalidFields();
    void manifestRequiresCurrentPlatform();
    void manifestSelectsPlatformArtifact();
    void downloaderRejectsChecksumMismatch();
};

void UpdaterTests::versionRejectsNonStrictFormat()
{
    QVERIFY(!Version::parse(QStringLiteral("1.2")).has_value());
    QVERIFY(!Version::parse(QStringLiteral("1.2.3.4")).has_value());
    QVERIFY(!Version::parse(QStringLiteral("v1.2.3")).has_value());
}

void UpdaterTests::versionComparesNumerically()
{
    const auto lower =
        Version::parse(
            QStringLiteral("1.2.9")
            );
    const auto higher =
        Version::parse(
            QStringLiteral("1.10.0")
            );

    QVERIFY(lower.has_value());
    QVERIFY(higher.has_value());
    QVERIFY(*higher > *lower);
}

void UpdaterTests::manifestRejectsInvalidFields()
{
    QVERIFY(
        !UpdateManifest::fromJson(
            manifestJson(QStringLiteral("1.2")),
            {QStringLiteral("windows-x64")}
            ).has_value()
        );

    QVERIFY(
        !UpdateManifest::fromJson(
            manifestJson(
                QStringLiteral("0.1.1"),
                QStringLiteral("windows-x64"),
                QStringLiteral("http://example.com/update.exe")
                ),
            {QStringLiteral("windows-x64")}
            ).has_value()
        );

    QVERIFY(
        !UpdateManifest::fromJson(
            manifestJson(
                QStringLiteral("0.1.1"),
                QStringLiteral("windows-x64"),
                QStringLiteral("https://example.com/update.exe"),
                QStringLiteral("abc")
                ),
            {QStringLiteral("windows-x64")}
            ).has_value()
        );

    QJsonObject root =
        QJsonDocument::fromJson(
            manifestJson()
            ).object();
    root.insert(
        QStringLiteral("schemaVersion"),
        2
        );

    QVERIFY(
        !UpdateManifest::fromJson(
            QJsonDocument(root).toJson(QJsonDocument::Compact),
            {QStringLiteral("windows-x64")}
            ).has_value()
        );
}

void UpdaterTests::manifestRequiresCurrentPlatform()
{
    const auto manifest =
        UpdateManifest::fromJson(
            manifestJson(
                QStringLiteral("0.1.1"),
                QStringLiteral("macos-universal")
                ),
            {QStringLiteral("windows-x64")}
            );

    QVERIFY(!manifest.has_value());
}

void UpdaterTests::manifestSelectsPlatformArtifact()
{
    const auto manifest =
        UpdateManifest::fromJson(
            manifestJson(),
            {QStringLiteral("windows-x64")}
            );

    QVERIFY(manifest.has_value());

    const auto artifact =
        manifest->artifactForAny(
            {QStringLiteral("windows-x64")}
            );

    QVERIFY(artifact.has_value());
    QCOMPARE(
        artifact->platformKey,
        QStringLiteral("windows-x64")
        );
}

void UpdaterTests::downloaderRejectsChecksumMismatch()
{
    const QByteArray body =
        QByteArrayLiteral("downloaded bytes");

    OneShotHttpServer server(body);

    if (!server.listen())
    {
        QSKIP(
            qPrintable(
                QStringLiteral("Local TCP server unavailable: %1")
                    .arg(server.errorString())
                )
            );
    }

    UpdateArtifact artifact;
    artifact.platformKey =
        QStringLiteral("windows-x64");
    artifact.url =
        server.url();
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
    QSignalSpy successSpy(
        &downloader,
        &UpdateDownloader::downloadSucceeded
        );

    downloader.download(artifact);

    QVERIFY(failedSpy.wait(5000));
    QCOMPARE(successSpy.count(), 0);
    QVERIFY(
        failedSpy.takeFirst().first().toString().contains(
            QStringLiteral("checksum"),
            Qt::CaseInsensitive
            )
        );
}

QTEST_MAIN(UpdaterTests)

#include "updater_tests.moc"
