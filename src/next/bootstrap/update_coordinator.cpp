#include "next/bootstrap/update_coordinator.h"

#include "core/updater/update_downloader.h"
#include "core/updater/version.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QWidget>

namespace ClassMngr::Next::Bootstrap
{
namespace
{
QString versionText(const Application::UpdateVersion& version)
{
    return Version(version.major, version.minor, version.patch).toString();
}

Application::ManualUpdateCheckResult manualResult(
    const Application::StartupUpdateCheckResult& result)
{
    return {
        .currentVersion = result.currentVersion,
        .latestVersion = result.latestVersion,
        .artifact = {
            result.artifact.platformKey, result.artifact.url,
            result.artifact.fileName, result.artifact.sha256,
            result.artifact.sizeBytes
        },
        .releaseDateIso = result.releaseDateIso,
        .releaseUrl = result.releaseUrl,
        .checkedAtUtcMilliseconds = result.checkedAtUtcMilliseconds,
        .updateAvailable = result.updateAvailable
    };
}
}

UpdateCoordinator::UpdateCoordinator(
    Application::StartupUpdateCheckPort& startupPort,
    Application::ManualUpdateCheckPort& manualPort,
    const Application::AutomaticUpdatePreferencesPort& automaticPreferences,
    const Application::SkippedUpdateVersionPreferencesPort& skippedPreferences,
    bool checkOnStartup,
    bool hasReleasesApiUrl,
    QWidget* presentationParent)
    : m_manualPort(manualPort)
    , m_automaticPreferences(automaticPreferences)
    , m_skippedPreferences(skippedPreferences)
    , m_startup(startupPort)
    , m_parent(presentationParent)
    , m_checkOnStartup(checkOnStartup)
    , m_hasReleasesApiUrl(hasReleasesApiUrl)
{
}

UpdateCoordinator::~UpdateCoordinator()
{
    cancel();
}

void UpdateCoordinator::runMaintenance()
{
    if (!m_maintenanceRun)
    {
        m_maintenanceRun = true;
        UpdateDownloader::cleanupDownloads(UpdateDownloader::CleanupMode::OrphansOnly);
    }
}

void UpdateCoordinator::dispatch()
{
    if (m_cancelled)
    {
        return;
    }
    runMaintenance();
    const QPointer<UpdateCoordinator> guard(this);
    static_cast<void>(m_startup.start({
        .startupComplete = true,
        .checkOnStartup = m_checkOnStartup,
        .preferences = m_automaticPreferences.read(),
        .hasReleasesApiUrl = m_hasReleasesApiUrl
    }, [guard](Application::StartupUpdateCheckOutcome outcome) {
        if (!guard || guard->m_cancelled)
        {
            return;
        }
        if (const auto* result = std::get_if<Application::StartupUpdateCheckResult>(&outcome))
        {
            const auto mapped = manualResult(*result);
            const bool skipped = guard->reconcile(mapped);
            guard->maintainDownloads(mapped);
            if (mapped.updateAvailable && !skipped && !guard->m_promptSuppressed
                && guard->m_checkOnStartup
                && guard->m_automaticPreferences.read().automaticChecksEnabled
                && (!guard->m_dialog || !guard->m_dialog->isVisible()))
            {
                auto* dialog = guard->ensureDialog(true);
                dialog->show();
                dialog->raise();
                dialog->refreshForOpen();
            }
        }
    }));
}

void UpdateCoordinator::openManual()
{
    if (m_cancelled)
    {
        return;
    }
    runMaintenance();
    auto* dialog = ensureDialog(false);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    dialog->refreshForOpen();
}

void UpdateCoordinator::cancel() noexcept
{
    if (m_cancelled)
    {
        return;
    }
    m_cancelled = true;
    // Destroy borrowers before their ports and service, including paused IO.
    if (m_dialog)
    {
        m_dialog->close();
        delete m_dialog.data();
    }
}

bool UpdateCoordinator::reconcile(const Application::ManualUpdateCheckResult& result)
{
    const auto stored = m_skippedPreferences.read().skippedVersion;
    Application::SkippedUpdateVersionPolicyInput input;
    input.currentVersion = result.currentVersion;
    input.latestVersion = result.latestVersion;
    const auto latest = versionText(result.latestVersion).toStdString();
    input.latestVersionText = latest;
    if (stored)
    {
        input.storedVersionText = *stored;
        const auto parsed = Version::parse(QString::fromStdString(stored->value()));
        input.state = parsed ? Application::SkippedUpdateVersionState::Valid
                             : Application::SkippedUpdateVersionState::Invalid;
        if (parsed)
        {
            input.skippedVersion = {
                parsed->majorVersion(), parsed->minorVersion(), parsed->patchVersion()};
        }
    }
    const auto decision = Application::decideSkippedUpdateVersion(input);
    if (decision.clearStoredVersion && m_skippedPreferences.clear() && m_dialog)
    {
        m_dialog->setSkippedVersion({});
    }
    return decision.suppressAutomaticPrompt;
}

void UpdateCoordinator::maintainDownloads(const Application::ManualUpdateCheckResult& result)
{
    const auto mapped = UiShared::Details::toPresentationResult(result);
    UpdateDownloader::cleanupDownloads(
        result.updateAvailable ? UpdateDownloader::CleanupMode::KeepOnlyArtifact
                               : UpdateDownloader::CleanupMode::RemoveAll,
        result.updateAvailable ? std::optional<UpdateArtifact>(mapped.artifact) : std::nullopt);
}

UpdateDialog* UpdateCoordinator::ensureDialog(bool automatic)
{
    const auto stored = m_skippedPreferences.read().skippedVersion;
    const QString skipped = stored ? QString::fromStdString(stored->value()) : QString();
    if (m_dialog)
    {
        m_dialog->setSkippedVersion(skipped);
        return m_dialog;
    }
    auto* dialog = new UpdateDialog(
        m_manualPort, true, m_parent, skipped,
        UpdateDialog::ResourcePackPlaceholder::Omit);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setProperty("automaticUpdatePrompt", automatic);
    dialog->setSkipVersionResultHandlingEnabled(true);
    m_dialog = dialog;
    connect(dialog, &UpdateDialog::checkResultReceived, this,
        [this](const Application::ManualUpdateCheckResult& result) {
            static_cast<void>(reconcile(result));
            maintainDownloads(result);
        });
    connect(dialog, &UpdateDialog::skipVersionRequested, this,
        [this, dialog](const QString& version) {
            const auto parsed = Version::parse(version);
            if (!parsed)
            {
                dialog->showSkippedVersionSaveFailure(tr("The selected update version is invalid."));
                return;
            }
            const auto saved = m_skippedPreferences.write({
                .skippedVersion = Application::SkippedUpdateVersionText(
                    parsed->toString().toStdString())});
            if (!saved)
            {
                dialog->showSkippedVersionSaveFailure(QString::fromStdString(saved.error().message));
                return;
            }
            m_promptSuppressed = true;
            dialog->completeSkipVersion(version);
        });
    connect(dialog, &UpdateDialog::unskipVersionRequested, this,
        [this, dialog] {
            const auto cleared = m_skippedPreferences.clear();
            if (!cleared)
            {
                dialog->showSkippedVersionClearFailure(QString::fromStdString(cleared.error().message));
                return;
            }
            m_promptSuppressed = false;
            dialog->completeUnskipVersion();
        });
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        if (dialog->property("automaticUpdatePrompt").toBool())
        {
            m_promptSuppressed = true;
        }
    });
    return dialog;
}

} // namespace ClassMngr::Next::Bootstrap
