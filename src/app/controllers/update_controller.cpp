#include "update_controller.h"

#include "app/mainwindow.h"
#include "core/updater/update_configuration.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QAction>

UpdateController::UpdateController(
    MainWindow* window,
    QObject* parent
    )
    : QObject(parent)
    , m_window(window)
{
}

void UpdateController::connectActions(
    ActionRegistry& actions
    )
{
    if (!actions.checkForUpdates)
    {
        return;
    }

    connect(
        actions.checkForUpdates,
        &QAction::triggered,
        this,
        &UpdateController::showManualUpdateDialog
        );
}

void UpdateController::maybeCheckOnStartup()
{
    if (m_startupCheckStarted)
    {
        return;
    }

    const UpdateConfiguration configuration =
        UpdateConfiguration::fromBuild();

    if (
        !configuration.checkOnStartup
        || !configuration.hasManifestUrl()
        )
    {
        return;
    }

    m_startupCheckStarted =
        true;

    auto* service =
        new UpdateService(
            configuration,
            this
            );

    connect(
        service,
        &UpdateService::checkSucceeded,
        this,
        [this, service](const UpdateCheckResult& result)
        {
            service->deleteLater();

            if (result.updateAvailable)
            {
                showUpdateDialogForResult(result);
            }
        }
        );

    connect(
        service,
        &UpdateService::checkFailed,
        service,
        &QObject::deleteLater
        );

    service->checkForUpdates();
}

void UpdateController::showManualUpdateDialog()
{
    auto* dialog =
        new UpdateDialog(m_window);

    dialog->setAttribute(
        Qt::WA_DeleteOnClose
        );
    dialog->show();
    dialog->beginCheck();
}

void UpdateController::showUpdateDialogForResult(
    const UpdateCheckResult& result
    )
{
    auto* dialog =
        new UpdateDialog(m_window);

    dialog->setAttribute(
        Qt::WA_DeleteOnClose
        );
    dialog->showAvailableUpdate(result);
    dialog->show();
}
