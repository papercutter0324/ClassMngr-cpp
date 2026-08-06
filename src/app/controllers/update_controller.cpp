#include "update_controller.h"

#include "app/mainwindow.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_service.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/update_dialog.h"
#include "ui/shared/widgets/splash/splashscreen.h"

#include <QAction>
#include <QDialog>

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
            if (
                result.updateAvailable
                && !hasVisibleDialog()
                && !m_automaticPromptSuppressed
                )
            {
                showAutomaticUpdateDialog();
            }
        }
        );
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
        !configuration.checkOnStartup
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
            m_startupComplete ? m_window.data() : nullptr
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
