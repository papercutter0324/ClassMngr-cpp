#include "update_controller.h"

#include "app/mainwindow.h"
#include "core/settingsmanager.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"
#include "core/updater/version.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QAction>
#include <QDialog>

#include <optional>

UpdateController::UpdateController(
    UpdateService* service,
    QObject* parent
    )
    : QObject(parent)
    , m_service(service)
{
    Q_ASSERT(m_service);

    connect(
        m_service,
        &UpdateService::checkSucceeded,
        this,
        [this](const UpdateCheckResult& result)
        {
            reconcileSkippedVersion(result);

            UpdateDownloader::cleanupDownloads(
                result.updateAvailable
                    ? UpdateDownloader::CleanupMode::KeepOnlyArtifact
                    : UpdateDownloader::CleanupMode::RemoveAll,
                result.updateAvailable
                    ? std::optional<UpdateArtifact>(result.artifact)
                    : std::nullopt
                );

            if (
                result.updateAvailable
                && !hasVisibleDialog()
                && !m_automaticPromptSuppressed
                && automaticChecksEnabled()
                && !isVersionSkipped(
                    result.latestVersion.toString()
                    )
                )
            {
                showAutomaticUpdateDialog();
            }
        }
        );
}

bool UpdateController::automaticChecksEnabled() const
{
    return m_service
        && m_service->configuration().checkOnStartup
        && SettingsManager::instance()
            .automaticUpdateChecksEnabled();
}

bool UpdateController::isVersionSkipped(
    const QString& version
    ) const
{
    return !version.trimmed().isEmpty()
        && SettingsManager::instance().skippedUpdateVersion()
            == version.trimmed();
}

void UpdateController::reconcileSkippedVersion(
    const UpdateCheckResult& result
    )
{
    SettingsManager& settings =
        SettingsManager::instance();
    const QString skippedText =
        settings.skippedUpdateVersion();

    if (skippedText.isEmpty())
    {
        return;
    }

    const auto skipped =
        Version::parse(skippedText);
    if (
        !skipped
        || result.currentVersion >= *skipped
        || result.latestVersion > *skipped
        )
    {
        settings.clearSkippedUpdateVersion();
        if (m_dialog)
        {
            m_dialog->setSkippedVersion(QString());
        }
    }
}

void UpdateController::skipVersion(
    const QString& version
    )
{
    const auto parsed =
        Version::parse(version);
    if (!parsed)
    {
        return;
    }

    SettingsManager::instance().setSkippedUpdateVersion(
        parsed->toString()
        );
    m_automaticPromptSuppressed =
        true;
}

void UpdateController::unskipVersion()
{
    SettingsManager::instance().clearSkippedUpdateVersion();
    m_automaticPromptSuppressed =
        false;
}

void UpdateController::attachMainWindow(
    MainWindow* window,
    ActionRegistry& actions
    )
{
    m_window =
        window;

    connect(
        window,
        &QObject::destroyed,
        this,
        [this]()
        {
            if (m_dialog)
            {
                m_dialog->close();
            }

            m_window =
                nullptr;
        }
        );

    if (actions.checkForUpdates)
    {
        connect(
            actions.checkForUpdates,
            &QAction::triggered,
            this,
            &UpdateController::showManualUpdateDialog,
            Qt::UniqueConnection
            );
    }
}

void UpdateController::runStartupMaintenance()
{
    if (m_startupMaintenanceRun)
    {
        return;
    }

    m_startupMaintenanceRun = true;
    UpdateDownloader::cleanupDownloads(
        UpdateDownloader::CleanupMode::OrphansOnly
        );
}

void UpdateController::startAutomaticCheck()
{
    // The application-wide startup-complete transition is the only point at
    // which background update work may begin.  Keeping the guard here makes
    // that ordering hold even if this controller is reused by another
    // bootstrap path.
    if (
        !m_startupComplete
        || m_automaticCheckStarted
        || !m_service
        )
    {
        return;
    }

    runStartupMaintenance();

    const UpdateConfiguration configuration =
        m_service->configuration();

    if (
        !automaticChecksEnabled()
        || !configuration.hasReleasesApiUrl()
        )
    {
        return;
    }

    m_automaticCheckStarted =
        true;
    m_service->checkForUpdates(
        UpdateService::CheckPolicy::Force
        );
}

void UpdateController::setStartupComplete()
{
    m_startupComplete =
        true;

    if (m_dialog)
    {
        m_dialog->setStartupComplete(true);
    }
}

bool UpdateController::hasVisibleDialog() const
{
    return m_dialog
        && m_dialog->isVisible();
}

void UpdateController::showManualUpdateDialog()
{
    runStartupMaintenance();

    UpdateDialog* dialog =
        ensureDialog(false);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    dialog->refreshForOpen();
}

void UpdateController::showAutomaticUpdateDialog()
{
    UpdateDialog* dialog =
        ensureDialog(true);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    dialog->refreshForOpen();
}

UpdateDialog* UpdateController::ensureDialog(
    bool automaticPrompt
    )
{
    if (m_dialog)
    {
        m_dialog->setSkippedVersion(
            SettingsManager::instance().skippedUpdateVersion()
            );
        if (automaticPrompt)
        {
            m_dialog->setProperty(
                "automaticUpdatePrompt",
                true
                );
        }

        return m_dialog;
    }

    auto* dialog =
        new UpdateDialog(
            m_service,
            m_startupComplete,
            m_startupComplete ? m_window.data() : nullptr,
            SettingsManager::instance().skippedUpdateVersion()
            );

    dialog->setAttribute(
        Qt::WA_DeleteOnClose
        );
    dialog->setProperty(
        "automaticUpdatePrompt",
        automaticPrompt
        );

    m_dialog =
        dialog;

    connect(
        dialog,
        &UpdateDialog::skipVersionRequested,
        this,
        &UpdateController::skipVersion
        );
    connect(
        dialog,
        &UpdateDialog::unskipVersionRequested,
        this,
        &UpdateController::unskipVersion
        );

    connect(
        dialog,
        &QDialog::finished,
        this,
        [this, dialog](int)
        {
            if (
                dialog->property(
                    "automaticUpdatePrompt"
                    ).toBool()
                )
            {
                m_automaticPromptSuppressed =
                    true;
            }
        }
        );

    connect(
        dialog,
        &QObject::destroyed,
        this,
        [this]()
        {
            m_dialog =
                nullptr;
        }
        );

    return dialog;
}
