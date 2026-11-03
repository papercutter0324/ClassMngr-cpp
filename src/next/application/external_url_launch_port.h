#pragma once

#include <QUrl>

namespace ClassMngr::Next::Application
{

class ExternalUrlLaunchPort
{
public:
    virtual ~ExternalUrlLaunchPort() = default;

    [[nodiscard]] virtual bool openUrl(const QUrl& url) const = 0;
};

} // namespace ClassMngr::Next::Application
