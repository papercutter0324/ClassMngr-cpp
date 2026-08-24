#include "update_controller.h"

#include "app/mainwindow.h"
#include "core/settingsmanager.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"
#include "core/updater/version.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/update_dialog.h"
#include "ui/shared/widgets/splash/splashscreen.h"

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

    UpdateDownloader::cleanupDownloads(
        UpdateDownloader::CleanupMode::OrphansOnly
        );

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

void UpdateController::setSplashScreen(
    SplashScreen* splash
    )
{
    m_splash =
        splash;
}

void UpdateController::startStartupCheck()
{
    if (m_startupCheckStarted || !m_service)
    {
        return;
    }

    const UpdateConfiguration configuration =
        m_service->configuration();

    if (
        !automaticChecksEnabled()
        || !configuration.hasReleasesApiUrl()
        )
    {
        return;
    }

    m_startupCheckStarted =
        true;
    m_service->checkForUpdates(
        UpdateService::CheckPolicy::Force
        );
}

void UpdateController::setStartupComplete()
{
    m_startupComplete =
        true;
    m_splash =
        nullptr;

    if (m_dialog)
    {
        // An automatic prompt can appear while the splash is still visible,
        // before the main window is ready to own it.  Once the main window is
        // shown, make that dialog its child window so showing the main window
        // cannot place itself above the update prompt.
        if (
            m_window
            && m_dialog->parentWidget() != m_window
            )
        {
            const bool wasVisible =
                m_dialog->isVisible();
            const Qt::WindowFlags windowFlags =
                m_dialog->windowFlags();

            m_dialog->setParent(
                m_window,
                windowFlags
                );

            // QWidget::setParent() hides a visible widget.  Restore the
            // prompt after assigning its owner, so it remains in front of
            // the main window during the startup handoff.
            if (wasVisible)
            {
                m_dialog->show();
                m_dialog->raise();
                m_dialog->activateWindow();
            }
        }

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

    yieldSplashToDialog();

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

            restoreSplashAfterDialog();
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

void UpdateController::yieldSplashToDialog()
{
    if (!m_splash || m_startupComplete)
    {
        return;
    }

    m_splash->setWindowFlag(
        Qt::WindowStaysOnTopHint,
        false
        );
    m_splash->show();
    m_splash->lower();
}

void UpdateController::restoreSplashAfterDialog()
{
    if (!m_splash || m_startupComplete)
    {
        return;
    }

    m_splash->setWindowFlag(
        Qt::WindowStaysOnTopHint,
        true
        );
    m_splash->show();
    m_splash->raise();
    m_splash->activateWindow();
}
