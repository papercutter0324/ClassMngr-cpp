#include "next/platform/qt_update_package_action_port.h"

#include "core/updater/update_installer.h"

#include <QString>
#include <QtGlobal>

namespace ClassMngr::Next::Platform
{
namespace
{

Application::UpdatePackageActionResult fromStatus(const Status& status)
{
    return status
        ? Application::UpdatePackageActionResult::success()
        : Application::UpdatePackageActionResult::failure(
            status.error().toStdString());
}

} // namespace

Application::UpdatePackageActionKind QtUpdatePackageActionPort::primaryAction() const
{
#if defined(Q_OS_LINUX)
    return Application::UpdatePackageActionKind::RevealInFolder;
#else
    return Application::UpdatePackageActionKind::InstallOrLaunch;
#endif
}

Application::UpdatePackageActionResult QtUpdatePackageActionPort::launchInstaller(
    const std::string& filePath
    )
{
    return fromStatus(UpdateInstaller::launch(QString::fromStdString(filePath)));
}

Application::UpdatePackageActionResult QtUpdatePackageActionPort::revealInFolder(
    const std::string& filePath
    )
{
    return fromStatus(UpdateInstaller::revealInFolder(
        QString::fromStdString(filePath)));
}

} // namespace ClassMngr::Next::Platform
