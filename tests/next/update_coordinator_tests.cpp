#include "next/bootstrap/update_coordinator.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"
#include "core/settingsmanager.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QApplication>
#include <QDir>
#include <QCryptographicHash>
#include <QSignalSpy>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTranslator>
#include <QtTest>

using namespace ClassMngr::Next;

namespace
{
class AutomaticPreferences final : public Application::AutomaticUpdatePreferencesPort
{
public:
    bool enabled = false;
    Application::AutomaticUpdatePreferences read() const override { return {enabled}; }
    Domain::Result<void> write(const Application::AutomaticUpdatePreferences&) const override
    { return Domain::Result<void>::success(); }
};

// One fake service shared by both application ports, with no network manager.
class SharedChecks final : public Application::StartupUpdateCheckPort,
                           public Application::ManualUpdateCheckPort
{
public:
    Application::ManualUpdateCheckSnapshot state;
    Application::StartupUpdateCheckCompletion startupCompletion;
    Application::ManualUpdateCheckCompletion manualCompletion;
    Application::ManualUpdateCheckObserver observer;
    int startupCalls = 0;
    int manualCalls = 0;
    Application::ManualUpdateCheckPolicy lastPolicy = Application::ManualUpdateCheckPolicy::IfStale;

    Application::ManualUpdateCheckSnapshot snapshot() const override { return state; }
    void setObserver(Application::ManualUpdateCheckObserver value) override
    { observer = std::move(value); }
    Application::StartupUpdateCheckDispatch check(Application::StartupUpdateCheckCompletion completion) override
    {
        if (state.requestInProgress) return Application::StartupUpdateCheckDispatch::Busy;
        ++startupCalls;
        state.requestInProgress = true;
        startupCompletion = std::move(completion);
        if (observer) observer(Application::ManualUpdateCheckStarted{});
        return Application::StartupUpdateCheckDispatch::Started;
    }
    Application::ManualUpdateCheckDispatch check(Application::ManualUpdateCheckPolicy policy,
        Application::ManualUpdateCheckCompletion completion) override
    {
        if (state.requestInProgress) return Application::ManualUpdateCheckDispatch::Busy;
        ++manualCalls;
        lastPolicy = policy;
        state.requestInProgress = true;
        manualCompletion = std::move(completion);
        return Application::ManualUpdateCheckDispatch::Started;
    }
    void succeed(bool available = true)
    {
        Application::ManualUpdateCheckResult result{
            .currentVersion = {1, 0, 0},
            .latestVersion = available ? Application::UpdateVersion{2, 0, 0}
                                       : Application::UpdateVersion{1, 0, 0},
            .artifact = {
                .platformKey = Application::UpdateArtifactPlatformKey("macos-universal"),
                .url = Application::EncodedUrlText("https://github.com/example/ClassMngr/releases/download/v2.0.0/ClassMngr-2.0.0.dmg"),
                .fileName = Application::UpdateArtifactFileName("ClassMngr-2.0.0.dmg"),
                .sha256 = Application::UpdateArtifactSha256(QCryptographicHash::hash(
                    QByteArray(1024 * 1024, 'x'), QCryptographicHash::Sha256).toHex().toStdString()),
                .sizeBytes = Application::UpdateArtifactSizeBytes(1024 * 1024)
            },
            .releaseDateIso = Application::ReleaseDateIsoText("2026-10-01"),
            .releaseUrl = Application::EncodedUrlText("https://example.invalid/releases/2.0.0"),
            .checkedAtUtcMilliseconds = Application::UpdateCheckUtcMilliseconds(1790812800000),
            .updateAvailable = available
        };
        state = {.cachedResult = result, .resultFresh = true, .requestInProgress = false};
        auto startup = std::move(startupCompletion);
        auto manual = std::move(manualCompletion);
        if (startup)
        {
            startup(Application::StartupUpdateCheckResult{
                .currentVersion = result.currentVersion, .latestVersion = result.latestVersion,
                .artifact = {result.artifact.platformKey, result.artifact.url,
                    result.artifact.fileName, result.artifact.sha256, result.artifact.sizeBytes},
                .releaseDateIso = result.releaseDateIso, .releaseUrl = result.releaseUrl,
                .checkedAtUtcMilliseconds = result.checkedAtUtcMilliseconds,
                .updateAvailable = available});
        }
        if (manual) manual(result);
        else if (observer) observer(result);
    }
    void fail(Application::ManualUpdateCheckErrorCode code)
    {
        state.requestInProgress = false;
        auto callback = std::move(manualCompletion);
        if (callback) callback(Application::ManualUpdateCheckError{code, "Fixture update failure"});
    }
};

UpdateDialog* dialogFor(QWidget& parent)
{ return parent.findChild<UpdateDialog*>(); }
QLabel* title(UpdateDialog* dialog)
{ return dialog->findChild<QLabel*>("programUpdateTitle"); }
QPushButton* button(UpdateDialog* dialog, const char* name)
{ return dialog->findChild<QPushButton*>(QLatin1String(name)); }

void savedDownload(UpdateDownloader& downloader, const UpdateArtifact& artifact,
    const QByteArray& bytes, const QString& status)
{
    QVERIFY(QDir().mkpath(downloader.downloadDirectory()));
    const QString path = QDir(downloader.downloadDirectory()).filePath(artifact.fileName);
    QFile payload(status == "completed" ? path : path + ".part");
    QVERIFY(payload.open(QIODevice::WriteOnly));
    QCOMPARE(payload.write(bytes), bytes.size());
    payload.close();
    QFile metadata(path + ".download.json");
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QJsonDocument(QJsonObject{
        {"schemaVersion", 1}, {"platform", artifact.platformKey},
        {"url", artifact.url.toString()}, {"fileName", artifact.fileName},
        {"sha256", artifact.sha256}, {"sizeBytes", artifact.sizeBytes},
        {"etag", "fixture"}, {"lastModified", ""}, {"status", status},
        {"createdAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"updatedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    }).toJson()) > 0);
}
}

class UpdateCoordinatorTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void disabledAutomaticStillAllowsManualCheckAndRetry();
    void manualOpenObservesSharedStartupAndCache();
    void automaticPromptHonorsSkippedVersionAndReconcilesInvalidStorage();
    void persistedSkipUnskipAndStalePreferenceReconciliation();
    void preferenceWriteFailuresRemainVisible();
    void downloadPresentationAndDiscardWithoutNetwork();
    void cancelDestroysDialogAndSuppressesLateCompletion();
    void bilingualThemeCaptures();
private:
    QTemporaryDir m_settings;
};

void UpdateCoordinatorTests::initTestCase()
{
    QVERIFY(m_settings.isValid());
    qputenv("CLASSMNGR_SETTINGS_ROOT", m_settings.path().toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("ClassMngrC47Tests");
    SettingsManager::instance().clear();
}

void UpdateCoordinatorTests::disabledAutomaticStillAllowsManualCheckAndRetry()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.clear());
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
    coordinator.dispatch();
    QCOMPARE(checks.startupCalls, 0);
    coordinator.openManual();
    auto* dialog = dialogFor(parent);
    QVERIFY(dialog && dialog->isVisible());
    QVERIFY(!dialog->findChild<QWidget*>("resourcePackUpdateSection"));
    QVERIFY(!dialog->findChild<QLabel*>("resourcePackUpdateText"));
    QCOMPARE(checks.manualCalls, 1);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Checking for Updates"));
    checks.fail(Application::ManualUpdateCheckErrorCode::Configuration);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Update Check Failed"));
    QVERIFY(dialog->findChild<QLabel*>("programUpdateDetails")->text().contains("Fixture update failure"));
    button(dialog, "updateCheckButton")->click();
    QCOMPARE(checks.manualCalls, 2);
    QCOMPARE(checks.lastPolicy, Application::ManualUpdateCheckPolicy::Force);
    checks.fail(Application::ManualUpdateCheckErrorCode::Network);
    QVERIFY(button(dialog, "updateCheckButton")->isEnabled());
    button(dialog, "updateCheckButton")->click();
    checks.succeed(false);
    QCOMPARE(title(dialog)->text(), QStringLiteral("You're Up to Date"));
    coordinator.openManual();
    QCOMPARE(checks.manualCalls, 3); // Fresh results do not send another request.
}

void UpdateCoordinatorTests::manualOpenObservesSharedStartupAndCache()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    automatic.enabled = true;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.clear());
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
    coordinator.dispatch();
    QCOMPARE(checks.startupCalls, 1);
    coordinator.openManual();
    auto* dialog = dialogFor(parent);
    QCOMPARE(checks.manualCalls, 0);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Checking for Updates"));
    checks.succeed();
    QCOMPARE(parent.findChildren<UpdateDialog*>().size(), 1);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Update Available"));
    QVERIFY(button(dialog, "updatePrimaryButton")->isEnabled());
    coordinator.openManual();
    QCOMPARE(checks.manualCalls, 0);
    QCOMPARE(checks.startupCalls, 1);
}

void UpdateCoordinatorTests::automaticPromptHonorsSkippedVersionAndReconcilesInvalidStorage()
{
    AutomaticPreferences automatic;
    automatic.enabled = true;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.write({Application::SkippedUpdateVersionText("2.0.0")}));
    {
        SharedChecks checks;
        QWidget parent;
        Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
        coordinator.dispatch();
        checks.succeed();
        QVERIFY(!dialogFor(parent));
        coordinator.openManual();
        QVERIFY(dialogFor(parent)->isVisible());
        QCOMPARE(button(dialogFor(parent), "updateSecondaryButton")->text(),
            QStringLiteral("Notify Me About This Version"));
    }
    QVERIFY(skipped.write({Application::SkippedUpdateVersionText("invalid-version")}));
    {
        SharedChecks checks;
        QWidget parent;
        Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
        coordinator.dispatch();
        checks.succeed();
        QVERIFY(dialogFor(parent)->isVisible());
        QVERIFY(dialogFor(parent)->property("automaticUpdatePrompt").toBool());
        QVERIFY(!skipped.read().skippedVersion);
        QCOMPARE(checks.manualCalls, 0);
    }
}

void UpdateCoordinatorTests::persistedSkipUnskipAndStalePreferenceReconciliation()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.clear());
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
    coordinator.openManual();
    checks.succeed();
    button(dialogFor(parent), "updateSecondaryButton")->click();
    QCOMPARE(skipped.read().skippedVersion->value(), std::string("2.0.0"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!dialogFor(parent));
    coordinator.openManual();
    auto* dialog = dialogFor(parent);
    QCOMPARE(button(dialog, "updateSecondaryButton")->text(), QStringLiteral("Notify Me About This Version"));
    button(dialog, "updateSecondaryButton")->click();
    QVERIFY(!skipped.read().skippedVersion);
    QVERIFY(button(dialog, "updateSecondaryButton")->text().contains("Skip"));
    QVERIFY(skipped.write({Application::SkippedUpdateVersionText("0.1.0")}));
    checks.state.resultFresh = false;
    coordinator.openManual();
    checks.succeed();
    QVERIFY(!skipped.read().skippedVersion);
}

void UpdateCoordinatorTests::preferenceWriteFailuresRemainVisible()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    QVERIFY(Platform::SettingsManagerSkippedUpdateVersionPort().clear());
    Platform::SettingsManagerSkippedUpdateVersionPort skipped([](QSettings&) { return QSettings::AccessError; });
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
    coordinator.openManual();
    checks.succeed();
    button(dialogFor(parent), "updateSecondaryButton")->click();
    QVERIFY(dialogFor(parent)->isVisible());
    QVERIFY(dialogFor(parent)->findChild<QLabel*>("programUpdateDetails")->text().contains("could not be saved"));
    QVERIFY(Platform::SettingsManagerSkippedUpdateVersionPort().write(
        {Application::SkippedUpdateVersionText("2.0.0")}));
    coordinator.openManual();
    button(dialogFor(parent), "updateSecondaryButton")->click();
    QVERIFY(dialogFor(parent)->findChild<QLabel*>("programUpdateDetails")->text().contains("could not be cleared"));
    QVERIFY(Platform::SettingsManagerSkippedUpdateVersionPort().clear());
}

void UpdateCoordinatorTests::downloadPresentationAndDiscardWithoutNetwork()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.clear());
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, false, false, &parent);
    coordinator.openManual();
    checks.succeed();
    auto* dialog = dialogFor(parent);
    auto* downloader = dialog->findChild<UpdateDownloader*>();
    QVERIFY(downloader);
    const auto artifact = UiShared::Details::toPresentationResult(*checks.state.cachedResult).artifact;
    savedDownload(*downloader, artifact, QByteArray(512, 'x'), "partial");
    QSignalSpy networkStarted(downloader, &UpdateDownloader::downloadStarted);
    QSignalSpy failed(downloader, &UpdateDownloader::downloadFailed);
    // Start from an on-disk fixture and pause during preparation, before any
    // network request can be dispatched. This exercises the actual buttons.
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(downloader->state(), UpdateDownloader::State::Preparing);
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(downloader->state(), UpdateDownloader::State::Paused);
    QCoreApplication::processEvents();
    QCOMPARE(networkStarted.count(), 0);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Download Paused"));
    QVERIFY(button(dialog, "updatePrimaryButton")->text().contains("Resume"));
    button(dialog, "updateSecondaryButton")->click();
    QCOMPARE(downloader->state(), UpdateDownloader::State::Idle);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Update Available"));
    QVERIFY(!QFile::exists(QDir(downloader->downloadDirectory()).filePath(artifact.fileName + ".part")));
    // Deterministic transfer signals cover presentation without a live server.
    emit downloader->downloadStarted();
    emit downloader->downloadProgress(512, 1024);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Downloading Update"));
    QCOMPARE(dialog->findChild<QProgressBar*>("programUpdateProgress")->value(), 50);
    emit downloader->downloadPaused(512, 1024);
    button(dialog, "updateSecondaryButton")->click();
    // The real downloader rehashes this complete saved fixture and reaches
    // installation readiness without downloading or launching an installer.
    savedDownload(*downloader, artifact, QByteArray(1024 * 1024, 'x'), "completed");
    const int startsBeforeReuse = networkStarted.count();
    button(dialog, "updatePrimaryButton")->click();
    QTRY_COMPARE(downloader->state(), UpdateDownloader::State::Completed);
    QCOMPARE(networkStarted.count(), startsBeforeReuse);
    QCOMPARE(failed.count(), 0);
#if defined(Q_OS_LINUX)
    QCOMPARE(button(dialog, "updatePrimaryButton")->text(), QStringLiteral("Open Download Folder"));
#else
    QCOMPARE(button(dialog, "updatePrimaryButton")->text(), QStringLiteral("Install and Close"));
#endif
    QVERIFY(button(dialog, "updatePrimaryButton")->isEnabled());
    downloader->discard();
}

void UpdateCoordinatorTests::cancelDestroysDialogAndSuppressesLateCompletion()
{
    for (const bool fromStartup : {false, true})
    {
        SharedChecks checks;
        AutomaticPreferences automatic;
        automatic.enabled = fromStartup;
        Platform::SettingsManagerSkippedUpdateVersionPort skipped;
        QWidget parent;
        Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
        if (fromStartup) coordinator.dispatch();
        coordinator.openManual();
        QPointer<UpdateDialog> dialog = dialogFor(parent);
        coordinator.cancel();
        QVERIFY(!dialog);
        QVERIFY(!checks.observer);
        checks.succeed();
        coordinator.openManual();
        QVERIFY(!dialogFor(parent));
    }
}

void UpdateCoordinatorTests::bilingualThemeCaptures()
{
    const QString output = qEnvironmentVariable("CLASSMNGR_C47_CAPTURE_DIR");
    if (output.isEmpty()) QSKIP("Set CLASSMNGR_C47_CAPTURE_DIR to regenerate the review package.");
    QVERIFY(QDir().mkpath(output));
    qApp->setStyle("Fusion");
    QJsonArray captures;
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        QTranslator translator;
        QVERIFY(translator.load(QDir(QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY))
            .filePath(QStringLiteral("ClassMngr_%1.qm").arg(locale))));
        qApp->installTranslator(&translator);
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QFile stylesheet(QDir(QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT))
                .filePath(QStringLiteral("assets/styles/%1.qss").arg(theme)));
            QVERIFY(stylesheet.open(QIODevice::ReadOnly));
            qApp->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
            SharedChecks checks;
            AutomaticPreferences automatic;
            Platform::SettingsManagerSkippedUpdateVersionPort skipped;
            QVERIFY(skipped.clear());
            QWidget parent;
            Bootstrap::UpdateCoordinator coordinator(checks, checks, automatic, skipped, true, true, &parent);
            coordinator.openManual();
            auto* dialog = dialogFor(parent);
            const auto capture = [&](const QString& state) {
                QCoreApplication::processEvents();
                const QString file = QStringLiteral("next_%1_%2_%3.png").arg(locale, theme, state);
                const QImage pixels = dialog->grab().toImage();
                QVERIFY(!pixels.isNull());
                QVERIFY(pixels.save(QDir(output).filePath(file)));
                captures.append(QJsonObject{{"locale", locale}, {"theme", theme}, {"state", state},
                    {"file", file}, {"width", pixels.width()}, {"height", pixels.height()}});
            };
            capture("checking");
            checks.fail(Application::ManualUpdateCheckErrorCode::Configuration);
            capture("configuration_error");
            button(dialog, "updateCheckButton")->click();
            checks.succeed();
            capture("available");
            if (locale == "ko_KR")
            {
                const QString details = dialog->findChild<QLabel*>("programUpdateDetails")->text();
                QVERIFY(details.contains(QStringLiteral("출시일:")));
                QVERIFY(!details.contains(QStringLiteral("Released:")));
            }
            QVERIFY(skipped.write({Application::SkippedUpdateVersionText("2.0.0")}));
            coordinator.openManual();
            capture("skipped");
            QVERIFY(skipped.clear());
            coordinator.openManual();
            auto* downloader = dialog->findChild<UpdateDownloader*>();
            emit downloader->downloadStarted();
            emit downloader->downloadProgress(512, 1024);
            capture("downloading");
            emit downloader->downloadPaused(512, 1024);
            capture("paused");
            emit downloader->downloadSucceeded(QStringLiteral("/fixture/verified.dmg"));
            capture("install");
            button(dialog, "updateCheckButton")->click();
            // A new fixture completion supplies deterministic terminal states.
            checks.state.resultFresh = false;
            coordinator.openManual();
            checks.fail(Application::ManualUpdateCheckErrorCode::Network);
            capture("error");
            button(dialog, "updateCheckButton")->click();
            checks.succeed(false);
            capture("up_to_date");
            if (locale == "ko_KR")
                QVERIFY(dialog->findChild<QLabel*>("programUpdateDetails")->text()
                    .contains(QStringLiteral("최신 버전:")));
        }
        qApp->removeTranslator(&translator);
    }
    qApp->setStyleSheet({});
    QFile manifest(QDir(output).filePath("captures.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    QVERIFY(manifest.write(QJsonDocument(QJsonObject{
        {"task", "phase7_c47_manual_update_implementation_01"},
        {"route", "Next bootstrap UpdateCoordinator → shared UpdateDialog"},
        {"automaticChecksEnabled", false},
        {"fixture", "SharedChecks fake application ports and deterministic downloader signals; no network or installer launch"},
        {"captures", captures}}).toJson()) > 0);
}

int main(int argc, char** argv)
{
    QTemporaryDir isolatedTemporaryRoot;
    if (!isolatedTemporaryRoot.isValid()) return 1;
    for (const char* variable : {"TMP", "TEMP", "TMPDIR"})
        qputenv(variable, isolatedTemporaryRoot.path().toUtf8());
    QApplication application(argc, argv);
    UpdateCoordinatorTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "update_coordinator_tests.moc"
