#include "app/controllers/update_controller.h"
#include "core/settingsmanager.h"
#include "core/updater/github_release.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

#include <memory>
#include <utility>

namespace
{

QString assetName(const QString& platformKey)
{
    if (platformKey == QStringLiteral("windows-x64"))
    {
        return QStringLiteral("ClassMngr-1.0.0-win-x64.exe");
    }

    if (platformKey == QStringLiteral("windows-arm64"))
    {
        return QStringLiteral("ClassMngr-1.0.0-win-arm64.exe");
    }

    if (platformKey == QStringLiteral("macos-universal"))
    {
        return QStringLiteral("ClassMngr-1.0.0-macos-universal.dmg");
    }

    return QStringLiteral("ClassMngr-1.0.0-linux-x86_64.tar.gz");
}

QByteArray validReleaseResponse()
{
    const QString platformKey = GitHubRelease::currentPlatformKeys().first();
    const QString fileName = assetName(platformKey);

    QJsonObject asset;
    asset.insert(QStringLiteral("name"), fileName);
    asset.insert(QStringLiteral("state"), QStringLiteral("uploaded"));
    asset.insert(QStringLiteral("size"), 123);
    asset.insert(
        QStringLiteral("digest"),
        QStringLiteral(
            "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            )
        );
    asset.insert(
        QStringLiteral("browser_download_url"),
        QStringLiteral("https://github.com/example/releases/download/v1.0.0/%1")
            .arg(fileName)
        );

    QJsonArray assets;
    assets.append(asset);

    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), QStringLiteral("v1.0.0"));
    release.insert(
        QStringLiteral("html_url"),
        QStringLiteral("https://github.com/example/releases/tag/v1.0.0")
        );
    release.insert(
        QStringLiteral("published_at"),
        QStringLiteral("2026-08-05T14:49:05Z")
        );
    release.insert(QStringLiteral("draft"), false);
    release.insert(QStringLiteral("prerelease"), false);
    release.insert(QStringLiteral("assets"), assets);

    QJsonArray releases;
    releases.append(release);
    return QJsonDocument(releases).toJson(QJsonDocument::Compact);
}

class LocalHttpServer final : public QObject
{
    Q_OBJECT

public:
    explicit LocalHttpServer(
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
            &LocalHttpServer::acceptConnections
            );
    }

    [[nodiscard]] bool listen()
    {
        return m_server.listen(
            QHostAddress(QStringLiteral("127.0.0.1")),
            0
            );
    }

    [[nodiscard]] QUrl url() const
    {
        return QUrl(
            QStringLiteral("http://127.0.0.1:%1/releases")
                .arg(m_server.serverPort())
            );
    }

    [[nodiscard]] int requestCount() const noexcept
    {
        return m_requestCount;
    }

    void setBody(QByteArray body)
    {
        m_body = std::move(body);
    }

private slots:
    void acceptConnections()
    {
        while (m_server.hasPendingConnections())
        {
            QTcpSocket* const socket = m_server.nextPendingConnection();
            if (!socket)
            {
                continue;
            }

            connect(
                socket,
                &QTcpSocket::readyRead,
                this,
                [this, socket]()
                {
                    QByteArray request =
                        socket->property("requestBytes").toByteArray();
                    request += socket->readAll();
                    socket->setProperty("requestBytes", request);

                    if (
                        socket->property("responded").toBool()
                        || !request.contains(QByteArrayLiteral("\r\n\r\n"))
                        )
                    {
                        return;
                    }

                    socket->setProperty("responded", true);
                    ++m_requestCount;

                    const QByteArray header =
                        QByteArrayLiteral(
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Connection: close\r\n"
                            "Content-Length: "
                            )
                        + QByteArray::number(m_body.size())
                        + QByteArrayLiteral("\r\n\r\n");
                    socket->write(header + m_body);
                    socket->disconnectFromHost();
                }
                );
            connect(
                socket,
                &QTcpSocket::disconnected,
                socket,
                &QObject::deleteLater
                );
        }
    }

private:
    QTcpServer m_server;
    QByteArray m_body;
    int m_requestCount = 0;
};

void setAutomaticChecksEnabled(const bool enabled)
{
    SettingsManager::instance().set(
        QString::fromUtf8(
            SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
            ),
        enabled
        );
    SettingsManager::instance().sync();
}

UpdateConfiguration configurationFor(const QUrl& apiUrl)
{
    UpdateConfiguration configuration;
    configuration.releasesApiUrl = apiUrl;
    configuration.checkOnStartup = true;
    return configuration;
}

bool writeStaleUpdaterOrphan(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    if (file.write(QByteArrayLiteral("stale updater artifact")) < 0)
    {
        return false;
    }
    const bool timestampSet = file.setFileTime(
        QDateTime::currentDateTimeUtc().addDays(-31),
        QFileDevice::FileModificationTime
        );
    file.close();
    return timestampSet;
}

} // namespace

class UpdateControllerAutomaticStartupTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void startupWaitsForCompletionAndForcesOnlyOnce();
    void disabledPreferenceAttemptCanBeRetried();
    void unconfiguredStartupDoesNotDispatch();

private:
    std::unique_ptr<QTemporaryDir> m_settingsRoot;
};

void UpdateControllerAutomaticStartupTests::initTestCase()
{
    QString tempRoot = qEnvironmentVariable("TMP");
    if (tempRoot.trimmed().isEmpty())
    {
        tempRoot = QDir::tempPath();
    }
    QVERIFY(QDir().mkpath(tempRoot));

    m_settingsRoot = std::make_unique<QTemporaryDir>(
        QDir(tempRoot).filePath(
            QStringLiteral("ClassMngrUpdateController-XXXXXX")
            )
        );
    QVERIFY(m_settingsRoot->isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsRoot->path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
    QCoreApplication::setApplicationVersion(QStringLiteral("99.0.0"));
}

void UpdateControllerAutomaticStartupTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void UpdateControllerAutomaticStartupTests::
startupWaitsForCompletionAndForcesOnlyOnce()
{
    LocalHttpServer server(validReleaseResponse());
    QVERIFY(server.listen());

    UpdateService service(configurationFor(server.url()));
    QSignalSpy startedSpy(&service, &UpdateService::checkStarted);
    QSignalSpy succeededSpy(&service, &UpdateService::checkSucceeded);

    // Give the service a fresh successful result. The controller must still
    // dispatch a forced startup check when its startup-complete transition
    // arrives.
    QVERIFY(
        service.checkForUpdates(UpdateService::CheckPolicy::Force)
        );
    QTRY_COMPARE(server.requestCount(), 1);
    QTRY_COMPARE(succeededSpy.count(), 1);

    server.setBody(QByteArrayLiteral("{not json"));
    setAutomaticChecksEnabled(true);
    UpdateController controller(&service);
    QSignalSpy failedSpy(&service, &UpdateService::checkFailed);

    controller.startAutomaticCheck();
    QCOMPARE(startedSpy.count(), 1);

    controller.setStartupComplete();
    controller.startAutomaticCheck();
    QCOMPARE(startedSpy.count(), 2);
    QTRY_COMPARE(server.requestCount(), 2);
    QTRY_COMPARE(failedSpy.count(), 1);

    controller.startAutomaticCheck();
    QCOMPARE(startedSpy.count(), 2);
}

void UpdateControllerAutomaticStartupTests::
disabledPreferenceAttemptCanBeRetried()
{
    LocalHttpServer server(QByteArrayLiteral("{not json"));
    QVERIFY(server.listen());

    UpdateService service(configurationFor(server.url()));
    QSignalSpy startedSpy(&service, &UpdateService::checkStarted);
    QSignalSpy failedSpy(&service, &UpdateService::checkFailed);
    UpdateController controller(&service);
    controller.setStartupComplete();

    setAutomaticChecksEnabled(false);
    controller.startAutomaticCheck();
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(server.requestCount(), 0);

    setAutomaticChecksEnabled(true);
    controller.startAutomaticCheck();
    QCOMPARE(startedSpy.count(), 1);
    QTRY_COMPARE(server.requestCount(), 1);
    QTRY_COMPARE(failedSpy.count(), 1);

    controller.startAutomaticCheck();
    QCOMPARE(startedSpy.count(), 1);
}

void UpdateControllerAutomaticStartupTests::
unconfiguredStartupDoesNotDispatch()
{
    setAutomaticChecksEnabled(true);

    const QString updateDirectory = QDir(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        ).filePath(QStringLiteral("ClassMngr/updates"));
    QVERIFY(QDir().mkpath(updateDirectory));

    const QString firstOrphanPath = QDir(updateDirectory).filePath(
        QStringLiteral("ClassMngr-f215-startup-maintenance-first.exe")
        );
    const QString secondOrphanPath = QDir(updateDirectory).filePath(
        QStringLiteral("ClassMngr-f215-startup-maintenance-second.exe")
        );
    QVERIFY(writeStaleUpdaterOrphan(firstOrphanPath));

    UpdateService service(configurationFor(QUrl()));
    QSignalSpy startedSpy(&service, &UpdateService::checkStarted);
    QSignalSpy failedSpy(&service, &UpdateService::checkFailed);
    UpdateController controller(&service);

    controller.startAutomaticCheck();
    QVERIFY(QFileInfo::exists(firstOrphanPath));

    controller.setStartupComplete();
    controller.startAutomaticCheck();
    QVERIFY(!QFileInfo::exists(firstOrphanPath));

    QVERIFY(writeStaleUpdaterOrphan(secondOrphanPath));
    controller.startAutomaticCheck();
    QVERIFY(QFileInfo::exists(secondOrphanPath));

    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFile::remove(secondOrphanPath));
}

QTEST_MAIN(UpdateControllerAutomaticStartupTests)

#include "update_controller_automatic_startup_tests.moc"
