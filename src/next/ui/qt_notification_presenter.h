#pragma once

#include "next/application/notification_port.h"

#include <QPointer>

#include <chrono>

class QStatusBar;

namespace ClassMngr::Next::Ui
{

// Transient Qt implementation of the Qt-free notification port. The status
// bar is borrowed and must outlive the presenter. Timeout policy stays here.
class QtNotificationPresenter final : public Application::NotificationPort
{
public:
    static constexpr std::chrono::milliseconds DefaultTimeout{5000};

    explicit QtNotificationPresenter(
        QStatusBar& statusBar,
        std::chrono::milliseconds timeout = DefaultTimeout
        );
    ~QtNotificationPresenter() override;

    QtNotificationPresenter(const QtNotificationPresenter&) = delete;
    QtNotificationPresenter& operator=(const QtNotificationPresenter&) = delete;

    [[nodiscard]] Domain::Result<void> show(
        const Application::NotificationRequest& request
        ) override;

    [[nodiscard]] Domain::Result<void> clear() override;

private:
    QPointer<QStatusBar> m_statusBar;
    std::chrono::milliseconds m_timeout;
};

} // namespace ClassMngr::Next::Ui
