#include "next/application/notification_port.h"

#include <iostream>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class FakeNotificationPort final : public NotificationPort
{
public:
    Domain::Result<void> show(
        const NotificationRequest& request
        ) override
    {
        ++showCalls;
        lastRequest = request;
        return showResult;
    }

    Domain::Result<void> clear() override
    {
        ++clearCalls;
        return clearResult;
    }

    int showCalls = 0;
    int clearCalls = 0;
    NotificationRequest lastRequest;
    Domain::Result<void> showResult = Domain::Result<void>::success();
    Domain::Result<void> clearResult = Domain::Result<void>::success();
};

bool notificationRequestsKeepTextAndTypedSeverity()
{
    const NotificationRequest defaultRequest{
        .message = "The report was saved.",
    };
    const NotificationRequest warningRequest{
        .message = "保存しました 📘",
        .severity = NotificationSeverity::Warning
    };
    const NotificationRequest errorRequest{
        .message = "Could not save the report.",
        .severity = NotificationSeverity::Error
    };

    FakeNotificationPort port;
    const auto defaultResult = port.show(defaultRequest);
    const auto warningResult = port.show(warningRequest);
    const auto errorResult = port.show(errorRequest);
    const auto clearResult = port.clear();

    return defaultRequest.severity == NotificationSeverity::Information
        && defaultResult.hasValue()
        && warningResult.hasValue()
        && errorResult.hasValue()
        && clearResult.hasValue()
        && port.showCalls == 3
        && port.clearCalls == 1
        && port.lastRequest.message == errorRequest.message
        && port.lastRequest.severity == NotificationSeverity::Error;
}

bool technicalPresentationErrorsStayInDomainResult()
{
    FakeNotificationPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "The notification presenter is unavailable.",
        .recoverable = false
    };
    port.showResult = Domain::Result<void>::failure(expected);

    const auto failed = port.show(NotificationRequest{
        .message = "Saved"
    });
    const auto cleared = port.clear();
    return !failed.hasValue()
        && failed.error() == expected
        && cleared.hasValue();
}

} // namespace

int main()
{
    if (!notificationRequestsKeepTextAndTypedSeverity()
        || !technicalPresentationErrorsStayInDomainResult())
    {
        std::cerr << "Next notification-port tests failed.\n";
        return 1;
    }
    std::cout << "Next notification-port tests passed.\n";
    return 0;
}
