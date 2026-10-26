#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

enum class NotificationSeverity
{
    Information,
    Warning,
    Error
};

// Notification text is UTF-8. The request contains application meaning only;
// presentation lifetime and layout belong to the platform adapter.
struct NotificationRequest final
{
    std::string message;
    NotificationSeverity severity = NotificationSeverity::Information;
};

class NotificationPort
{
public:
    virtual ~NotificationPort() = default;

    // Replaces any active notice; the presenter applies its own finite
    // display lifetime.
    [[nodiscard]] virtual Domain::Result<void> show(
        const NotificationRequest& request
        ) = 0;

    // Clears the active notice, if any.
    [[nodiscard]] virtual Domain::Result<void> clear() = 0;
};

} // namespace ClassMngr::Next::Application
