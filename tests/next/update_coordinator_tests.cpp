#include "next/bootstrap/update_coordinator.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"
#include "core/settingsmanager.h"
#include "ui/shared/dialogs/update_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QApplication>
#include <QDir>
#include <QCryptographicHash>
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

#include <optional>
#include <vector>

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

class FakeUpdateDownloadPort final : public Application::UpdateDownloadPort
{
public:
    struct CleanupCall final
    {
        Application::UpdateDownloadCleanupPolicy policy;
        std::optional<Application::UpdateArtifact> artifact;
    };

    void setObserver(Application::UpdateDownloadObserver value) override
    {
        if (value)
        {
            lastObserver = value;
        }
        observer = std::move(value);
    }
    void startOrResume(const Application::UpdateArtifact& artifact) override
    {
        lastArtifact = artifact;
        ++startOrResumeCalls;
        busy = true;
        publishEvent(Application::UpdateDownloadStarted{});
    }
    void pause() override
    {
        ++pauseCalls;
        busy = false;
        resumable = true;
        completed = false;
        publishEvent(Application::UpdateDownloadPaused{bytes, totalBytes});
    }
    void discard() override
    {
        ++discardCalls;
        busy = false;
        resumable = false;
        completed = false;
        publishEvent(Application::UpdateDownloadDiscarded{});
    }
    bool isBusy() const override { return busy; }
    bool hasResumableDownload(const Application::UpdateArtifact&) const override
    { return resumable; }
    std::int64_t resumableBytes(const Application::UpdateArtifact&) const override
    { return bytes; }
    bool hasCompletedDownload(const Application::UpdateArtifact&) const override
    { return completed; }
    void cleanup(Application::UpdateDownloadCleanupPolicy policy,
        std::optional<Application::UpdateArtifact> artifact) override
    { cleanupCalls.push_back({policy, std::move(artifact)}); }

    void publishEvent(Application::UpdateDownloadEvent event)
    {
        const auto current = observer;
        if (current)
        {
            current(std::move(event));
        }
    }

    void complete(const std::string& filePath)
    {
        busy = false;
        resumable = false;
        completed = true;
        publishEvent(Application::UpdateDownloadSucceeded{filePath});
    }

    void emitLate(Application::UpdateDownloadEvent event) const
    {
        if (lastObserver)
        {
            lastObserver(std::move(event));
        }
    }

    Application::UpdateDownloadObserver observer;
    Application::UpdateDownloadObserver lastObserver;
    std::vector<CleanupCall> cleanupCalls;
    std::optional<Application::UpdateArtifact> lastArtifact;
    std::int64_t bytes = 512;
    std::int64_t totalBytes = 1024;
    int startOrResumeCalls = 0;
    int pauseCalls = 0;
    int discardCalls = 0;
    bool busy = false;
    bool resumable = false;
    bool completed = false;
};

class FakeUpdatePackageActionPort final : public Application::UpdatePackageActionPort
{
public:
    Application::UpdatePackageActionKind primaryAction() const override
    { return action; }
    Application::UpdatePackageActionResult launchInstaller(const std::string& filePath) override
    {
        ++launchCalls;
        lastFilePath = filePath;
        return launchResult;
    }
    Application::UpdatePackageActionResult revealInFolder(const std::string& filePath) override
    {
        ++revealCalls;
        lastFilePath = filePath;
        return revealResult;
    }

    Application::UpdatePackageActionKind action =
        Application::UpdatePackageActionKind::InstallOrLaunch;
    Application::UpdatePackageActionResult launchResult =
        Application::UpdatePackageActionResult::success();
    Application::UpdatePackageActionResult revealResult =
        Application::UpdatePackageActionResult::success();
    std::string lastFilePath;
    int launchCalls = 0;
    int revealCalls = 0;
};

struct UpdatePorts final
{
    FakeUpdateDownloadPort downloads;
    FakeUpdatePackageActionPort packageActions;
};

class ScriptedPrompts final : public IUserPromptService
{
public:
    void showMessage(const PromptRequest& request) override
    { messages.push_back(request); }
    void showMessageAsync(const PromptRequest& request) override
    { messages.push_back(request); }
    PromptChoice confirm(const PromptRequest& request) override
    {
        confirmations.push_back(request);
        return nextChoice;
    }
    UnsavedChangesChoice confirmUnsavedChanges(const UnsavedChangesRequest&) override
    { return UnsavedChangesChoice::Cancel; }
    QString chooseAction(const ActionPromptRequest&) override { return {}; }

    PromptChoice nextChoice = PromptChoice::Accepted;
    std::vector<PromptRequest> confirmations;
    std::vector<PromptRequest> messages;
};

class PromptServiceGuard final
{
public:
    explicit PromptServiceGuard(IUserPromptService& service)
    { DialogServices::setUserPromptServiceForTesting(&service); }
    ~PromptServiceGuard()
    { DialogServices::setUserPromptServiceForTesting(nullptr); }
};

UpdateDialog* dialogFor(QWidget& parent)
{ return parent.findChild<UpdateDialog*>(); }
QLabel* title(UpdateDialog* dialog)
{ return dialog->findChild<QLabel*>("programUpdateTitle"); }
QPushButton* button(UpdateDialog* dialog, const char* name)
{ return dialog->findChild<QPushButton*>(QLatin1String(name)); }

}

class UpdateCoordinatorTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void disabledAutomaticStillAllowsManualCheckAndRetry();
    void updateDialogPersistsGeometryThroughNextComposition();
    void manualOpenObservesSharedStartupAndCache();
    void automaticPromptHonorsSkippedVersionAndReconcilesInvalidStorage();
    void persistedSkipUnskipAndStalePreferenceReconciliation();
    void preferenceWriteFailuresRemainVisible();
    void downloadPresentationAndDiscardWithoutNetwork();
    void downloadFailureRemainsVisibleAndCanBeRetried();
    void cleanupPolicyTracksCurrentArtifact();
    void packageActionsRemainInjectedAndResultBearing();
    void completedPackageWaitsForStartupCompletion();
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
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, true, true, &parent);
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
    QCOMPARE(updatePorts.downloads.cleanupCalls.back().policy,
        Application::UpdateDownloadCleanupPolicy::RemoveAll);
    QVERIFY(!updatePorts.downloads.cleanupCalls.back().artifact.has_value());
    coordinator.openManual();
    QCOMPARE(checks.manualCalls, 3); // Fresh results do not send another request.
}

void UpdateCoordinatorTests::updateDialogPersistsGeometryThroughNextComposition()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.clear());
    SettingsManager::instance().set(
        QStringLiteral("ui/dialogs/update/geometry"),
        QByteArray()
        );
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(
        checks,
        checks,
        updatePorts.downloads,
        updatePorts.packageActions,
        automatic,
        skipped,
        true,
        true,
        &parent
        );

    coordinator.openManual();
    auto* const dialog = dialogFor(parent);
    QVERIFY(dialog && dialog->isVisible());
    dialog->resize(560, 420);
    dialog->close();

    QVERIFY(!SettingsManager::instance()
        .get(QStringLiteral("ui/dialogs/update/geometry"))
        .toByteArray()
        .isEmpty());
}

void UpdateCoordinatorTests::manualOpenObservesSharedStartupAndCache()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    automatic.enabled = true;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(skipped.clear());
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, true, true, &parent);
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
        UpdatePorts updatePorts;
        QWidget parent;
        Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
            updatePorts.packageActions, automatic, skipped, true, true, &parent);
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
        UpdatePorts updatePorts;
        QWidget parent;
        Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
            updatePorts.packageActions, automatic, skipped, true, true, &parent);
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
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, true, true, &parent);
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
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, true, true, &parent);
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
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, false, false, &parent);
    coordinator.openManual();
    checks.succeed();
    auto* dialog = dialogFor(parent);
    QVERIFY(dialog);
    QVERIFY(updatePorts.downloads.cleanupCalls.size() >= 2U);
    QCOMPARE(updatePorts.downloads.cleanupCalls.front().policy,
        Application::UpdateDownloadCleanupPolicy::OrphansOnly);
    QCOMPARE(updatePorts.downloads.cleanupCalls.back().policy,
        Application::UpdateDownloadCleanupPolicy::KeepOnlyArtifact);
    QVERIFY(updatePorts.downloads.cleanupCalls.back().artifact.has_value());
    QCOMPARE(updatePorts.downloads.cleanupCalls.back().artifact->fileName.rawValue(),
        "ClassMngr-2.0.0.dmg");

    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(updatePorts.downloads.startOrResumeCalls, 1);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Downloading Update"));
    QVERIFY(updatePorts.downloads.lastArtifact.has_value());
    updatePorts.downloads.publishEvent(Application::UpdateDownloadProgress{512, 1024});
    QCOMPARE(dialog->findChild<QProgressBar*>("programUpdateProgress")->value(), 50);
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(updatePorts.downloads.pauseCalls, 1);
    QVERIFY(updatePorts.downloads.resumable);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Download Paused"));
    QVERIFY(button(dialog, "updatePrimaryButton")->text().contains("Resume"));
    QVERIFY(button(dialog, "updateSecondaryButton")->isEnabled());
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(updatePorts.downloads.startOrResumeCalls, 2);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Downloading Update"));

    // Closing the dialog is another pause path and retains resumable state.
    dialog->close();
    QCOMPARE(updatePorts.downloads.pauseCalls, 2);
    QVERIFY(updatePorts.downloads.resumable);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!dialogFor(parent));
    QVERIFY(!updatePorts.downloads.observer);

    coordinator.openManual();
    dialog = dialogFor(parent);
    QVERIFY(dialog);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Download Paused"));
    button(dialog, "updateSecondaryButton")->click();
    QCOMPARE(updatePorts.downloads.discardCalls, 1);
    QVERIFY(!updatePorts.downloads.resumable);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Update Available"));
}

void UpdateCoordinatorTests::downloadFailureRemainsVisibleAndCanBeRetried()
{
    SharedChecks checks;
    AutomaticPreferences automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, false, false, &parent);
    coordinator.openManual();
    checks.succeed();

    auto* dialog = dialogFor(parent);
    QVERIFY(dialog);
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(title(dialog)->text(), QStringLiteral("Downloading Update"));

    updatePorts.downloads.busy = false;
    updatePorts.downloads.publishEvent(
        Application::UpdateDownloadFailed{"Fixture service error"});

    QCOMPARE(title(dialog)->text(), QStringLiteral("Update Available"));
    QVERIFY(dialog->findChild<QLabel*>("programUpdateDetails")->text().contains(
        QStringLiteral("The update download failed: Fixture service error")));
    QVERIFY(button(dialog, "updatePrimaryButton")->isEnabled());
    QCOMPARE(button(dialog, "updatePrimaryButton")->text(),
        QStringLiteral("Download Update"));

    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(updatePorts.downloads.startOrResumeCalls, 2);
    QCOMPARE(title(dialog)->text(), QStringLiteral("Downloading Update"));
}

void UpdateCoordinatorTests::cleanupPolicyTracksCurrentArtifact()
{
    AutomaticPreferences automatic;
    automatic.enabled = true;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    SharedChecks checks;
    UpdatePorts updatePorts;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, true, true, &parent);
    coordinator.dispatch();
    QCOMPARE(updatePorts.downloads.cleanupCalls.size(), 1U);
    QCOMPARE(updatePorts.downloads.cleanupCalls.back().policy,
        Application::UpdateDownloadCleanupPolicy::OrphansOnly);
    checks.succeed();
    QVERIFY(updatePorts.downloads.cleanupCalls.size() >= 2U);
    QCOMPARE(updatePorts.downloads.cleanupCalls.back().policy,
        Application::UpdateDownloadCleanupPolicy::KeepOnlyArtifact);
    QVERIFY(updatePorts.downloads.cleanupCalls.back().artifact.has_value());
    QCOMPARE(updatePorts.downloads.cleanupCalls.back().artifact->fileName.rawValue(),
        "ClassMngr-2.0.0.dmg");

    SharedChecks noUpdateChecks;
    UpdatePorts noUpdatePorts;
    QWidget noUpdateParent;
    Bootstrap::UpdateCoordinator noUpdateCoordinator(noUpdateChecks, noUpdateChecks,
        noUpdatePorts.downloads, noUpdatePorts.packageActions, automatic, skipped,
        true, true, &noUpdateParent);
    noUpdateCoordinator.openManual();
    noUpdateChecks.succeed(false);
    QCOMPARE(noUpdatePorts.downloads.cleanupCalls.back().policy,
        Application::UpdateDownloadCleanupPolicy::RemoveAll);
    QVERIFY(!noUpdatePorts.downloads.cleanupCalls.back().artifact.has_value());
}

void UpdateCoordinatorTests::packageActionsRemainInjectedAndResultBearing()
{
    ScriptedPrompts prompts;
    PromptServiceGuard promptsGuard(prompts);
    SharedChecks checks;
    AutomaticPreferences automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    UpdatePorts updatePorts;
    updatePorts.downloads.completed = true;
    QWidget parent;
    Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
        updatePorts.packageActions, automatic, skipped, true, true, &parent);
    coordinator.openManual();
    checks.succeed();
    auto* dialog = dialogFor(parent);
    QCOMPARE(updatePorts.downloads.startOrResumeCalls, 1); // Reverify saved package.
    updatePorts.downloads.complete("/fixture/verified-package.exe");
    QCOMPARE(title(dialog)->text(), QStringLiteral("Ready to Install"));

    prompts.nextChoice = PromptChoice::Rejected;
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(prompts.confirmations.size(), 1U);
    QCOMPARE(updatePorts.packageActions.launchCalls, 0);
    QVERIFY(dialog->isVisible());

    prompts.nextChoice = PromptChoice::Accepted;
    updatePorts.packageActions.launchResult =
        Application::UpdatePackageActionResult::failure("fixture launch failure");
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(updatePorts.packageActions.launchCalls, 1);
    QCOMPARE(updatePorts.packageActions.lastFilePath,
        std::string("/fixture/verified-package.exe"));
    QVERIFY(dialog->findChild<QLabel*>("programUpdateDetails")->text()
        .contains("fixture launch failure"));
    QVERIFY(dialog->isVisible());

    updatePorts.packageActions.launchResult =
        Application::UpdatePackageActionResult::success();
    button(dialog, "updatePrimaryButton")->click();
    QCOMPARE(updatePorts.packageActions.launchCalls, 2);
    QCOMPARE(dialog->result(), QDialog::Accepted);

    SharedChecks revealChecks;
    UpdatePorts revealPorts;
    revealPorts.packageActions.action = Application::UpdatePackageActionKind::RevealInFolder;
    QWidget revealParent;
    Bootstrap::UpdateCoordinator revealCoordinator(revealChecks, revealChecks,
        revealPorts.downloads, revealPorts.packageActions, automatic, skipped,
        true, true, &revealParent);
    revealCoordinator.openManual();
    revealChecks.succeed();
    auto* revealDialog = dialogFor(revealParent);
    revealPorts.downloads.complete("/fixture/verified.tar.gz");
    QCOMPARE(title(revealDialog)->text(), QStringLiteral("Download Complete"));
    revealPorts.packageActions.revealResult =
        Application::UpdatePackageActionResult::failure("fixture reveal failure");
    button(revealDialog, "updatePrimaryButton")->click();
    QCOMPARE(revealPorts.packageActions.revealCalls, 1);
    QVERIFY(revealDialog->findChild<QLabel*>("programUpdateDetails")->text()
        .contains("fixture reveal failure"));
    revealPorts.packageActions.revealResult =
        Application::UpdatePackageActionResult::success();
    button(revealDialog, "updatePrimaryButton")->click();
    QCOMPARE(revealPorts.packageActions.revealCalls, 2);
    QCOMPARE(prompts.messages.size(), 1U);
    QCOMPARE(prompts.messages.back().title, QStringLiteral("Linux Update Downloaded"));
}

void UpdateCoordinatorTests::completedPackageWaitsForStartupCompletion()
{
    SharedChecks checks;
    UpdatePorts updatePorts;
    updatePorts.downloads.completed = true;
    UpdateDialog dialog(checks, updatePorts.downloads,
        updatePorts.packageActions, false);
    dialog.refreshForOpen();
    checks.succeed();
    QCOMPARE(updatePorts.downloads.startOrResumeCalls, 1);
    updatePorts.downloads.complete("/fixture/verified-package.exe");
    QCOMPARE(title(&dialog)->text(), QStringLiteral("Ready to Install"));
    QVERIFY(!button(&dialog, "updatePrimaryButton")->isEnabled());
    dialog.setStartupComplete(true);
    QVERIFY(button(&dialog, "updatePrimaryButton")->isEnabled());
}

void UpdateCoordinatorTests::cancelDestroysDialogAndSuppressesLateCompletion()
{
    for (const bool fromStartup : {false, true})
    {
        SharedChecks checks;
        AutomaticPreferences automatic;
        automatic.enabled = fromStartup;
        Platform::SettingsManagerSkippedUpdateVersionPort skipped;
        UpdatePorts updatePorts;
        QWidget parent;
        Bootstrap::UpdateCoordinator coordinator(checks, checks, updatePorts.downloads,
            updatePorts.packageActions, automatic, skipped, true, true, &parent);
        if (fromStartup) coordinator.dispatch();
        coordinator.openManual();
        QPointer<UpdateDialog> dialog = dialogFor(parent);
        const auto pendingDownloadEvent = updatePorts.downloads.lastObserver;
        coordinator.cancel();
        QVERIFY(!dialog);
        QVERIFY(!checks.observer);
        QVERIFY(!updatePorts.downloads.observer);
        if (pendingDownloadEvent)
        {
            pendingDownloadEvent(Application::UpdateDownloadSucceeded{
                "/fixture/late-package.exe"});
        }
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
            UpdatePorts updatePorts;
            QWidget parent;
            Bootstrap::UpdateCoordinator coordinator(checks, checks,
                updatePorts.downloads, updatePorts.packageActions,
                automatic, skipped, true, true, &parent);
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
            updatePorts.downloads.publishEvent(Application::UpdateDownloadStarted{});
            updatePorts.downloads.publishEvent(Application::UpdateDownloadProgress{512, 1024});
            capture("downloading");
            updatePorts.downloads.publishEvent(Application::UpdateDownloadPaused{512, 1024});
            capture("paused");
            updatePorts.downloads.complete("/fixture/verified.dmg");
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
