#pragma once

#include "next/application/external_url_launch_port.h"

namespace ClassMngr::Next::Platform
{

class QtExternalUrlLaunchAdapter final
    : public Application::ExternalUrlLaunchPort
{
public:
    [[nodiscard]] bool openUrl(const QUrl& url) const override;
};

} // namespace ClassMngr::Next::Platform
