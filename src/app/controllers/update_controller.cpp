#include "update_controller.h"

#include "app/mainwindow.h"
#include "core/resource_packs/resource_pack_configuration.h"
#include "core/resource_packs/resource_pack_update_service.h"
#include "core/updater/update_configuration.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QAction>
#include <QDebug>

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
    maybeCheckResourcePacksOnStartup();

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

void UpdateController::maybeCheckResourcePacksOnStartup()
{
    if (m_resourcePackCheckStarted)
    {
        return;
    }

    const ResourcePackConfiguration configuration =
        ResourcePackConfiguration::fromBuild();

    if (
        !configuration.checkOnStartup
        || !configuration.hasManifestUrl()
        )
    {
        return;
    }

    m_resourcePackCheckStarted =
        true;

    auto* service =
        new ResourcePackUpdateService(
            configuration,
            this
            );

    connect(
        service,
        &ResourcePackUpdateService::checkSucceeded,
        this,
        [service](const QStringList& stagedPackIds)
        {
            if (!stagedPackIds.isEmpty())
            {
                qInfo().noquote()
                    << tr("Resource-pack updates were downloaded and will be used after the next launch: %1")
                           .arg(stagedPackIds.join(QStringLiteral(", ")));
            }

            service->deleteLater();
        }
        );

    connect(
        service,
        &ResourcePackUpdateService::checkFailed,
        this,
        [service](const QString& message)
        {
            qWarning().noquote()
                << tr("Resource-pack update check failed: %1")
                       .arg(message);
            service->deleteLater();
        }
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
