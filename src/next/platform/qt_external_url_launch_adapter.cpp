#include "next/platform/qt_external_url_launch_adapter.h"

#include <QDesktopServices>

namespace ClassMngr::Next::Platform
{

bool QtExternalUrlLaunchAdapter::openUrl(const QUrl& url) const
{
    return QDesktopServices::openUrl(url);
}

} // namespace ClassMngr::Next::Platform
