#include "app/controllers/update_controller.h"
#include "core/settingsmanager.h"
#include "core/updater/github_release.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_service.h"
#include "next/application/startup_update_check_use_case.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"
#include "next/platform/update_service_manual_check_port.h"
#include "next/platform/update_service_startup_check_port.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPushButton>
#include <QVariant>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using ClassMngr::Next::Application::SkippedUpdateVersionText;

namespace
{

SkippedUpdateVersionText skippedVersionText(std::string value)
{
    return SkippedUpdateVersionText(std::move(value));
}

QString assetName(
    const QString& platformKey,
    const QString& versionText
    )
{
    if (platformKey == QStringLiteral("windows-x64"))
    {
        return QStringLiteral("ClassMngr-%1-win-x64.exe").arg(versionText);
    }

    if (platformKey == QStringLiteral("windows-arm64"))
    {
        return QStringLiteral("ClassMngr-%1-win-arm64.exe").arg(versionText);
    }

    if (platformKey == QStringLiteral("macos-universal"))
    {
        return QStringLiteral("ClassMngr-%1-macos-universal.dmg")
            .arg(versionText);
    }

    return QStringLiteral("ClassMngr-%1-linux-x86_64.tar.gz")
        .arg(versionText);
}

QByteArray validReleaseResponse(
    const QString& versionText = QStringLiteral("1.0.0"),
    const QString& requestedPlatformKey = QString()
    )
{
    const QString platformKey = requestedPlatformKey.isEmpty()
        ? GitHubRelease::currentPlatformKeys().first()
        : requestedPlatformKey;
    const QString fileName = assetName(platformKey, versionText);

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
        QStringLiteral("https://github.com/example/releases/download/v%1/%2")
            .arg(versionText, fileName)
        );

    QJsonArray assets;
    assets.append(asset);

    QJsonObject release;
    release.insert(
        QStringLiteral("tag_name"),
        QStringLiteral("v%1").arg(versionText)
        );
    release.insert(
        QStringLiteral("html_url"),
        QStringLiteral("https://github.com/example/releases/tag/v%1")
            .arg(versionText)
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

[[nodiscard]] bool setSkippedUpdateVersion(const std::string& version)
{
    const ClassMngr::Next::Platform::
        SettingsManagerSkippedUpdateVersionPort
        skippedUpdateVersionPort;
    const auto result = skippedUpdateVersionPort.write({
        .skippedVersion = skippedVersionText(version)
    });
    return static_cast<bool>(result);
}

UpdateDialog* automaticUpdateDialog()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        auto* const dialog = qobject_cast<UpdateDialog*>(widget);
        if (
            dialog
            && dialog->isVisible()
            && dialog->property("automaticUpdatePrompt").toBool()
            )
        {
            return dialog;
        }
    }

    return nullptr;
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
    if (!file.flush())
    {
        return false;
    }
    const QDateTime staleModificationTime =
        QDateTime::currentDateTimeUtc().addDays(-31);
    const bool timestampSet = file.setFileTime(
        staleModificationTime,
        QFileDevice::FileModificationTime
        );
    file.close();
    const QDateTime persistedModificationTime =
        QFileInfo(path).lastModified().toUTC();
    return timestampSet
        && persistedModificationTime.isValid()
        && persistedModificationTime <= staleModificationTime;
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
    void staleSkippedVersionClearsAndSynchronizesOpenDialog();
    void exactSkippedVersionIsKeptAndSuppressesPrompt();
    void failedUserUnskipRetainsStateAndAllowsRetry();
    void failedStaleCleanupRetainsValueAndDisplayAndAllowsPrompt();
    void failedAutomaticSkipKeepsDialogOpenAndAllowsRetry();
    void bridgeMapsSuccessAndForcesFreshCheck_data();
    void bridgeMapsSuccessAndForcesFreshCheck();
    void parserMatchesPlatformKeysExactlyAndKeepsLinuxAliases();
    void bridgeMapsFailures_data();
    void bridgeMapsFailures();
    void bridgeAndControllerDestructionSuppressDelivery();
    void manualAdapterDistinguishesFreshBusyForceAndFailure();
    void manualDialogObservesConcurrentStartupResult();
    void startupSuccessEffectsOccurOnceAndManualChecksStillWork_data();
    void startupSuccessEffectsOccurOnceAndManualChecksStillWork();

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

void UpdateControllerAutomaticStartupTests::
staleSkippedVersionClearsAndSynchronizesOpenDialog()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    setAutomaticChecksEnabled(true);

    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    UpdateController controller(&service);
    QSignalSpy succeededSpy(&service, &UpdateService::checkSucceeded);

    // The stale value is cleared before prompt eligibility is evaluated, so
    // the available update opens the automatic prompt.
    QVERIFY(setSkippedUpdateVersion("1.5.0"));
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 1);
    QCOMPARE(server.requestCount(), 1);

    const ClassMngr::Next::Platform::
        SettingsManagerSkippedUpdateVersionPort
        skippedUpdateVersionPort;
    QVERIFY(!skippedUpdateVersionPort.read().skippedVersion.has_value());

    UpdateDialog* const dialog = automaticUpdateDialog();
    QVERIFY(dialog);
    QVERIFY(controller.hasVisibleDialog());
    auto* const secondaryButton =
        dialog->findChild<QPushButton*>(QStringLiteral("updateSecondaryButton"));
    QVERIFY(secondaryButton);
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));

    // Put the open dialog in its skipped state, then persist a stale value
    // again. The next result must clear both storage and the displayed state.
    dialog->setSkippedVersion(QStringLiteral("2.0.0"));
    QCOMPARE(
        secondaryButton->text(),
        QStringLiteral("Notify Me About This Version")
        );
    QVERIFY(setSkippedUpdateVersion("1.5.0"));

    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 2);

    QVERIFY(!skippedUpdateVersionPort.read().skippedVersion.has_value());
    QVERIFY(controller.hasVisibleDialog());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));

    dialog->setProperty("automaticUpdatePrompt", false);
    dialog->close();
}

void UpdateControllerAutomaticStartupTests::
exactSkippedVersionIsKeptAndSuppressesPrompt()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    setAutomaticChecksEnabled(true);
    QVERIFY(setSkippedUpdateVersion("2.0.0"));

    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    UpdateController controller(&service);
    QSignalSpy succeededSpy(&service, &UpdateService::checkSucceeded);

    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 1);
    QCOMPARE(server.requestCount(), 1);

    const ClassMngr::Next::Platform::
        SettingsManagerSkippedUpdateVersionPort
        skippedUpdateVersionPort;
    QCOMPARE(
        skippedUpdateVersionPort.read().skippedVersion,
        std::optional<SkippedUpdateVersionText>(
            skippedVersionText("2.0.0")
            )
        );
    QVERIFY(!controller.hasVisibleDialog());
}

void UpdateControllerAutomaticStartupTests::
failedUserUnskipRetainsStateAndAllowsRetry()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    setAutomaticChecksEnabled(true);

    const QString skippedVersionKey = QString::fromUtf8(
        SettingsManager::Keys::SKIPPED_UPDATE_VERSION
        );
    SettingsManager& settings = SettingsManager::instance();
    settings.set(skippedVersionKey, QStringLiteral("02.0.0"));
    settings.sync();

    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    QSignalSpy succeededSpy(&service, &UpdateService::checkSucceeded);

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    auto skippedUpdateVersionPreferences = std::make_shared<
        ClassMngr::Next::Platform::
            SettingsManagerSkippedUpdateVersionPort
        >(
            [&injectAccessError, &synchronizationAttempts](QSettings& settings)
            {
                ++synchronizationAttempts;
                if (injectAccessError)
                {
                    return QSettings::AccessError;
                }

                settings.sync();
                return settings.status();
            }
            );
    UpdateController controller(
        &service,
        nullptr,
        skippedUpdateVersionPreferences
        );

    // The stored text is parseable and numerically equal to the latest
    // version, but its noncanonical spelling does not match literally. This
    // keeps the automatic prompt eligible while retaining a stored value for
    // the Unskip failure path below.
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 1);
    QTRY_VERIFY(controller.hasVisibleDialog());
    QCOMPARE(server.requestCount(), 1);

    UpdateDialog* const dialog = automaticUpdateDialog();
    QVERIFY(dialog);
    auto* const secondaryButton = dialog->findChild<QPushButton*>(
        QStringLiteral("updateSecondaryButton")
        );
    auto* const detailsLabel = dialog->findChild<QLabel*>(
        QStringLiteral("programUpdateDetails")
        );
    QVERIFY(secondaryButton);
    QVERIFY(detailsLabel);

    // Closing an automatic prompt sets suppression. Keep the widget alive and
    // reopen it to exercise user Unskip while suppression is already active.
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    dialog->setSkippedVersion(QStringLiteral("2.0.0"));
    QCOMPARE(
        secondaryButton->text(),
        QStringLiteral("Notify Me About This Version")
        );
    dialog->close();
    QCoreApplication::processEvents();
    QVERIFY(!controller.hasVisibleDialog());
    dialog->show();
    QCoreApplication::processEvents();
    QVERIFY(controller.hasVisibleDialog());

    secondaryButton->click();

    QVERIFY(dialog->isVisible());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Notify Me About This Version"));
    QCOMPARE(settings.get(skippedVersionKey), QVariant(QStringLiteral("02.0.0")));
    QCOMPARE(
        skippedUpdateVersionPreferences->read().skippedVersion,
        std::optional<SkippedUpdateVersionText>(
            skippedVersionText("02.0.0")
            )
        );
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(
        detailsLabel->text().contains(
            QStringLiteral("The skipped update version could not be cleared:"),
            Qt::CaseInsensitive
            )
        );
    QCOMPARE(
        detailsLabel->text().count(
            QStringLiteral("The skipped update version could not be cleared:")
            ),
        1
        );
    QCOMPARE(detailsLabel->text().count(QStringLiteral("AccessError")), 1);

    // With the dialog hidden, the exact-text mismatch leaves the update
    // eligible. The still-active suppression from closing the automatic
    // prompt must prevent it from reopening after the failed Unskip.
    dialog->hide();
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 2);
    QCOMPARE(server.requestCount(), 2);
    QVERIFY(!controller.hasVisibleDialog());
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(skippedVersionKey), QVariant(QStringLiteral("02.0.0")));

    dialog->show();
    QCoreApplication::processEvents();
    QVERIFY(controller.hasVisibleDialog());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Notify Me About This Version"));

    injectAccessError = false;
    secondaryButton->click();

    QVERIFY(controller.hasVisibleDialog());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));
    QVERIFY(!settings.get(skippedVersionKey).isValid());
    QVERIFY(!skippedUpdateVersionPreferences->read().skippedVersion.has_value());
    QCOMPARE(synchronizationAttempts, 2);

    // A successful Unskip resets suppression, so a later eligible release
    // can show the automatic prompt again.
    dialog->hide();
    server.setBody(validReleaseResponse(QStringLiteral("2.1.0")));
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 3);
    QCOMPARE(server.requestCount(), 3);
    QTRY_VERIFY(controller.hasVisibleDialog());
    QVERIFY(dialog->isVisible());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));

    dialog->setProperty("automaticUpdatePrompt", false);
    dialog->close();
}

void UpdateControllerAutomaticStartupTests::
failedStaleCleanupRetainsValueAndDisplayAndAllowsPrompt()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    setAutomaticChecksEnabled(true);

    const QString skippedVersionKey = QString::fromUtf8(
        SettingsManager::Keys::SKIPPED_UPDATE_VERSION
        );
    SettingsManager& settings = SettingsManager::instance();
    settings.set(skippedVersionKey, QStringLiteral("1.5.0"));
    settings.sync();

    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    QSignalSpy succeededSpy(&service, &UpdateService::checkSucceeded);

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    auto skippedUpdateVersionPreferences = std::make_shared<
        ClassMngr::Next::Platform::
            SettingsManagerSkippedUpdateVersionPort
        >(
            [&injectAccessError, &synchronizationAttempts](QSettings& settings)
            {
                ++synchronizationAttempts;
                if (injectAccessError)
                {
                    return QSettings::AccessError;
                }

                settings.sync();
                return settings.status();
            }
            );
    UpdateController controller(
        &service,
        nullptr,
        skippedUpdateVersionPreferences
        );

    // The stale saved value cannot suppress this newer eligible release even
    // when checked removal fails.
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 1);
    QTRY_VERIFY(controller.hasVisibleDialog());
    QCOMPARE(server.requestCount(), 1);
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(skippedVersionKey), QVariant(QStringLiteral("1.5.0")));
    QCOMPARE(
        skippedUpdateVersionPreferences->read().skippedVersion,
        std::optional<SkippedUpdateVersionText>(
            skippedVersionText("1.5.0")
            )
        );

    UpdateDialog* const dialog = automaticUpdateDialog();
    QVERIFY(dialog);
    auto* const secondaryButton = dialog->findChild<QPushButton*>(
        QStringLiteral("updateSecondaryButton")
        );
    QVERIFY(secondaryButton);
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));

    // Make the currently displayed selection observable as an Unskip action.
    // A subsequent failed stale cleanup must leave this dialog state intact.
    dialog->setSkippedVersion(QStringLiteral("2.0.0"));
    QCOMPARE(
        secondaryButton->text(),
        QStringLiteral("Notify Me About This Version")
        );
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 2);
    QCOMPARE(server.requestCount(), 2);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(skippedVersionKey), QVariant(QStringLiteral("1.5.0")));
    QCOMPARE(
        secondaryButton->text(),
        QStringLiteral("Notify Me About This Version")
        );

    injectAccessError = false;
    server.setBody(validReleaseResponse(QStringLiteral("2.1.0")));
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 3);
    QCOMPARE(server.requestCount(), 3);
    QCOMPARE(synchronizationAttempts, 3);
    QVERIFY(!settings.get(skippedVersionKey).isValid());
    QVERIFY(!skippedUpdateVersionPreferences->read().skippedVersion.has_value());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));

    dialog->setProperty("automaticUpdatePrompt", false);
    dialog->close();
}

void UpdateControllerAutomaticStartupTests::
failedAutomaticSkipKeepsDialogOpenAndAllowsRetry()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    setAutomaticChecksEnabled(true);

    const QString skippedVersionKey = QString::fromUtf8(
        SettingsManager::Keys::SKIPPED_UPDATE_VERSION
        );
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(skippedVersionKey);
    settings.sync();

    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    QSignalSpy succeededSpy(&service, &UpdateService::checkSucceeded);

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    auto skippedUpdateVersionPreferences = std::make_shared<
        ClassMngr::Next::Platform::
            SettingsManagerSkippedUpdateVersionPort
        >(
            [&injectAccessError, &synchronizationAttempts](QSettings& settings)
            {
                ++synchronizationAttempts;
                if (injectAccessError)
                {
                    return QSettings::AccessError;
                }

                settings.sync();
                return settings.status();
            }
            );
    UpdateController controller(
        &service,
        nullptr,
        skippedUpdateVersionPreferences
        );

    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 1);
    QTRY_VERIFY(controller.hasVisibleDialog());
    QCOMPARE(server.requestCount(), 1);

    UpdateDialog* const dialog = automaticUpdateDialog();
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    QVERIFY(dialog->property("automaticUpdatePrompt").toBool());
    auto* const secondaryButton = dialog->findChild<QPushButton*>(
        QStringLiteral("updateSecondaryButton")
        );
    auto* const detailsLabel = dialog->findChild<QLabel*>(
        QStringLiteral("programUpdateDetails")
        );
    QVERIFY(secondaryButton);
    QVERIFY(detailsLabel);
    QVERIFY(detailsLabel->isVisible());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));
    QVERIFY(!settings.get(skippedVersionKey).isValid());
    QVERIFY(
        !detailsLabel->text().contains(
            QStringLiteral("could not be saved"),
            Qt::CaseInsensitive
            )
        );

    secondaryButton->click();

    QVERIFY(controller.hasVisibleDialog());
    QVERIFY(dialog->isVisible());
    QVERIFY(dialog->property("automaticUpdatePrompt").toBool());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(skippedVersionKey).isValid());
    QVERIFY(
        !skippedUpdateVersionPreferences->read().skippedVersion.has_value()
        );
    QVERIFY(
        detailsLabel->text().contains(
            QStringLiteral("The skipped update version could not be saved:"),
            Qt::CaseInsensitive
            )
        );
    QCOMPARE(
        detailsLabel->text().count(
            QStringLiteral("The skipped update version could not be saved:")
            ),
        1
        );
    QCOMPARE(detailsLabel->text().count(QStringLiteral("AccessError")), 1);

    // Hiding does not finish the automatic dialog. A subsequent eligible
    // update result must be able to show it again, which proves the failed
    // save did not set automatic-prompt suppression.
    dialog->hide();
    QVERIFY(!controller.hasVisibleDialog());
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 2);
    QCOMPARE(server.requestCount(), 2);
    QTRY_VERIFY(controller.hasVisibleDialog());
    QVERIFY(dialog->isVisible());
    QCOMPARE(secondaryButton->text(), QStringLiteral("Skip This Version"));
    QVERIFY(!settings.get(skippedVersionKey).isValid());
    QCOMPARE(synchronizationAttempts, 1);

    injectAccessError = false;
    secondaryButton->click();

    QTRY_VERIFY(!controller.hasVisibleDialog());
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(skippedVersionKey), QVariant(QStringLiteral("2.0.0")));
    QCOMPARE(
        skippedUpdateVersionPreferences->read().skippedVersion,
        std::optional<SkippedUpdateVersionText>(
            skippedVersionText("2.0.0")
            )
        );
    QSettings persistedSettings(
        QDir(m_settingsRoot->path()).filePath(
            QStringLiteral("PaperCloud/ClassMngr.ini")
            ),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(
        persistedSettings.value(skippedVersionKey),
        QVariant(QStringLiteral("2.0.0"))
        );

    // A later newer result clears the now-stale stored value. The automatic
    // prompt remains suppressed after the successful Skip Version operation.
    server.setBody(validReleaseResponse(QStringLiteral("2.1.0")));
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeededSpy.count(), 3);
    QCOMPARE(server.requestCount(), 3);
    QVERIFY(!controller.hasVisibleDialog());
    QVERIFY(!settings.get(skippedVersionKey).isValid());
    QCOMPARE(synchronizationAttempts, 3);
}

void UpdateControllerAutomaticStartupTests::bridgeMapsSuccessAndForcesFreshCheck_data()
{
    QTest::addColumn<bool>("updateAvailable");
    QTest::newRow("update") << true;
    QTest::newRow("no-update") << false;
}

void UpdateControllerAutomaticStartupTests::bridgeMapsSuccessAndForcesFreshCheck()
{
    QFETCH(bool, updateAvailable);
    using namespace ClassMngr::Next::Application;
    QCoreApplication::setApplicationVersion(
        updateAvailable ? QStringLiteral("1.0.0") : QStringLiteral("3.0.0"));
    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    QSignalSpy succeeded(&service, &UpdateService::checkSucceeded);
    QSignalSpy started(&service, &UpdateService::checkStarted);
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeeded.count(), 1);
    QVERIFY(service.isResultFresh());

    ClassMngr::Next::Platform::UpdateServiceStartupCheckPort bridge(&service);
    int completions = 0;
    std::optional<StartupUpdateCheckOutcome> outcome;
    QCOMPARE(bridge.check([&](StartupUpdateCheckOutcome value)
        {
            ++completions;
            outcome = std::move(value);
        }), StartupUpdateCheckDispatch::Started);
    QCOMPARE(started.count(), 2); // A fresh cached result must not skip startup.
    int busyCompletions = 0;
    QCOMPARE(bridge.check([&](StartupUpdateCheckOutcome) { ++busyCompletions; }),
        StartupUpdateCheckDispatch::Busy);
    QCOMPARE(completions, 0);
    QTRY_COMPARE(completions, 1);
    QCOMPARE(succeeded.count(), 2);
    QCOMPARE(busyCompletions, 0);
    QVERIFY(outcome && std::holds_alternative<StartupUpdateCheckResult>(*outcome));
    const auto& result = std::get<StartupUpdateCheckResult>(*outcome);
    QCOMPARE(result.updateAvailable, updateAvailable);
    QCOMPARE(result.latestVersion, (UpdateVersion{2, 0, 0}));
    const auto legacy = service.lastResult();
    QVERIFY(legacy.has_value());
    QCOMPARE(result.currentVersion.major, legacy->currentVersion.majorVersion());
    QCOMPARE(result.artifact.platformKey.value(),
        legacy->artifact.platformKey.toUtf8().toStdString());
    QCOMPARE(result.artifact.url, legacy->artifact.url.toEncoded().toStdString());
    QCOMPARE(result.artifact.fileName, legacy->artifact.fileName.toStdString());
    QCOMPARE(result.artifact.sha256, legacy->artifact.sha256.toStdString());
    QCOMPARE(result.artifact.sizeBytes, legacy->artifact.sizeBytes);
    QCOMPARE(result.releaseDateIso, std::string("2026-08-05"));
    QCOMPARE(result.releaseUrl, legacy->releaseUrl.toEncoded().toStdString());
    QCOMPARE(result.checkedAtUtcMilliseconds, legacy->checkedAtUtc.toMSecsSinceEpoch());
    // Repeated terminal signals are not a second completion for this request.
    service.checkSucceeded(*legacy);
    service.checkFailedWithCategory(UpdateService::FailureCategory::Network,
        QStringLiteral("late terminal failure"));
    QCOMPARE(completions, 1);
    QCOMPARE(busyCompletions, 0);

    // A check already owned by a direct/manual caller also reports Busy and
    // must not borrow that caller's eventual completion.
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QCOMPARE(bridge.check([&](StartupUpdateCheckOutcome) { ++busyCompletions; }),
        StartupUpdateCheckDispatch::Busy);
    QTRY_COMPARE(succeeded.count(), 4);
    QCOMPARE(busyCompletions, 0);
}

void UpdateControllerAutomaticStartupTests::bridgeMapsFailures_data()
{
    QTest::addColumn<int>("failureKind");
    QTest::newRow("missing-url") << 0;
    QTest::newRow("disallowed-url") << 1;
    QTest::newRow("invalid-response") << 2;
    QTest::newRow("invalid-current-version") << 3;
    QTest::newRow("network") << 4;
}

void UpdateControllerAutomaticStartupTests::
parserMatchesPlatformKeysExactlyAndKeepsLinuxAliases()
{
    const QString knownPlatformKey = GitHubRelease::currentPlatformKeys().first();
    const QString wrongCasePlatformKey = knownPlatformKey.toUpper();
    QVERIFY(wrongCasePlatformKey != knownPlatformKey);

    const auto knownResponse = validReleaseResponse(
        QStringLiteral("2.0.0"), knownPlatformKey);
    const auto exactMatch = GitHubRelease::latestCompatibleFromJson(
        knownResponse, {knownPlatformKey});
    QVERIFY(exactMatch);
    QCOMPARE(exactMatch->artifact().platformKey, knownPlatformKey);

    const auto caseDistinctThenExact = GitHubRelease::latestCompatibleFromJson(
        knownResponse, {wrongCasePlatformKey, knownPlatformKey});
    QVERIFY(caseDistinctThenExact);
    QCOMPARE(caseDistinctThenExact->artifact().platformKey, knownPlatformKey);

    const auto wrongCaseOnly = GitHubRelease::latestCompatibleFromJson(
        knownResponse, {wrongCasePlatformKey});
    QVERIFY(!wrongCaseOnly);
    QCOMPARE(wrongCaseOnly.error(), QStringLiteral(
        "No published stable GitHub release contains a valid update asset for this platform."));

    const auto unknownKeyOnly = GitHubRelease::latestCompatibleFromJson(
        knownResponse, {QStringLiteral("unknown-architecture-key")});
    QVERIFY(!unknownKeyOnly);
    QCOMPARE(unknownKeyOnly.error(), QStringLiteral(
        "No published stable GitHub release contains a valid update asset for this platform."));

    const auto linuxAliasResponse = validReleaseResponse(
        QStringLiteral("2.0.0"), QStringLiteral("linux-x86_64"));
    for (const auto& alias : {QStringLiteral("linux-x86_64"),
            QStringLiteral("linux-x64")})
    {
        const auto aliasMatch = GitHubRelease::latestCompatibleFromJson(
            linuxAliasResponse, {alias});
        QVERIFY(aliasMatch);
        QCOMPARE(aliasMatch->artifact().platformKey, alias);
    }
}

void UpdateControllerAutomaticStartupTests::bridgeMapsFailures()
{
    QFETCH(int, failureKind);
    using namespace ClassMngr::Next::Application;
    QCoreApplication::setApplicationVersion(failureKind == 3
        ? QStringLiteral("not-a-version") : QStringLiteral("1.0.0"));
    LocalHttpServer server(failureKind == 2 ? QByteArrayLiteral("{not json")
        : validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    QUrl url = server.url();
    if (failureKind == 0)
    {
        url = QUrl();
    }
    else if (failureKind == 1)
    {
        url = QUrl(QStringLiteral("http://example.com/releases"));
    }
    else if (failureKind == 4)
    {
        QTcpServer closedPort;
        QVERIFY(closedPort.listen(QHostAddress::LocalHost, 0));
        url = QUrl(QStringLiteral("http://127.0.0.1:%1/releases").arg(closedPort.serverPort()));
        closedPort.close();
    }
    UpdateService service(configurationFor(url));
    QSignalSpy oldFailure(&service, &UpdateService::checkFailed);
    QSignalSpy typedFailure(&service, &UpdateService::checkFailedWithCategory);
    ClassMngr::Next::Platform::UpdateServiceStartupCheckPort bridge(&service);
    int completions = 0;
    std::optional<StartupUpdateCheckOutcome> outcome;
    const auto dispatch = bridge.check([&](StartupUpdateCheckOutcome value)
        {
            ++completions;
            outcome = std::move(value);
        });
    QCOMPARE(dispatch, failureKind < 2 ? StartupUpdateCheckDispatch::Failed
        : StartupUpdateCheckDispatch::Started);
    if (failureKind < 2)
    {
        QCOMPARE(completions, 1); // Inline configuration failure completes once.
    }
    QTRY_COMPARE(completions, 1);
    QCOMPARE(oldFailure.count(), 1);
    QCOMPARE(typedFailure.count(), 1);
    QCOMPARE(typedFailure.first().at(1).toString(), oldFailure.first().at(0).toString());
    QVERIFY(outcome && std::holds_alternative<StartupUpdateCheckError>(*outcome));
    const auto& error = std::get<StartupUpdateCheckError>(*outcome);
    const auto expected = failureKind < 2 ? StartupUpdateCheckErrorCode::Configuration
        : failureKind == 2 ? StartupUpdateCheckErrorCode::InvalidResponse
        : failureKind == 3 ? StartupUpdateCheckErrorCode::InvalidCurrentVersion
        : StartupUpdateCheckErrorCode::Network;
    QCOMPARE(error.code, expected);
    QCOMPARE(error.message, oldFailure.first().at(0).toString().toUtf8().toStdString());
    QVERIFY(!error.message.empty());
    service.checkFailedWithCategory(UpdateService::FailureCategory::Network,
        QStringLiteral("duplicate"));
    QCOMPARE(completions, 1);
}

void UpdateControllerAutomaticStartupTests::bridgeAndControllerDestructionSuppressDelivery()
{
    using namespace ClassMngr::Next::Application;
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    QSignalSpy succeeded(&service, &UpdateService::checkSucceeded);
    int completions = 0;
    {
        ClassMngr::Next::Platform::UpdateServiceStartupCheckPort bridge(&service);
        QCOMPARE(bridge.check([&](StartupUpdateCheckOutcome) { ++completions; }),
            StartupUpdateCheckDispatch::Started);
    }
    QTRY_COMPARE(succeeded.count(), 1);
    QCOMPARE(completions, 0);

    auto destroyedService = std::make_unique<UpdateService>(configurationFor(server.url()));
    ClassMngr::Next::Platform::UpdateServiceStartupCheckPort bridge(destroyedService.get());
    QCOMPARE(bridge.check([&](StartupUpdateCheckOutcome) { ++completions; }),
        StartupUpdateCheckDispatch::Started);
    destroyedService.reset();
    QCoreApplication::processEvents();
    QCOMPARE(completions, 0);
    std::optional<StartupUpdateCheckOutcome> missingServiceOutcome;
    QCOMPARE(bridge.check([&](StartupUpdateCheckOutcome outcome)
        { missingServiceOutcome = std::move(outcome); }), StartupUpdateCheckDispatch::Failed);
    QVERIFY(missingServiceOutcome.has_value());
    QCOMPARE(std::get<StartupUpdateCheckError>(*missingServiceOutcome).code,
        StartupUpdateCheckErrorCode::Dispatch);

    setAutomaticChecksEnabled(true);
    {
        UpdateController controller(&service);
        controller.setStartupComplete();
        controller.startAutomaticCheck();
    }
    QTRY_COMPARE(succeeded.count(), 2);
    QVERIFY(!automaticUpdateDialog());

    // A service disappearing before the controller also leaves startup safe.
    auto shortLivedService = std::make_unique<UpdateService>(configurationFor(server.url()));
    UpdateController controller(shortLivedService.get());
    shortLivedService.reset();
    controller.setStartupComplete();
    controller.startAutomaticCheck();
    QVERIFY(!controller.hasVisibleDialog());
}

void UpdateControllerAutomaticStartupTests::manualAdapterDistinguishesFreshBusyForceAndFailure()
{
    using namespace ClassMngr::Next::Application;
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());

    UpdateService service(configurationFor(server.url()));
    ClassMngr::Next::Platform::UpdateServiceManualCheckPort port(&service);
    int completions = 0;
    int busyCompletions = 0;
    std::optional<ManualUpdateCheckResult> lastResult;
    QCOMPARE(port.check(ManualUpdateCheckPolicy::IfStale,
        [&](ManualUpdateCheckOutcome outcome)
        {
            ++completions;
            const auto* result = std::get_if<ManualUpdateCheckResult>(&outcome);
            QVERIFY(result);
            lastResult = *result;
        }), ManualUpdateCheckDispatch::Started);
    QCOMPARE(port.check(ManualUpdateCheckPolicy::Force,
        [&](ManualUpdateCheckOutcome) { ++busyCompletions; }),
        ManualUpdateCheckDispatch::Busy);
    QCOMPARE(busyCompletions, 0);
    QTRY_COMPARE(completions, 1);
    QVERIFY(port.snapshot().resultFresh);
    QVERIFY(port.snapshot().cachedResult.has_value());
    const auto firstLegacyResult = service.lastResult();
    QVERIFY(firstLegacyResult.has_value());
    QVERIFY(lastResult.has_value());
    QCOMPARE(lastResult->artifact.platformKey.value(),
        firstLegacyResult->artifact.platformKey.toUtf8().toStdString());
    QCOMPARE(port.snapshot().cachedResult->artifact.platformKey.value(),
        firstLegacyResult->artifact.platformKey.toUtf8().toStdString());

    QCOMPARE(port.check(ManualUpdateCheckPolicy::IfStale,
        [&](ManualUpdateCheckOutcome) { ++busyCompletions; }),
        ManualUpdateCheckDispatch::FreshCache);
    QCOMPARE(server.requestCount(), 1);
    QCOMPARE(busyCompletions, 0);

    QCOMPARE(port.check(ManualUpdateCheckPolicy::Force,
        [&](ManualUpdateCheckOutcome outcome)
        {
            ++completions;
            const auto* result = std::get_if<ManualUpdateCheckResult>(&outcome);
            QVERIFY(result);
            lastResult = *result;
        }), ManualUpdateCheckDispatch::Started);
    QTRY_COMPARE(completions, 2);
    QCOMPARE(server.requestCount(), 2);
    const auto secondLegacyResult = service.lastResult();
    QVERIFY(secondLegacyResult.has_value());
    QVERIFY(lastResult.has_value());
    QCOMPARE(lastResult->artifact.platformKey.value(),
        secondLegacyResult->artifact.platformKey.toUtf8().toStdString());

    UpdateService unconfigured(UpdateConfiguration{});
    ClassMngr::Next::Platform::UpdateServiceManualCheckPort failedPort(&unconfigured);
    int failures = 0;
    std::optional<ManualUpdateCheckError> error;
    QCOMPARE(failedPort.check(ManualUpdateCheckPolicy::Force,
        [&](ManualUpdateCheckOutcome outcome)
        {
            ++failures;
            if (const auto* value = std::get_if<ManualUpdateCheckError>(&outcome))
            {
                error = *value;
            }
        }), ManualUpdateCheckDispatch::Failed);
    QCOMPARE(failures, 1);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, ManualUpdateCheckErrorCode::Configuration);
}

void UpdateControllerAutomaticStartupTests::manualDialogObservesConcurrentStartupResult()
{
    using namespace ClassMngr::Next::Application;
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());

    UpdateService service(configurationFor(server.url()));
    ClassMngr::Next::Platform::UpdateServiceStartupCheckPort startupPort(&service);
    StartupUpdateCheckUseCase startupUseCase(startupPort);
    ClassMngr::Next::Platform::UpdateServiceManualCheckPort manualPort(&service);
    UpdateDialog dialog(manualPort, true);

    QCOMPARE(startupUseCase.start({
        .startupComplete = true,
        .checkOnStartup = true,
        .preferences = {.automaticChecksEnabled = true},
        .hasReleasesApiUrl = true
    }, {}), StartupUpdateCheckDisposition::Started);
    QVERIFY(service.isBusy());
    dialog.refreshForOpen();
    QVERIFY(service.isBusy());
    QTRY_VERIFY(service.hasResult());
    QTRY_COMPARE(server.requestCount(), 1);

    QLabel* const title = dialog.findChild<QLabel*>(
        QStringLiteral("programUpdateTitle"));
    QVERIFY(title);
    QTRY_COMPARE(title->text(), QStringLiteral("Update Available"));
    QCOMPARE(server.requestCount(), 1);
}

void UpdateControllerAutomaticStartupTests::startupSuccessEffectsOccurOnceAndManualChecksStillWork_data()
{
    QTest::addColumn<bool>("updateAvailable");
    QTest::newRow("update") << true;
    QTest::newRow("no-update") << false;
}

void UpdateControllerAutomaticStartupTests::startupSuccessEffectsOccurOnceAndManualChecksStillWork()
{
    QFETCH(bool, updateAvailable);
    QCoreApplication::setApplicationVersion(
        updateAvailable ? QStringLiteral("1.0.0") : QStringLiteral("3.0.0"));
    setAutomaticChecksEnabled(true);
    QVERIFY(setSkippedUpdateVersion("1.5.0"));
    LocalHttpServer server(validReleaseResponse(QStringLiteral("2.0.0")));
    QVERIFY(server.listen());
    UpdateService service(configurationFor(server.url()));
    QSignalSpy succeeded(&service, &UpdateService::checkSucceeded);
    int cleanupAttempts = 0;
    auto preferences = std::make_shared<
        ClassMngr::Next::Platform::SettingsManagerSkippedUpdateVersionPort>(
            [&](QSettings&)
            {
                // Retaining stale data makes duplicate reconciliation visible.
                ++cleanupAttempts;
                return QSettings::AccessError;
            });
    UpdateController controller(&service, nullptr, preferences);
    controller.setStartupComplete();
    controller.startAutomaticCheck();
    controller.startAutomaticCheck();
    QTRY_COMPARE(succeeded.count(), 1);
    QCOMPARE(controller.hasVisibleDialog(), updateAvailable);
    QCOMPARE(cleanupAttempts, 1);
    QCOMPARE(server.requestCount(), 1);
    auto* dialog = automaticUpdateDialog();
    QCOMPARE(dialog != nullptr, updateAvailable);
    int visibleAutomaticDialogs = 0;
    for (auto* widget : QApplication::topLevelWidgets())
    {
        if (qobject_cast<UpdateDialog*>(widget) && widget->isVisible()
            && widget->property("automaticUpdatePrompt").toBool())
        {
            ++visibleAutomaticDialogs;
        }
    }
    QCOMPARE(visibleAutomaticDialogs, updateAvailable ? 1 : 0);
    controller.startAutomaticCheck();
    QCOMPARE(server.requestCount(), 1);

    // Later direct/manual checks keep their previous service signal route.
    QVERIFY(service.checkForUpdates(UpdateService::CheckPolicy::Force));
    QTRY_COMPARE(succeeded.count(), 2);
    QCOMPARE(cleanupAttempts, 2);
    QCOMPARE(server.requestCount(), 2);
    QCOMPARE(controller.hasVisibleDialog(), updateAvailable);
    if (dialog)
    {
        dialog->setProperty("automaticUpdatePrompt", false);
        dialog->close();
    }
}

QTEST_MAIN(UpdateControllerAutomaticStartupTests)

#include "update_controller_automatic_startup_tests.moc"
