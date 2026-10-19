#include "next/application/manual_update_check_use_case.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

class FakePort final : public ManualUpdateCheckPort
{
public:
    ManualUpdateCheckSnapshot current;
    ManualUpdateCheckDispatch nextDispatch = ManualUpdateCheckDispatch::Started;
    std::optional<ManualUpdateCheckOutcome> inlineOutcome;
    ManualUpdateCheckCompletion pending;
    ManualUpdateCheckObserver observer;
    std::vector<ManualUpdateCheckPolicy> policies;

    [[nodiscard]] ManualUpdateCheckSnapshot snapshot() const override
    {
        return current;
    }

    [[nodiscard]] ManualUpdateCheckDispatch check(
        ManualUpdateCheckPolicy policy,
        ManualUpdateCheckCompletion completion
        ) override
    {
        policies.push_back(policy);
        if (nextDispatch == ManualUpdateCheckDispatch::Started
            || nextDispatch == ManualUpdateCheckDispatch::Failed)
        {
            pending = std::move(completion);
            current.requestInProgress = nextDispatch == ManualUpdateCheckDispatch::Started;
            if (inlineOutcome)
            {
                auto outcome = *inlineOutcome;
                if (const auto* result = std::get_if<ManualUpdateCheckResult>(&outcome))
                {
                    current.cachedResult = *result;
                    current.resultFresh = true;
                }
                current.requestInProgress = false;
                pending(outcome);
                pending(outcome); // Duplicate terminal delivery is ignored.
            }
        }
        return nextDispatch;
    }

    void setObserver(ManualUpdateCheckObserver value) override
    {
        observer = std::move(value);
    }

    void finish(ManualUpdateCheckOutcome outcome)
    {
        current.requestInProgress = false;
        if (const auto* result = std::get_if<ManualUpdateCheckResult>(&outcome))
        {
            current.cachedResult = *result;
            current.resultFresh = true;
        }
        pending(outcome);
        pending(outcome);
    }

    void emit(ManualUpdateCheckEvent event)
    {
        if (observer)
        {
            observer(std::move(event));
        }
    }
};

const ManualUpdateCheckResult CachedResult{
    .currentVersion = {1, 0, 0},
    .latestVersion = {2, 0, 0},
    .artifact = {.fileName = "ClassMngr-2.0.0.exe"},
    .updateAvailable = true
};

bool require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

std::vector<std::string> eventNames(const std::vector<ManualUpdateCheckEvent>& events)
{
    std::vector<std::string> names;
    for (const auto& event : events)
    {
        names.push_back(std::visit([](const auto& value) -> std::string
        {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, ManualUpdateCheckStarted>)
            {
                return "started";
            }
            else if constexpr (std::is_same_v<Value, ManualUpdateCheckResult>)
            {
                return "result";
            }
            else
            {
                return "error";
            }
        }, event));
    }
    return names;
}

} // namespace

int main()
{
    // Manual success results keep platform keys opaque, including empty,
    // unknown, case-distinct, alias, malformed UTF-8, and embedded-NUL bytes.
    const std::vector<std::string> platformKeys{
        "",
        "windows-x64",
        "Windows-x64",
        "linux-x86_64",
        "linux-x64",
        "unknown-architecture-key",
        std::string{"\xC3\x28", 2},
        std::string{"x\0y", 3}
    };
    for (const auto& platformKey : platformKeys)
    {
        FakePort port;
        ManualUpdateCheckUseCase useCase(port);
        std::vector<std::string> observedPlatformKeys;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            if (const auto* result = std::get_if<ManualUpdateCheckResult>(&event))
            {
                observedPlatformKeys.push_back(result->artifact.platformKey.value());
            }
        });

        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started,
                "Opaque-key manual check did not start"))
        {
            return EXIT_FAILURE;
        }

        ManualUpdateCheckResult result{
            .artifact = {
                .platformKey = UpdateArtifactPlatformKey(platformKey)
            }
        };
        port.finish(result);
        if (!require(observedPlatformKeys == std::vector<std::string>{platformKey},
                "Manual result changed or duplicated an opaque platform key"))
        {
            return EXIT_FAILURE;
        }
    }

    // Fresh cache is presented without dispatch; retry still forces a request.
    {
        FakePort port;
        port.current = {.cachedResult = CachedResult, .resultFresh = true};
        ManualUpdateCheckUseCase useCase(port);
        std::vector<ManualUpdateCheckEvent> events;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            events.push_back(std::move(event));
        });

        if (!require(useCase.open() == ManualUpdateCheckDispatch::FreshCache
                && port.policies.empty(), "Fresh open dispatched a request")
            || !require(events.size() == 1
                && std::holds_alternative<ManualUpdateCheckResult>(events.front()),
                "Fresh open did not present its cached result")
            || !require(useCase.retry() == ManualUpdateCheckDispatch::Started
                && port.policies == std::vector{ManualUpdateCheckPolicy::Force},
                "Retry did not force a fresh request")
            || !require(eventNames(events) == std::vector<std::string>{
                "result", "started"}, "Retry did not publish its started state"))
        {
            return EXIT_FAILURE;
        }

        port.finish(CachedResult);
        if (!require(eventNames(events) == std::vector<std::string>{
                "result", "started", "result"},
                "Async retry result was lost or delivered more than once")
            || !require(useCase.retry() == ManualUpdateCheckDispatch::Started
                && port.policies.size() == 2
                && port.policies.back() == ManualUpdateCheckPolicy::Force,
                "Manual check use case was not repeatable"))
        {
            return EXIT_FAILURE;
        }
    }

    // A stale cache is presented first, then refreshed with IfStale.
    {
        FakePort port;
        port.current = {.cachedResult = CachedResult, .resultFresh = false};
        ManualUpdateCheckUseCase useCase(port);
        std::vector<ManualUpdateCheckEvent> events;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            events.push_back(std::move(event));
        });

        if (!require(useCase.open() == ManualUpdateCheckDispatch::Started
                && port.policies == std::vector{ManualUpdateCheckPolicy::IfStale},
                "Stale open did not dispatch IfStale")
            || !require(eventNames(events) == std::vector<std::string>{
                "result", "started"}, "Stale open did not retain the cached result"))
        {
            return EXIT_FAILURE;
        }
        port.finish(ManualUpdateCheckError{
            ManualUpdateCheckErrorCode::Network, "network failure"});
        const auto* error = events.size() == 3
            ? std::get_if<ManualUpdateCheckError>(&events.back())
            : nullptr;
        if (!require(error && error->code == ManualUpdateCheckErrorCode::Network
                && error->message == "network failure",
                "Async manual failure was lost or changed"))
        {
            return EXIT_FAILURE;
        }
    }

    // Existing work is observed as Busy, not adopted; external terminal events
    // still reach the observer while the dialog is open.
    {
        FakePort port;
        port.current = {
            .cachedResult = CachedResult,
            .resultFresh = true,
            .requestInProgress = true
        };
        ManualUpdateCheckUseCase useCase(port);
        std::vector<ManualUpdateCheckEvent> events;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            events.push_back(std::move(event));
        });

        if (!require(useCase.open() == ManualUpdateCheckDispatch::Busy
                && port.policies.empty(), "Busy open dispatched or adopted a request")
            || !require(eventNames(events) == std::vector<std::string>{
                "result", "started"}, "Busy open lost cached/loading state"))
        {
            return EXIT_FAILURE;
        }

        port.current.requestInProgress = false;
        port.emit(ManualUpdateCheckStarted{});
        port.emit(CachedResult);
        if (!require(eventNames(events) == std::vector<std::string>{
                "result", "started", "result"},
                "External startup check events were not observed"))
        {
            return EXIT_FAILURE;
        }
    }

    // Synchronous failure is terminal exactly once, and a failed manual
    // attempt does not make later retries one-shot.
    {
        FakePort port;
        port.nextDispatch = ManualUpdateCheckDispatch::Failed;
        port.inlineOutcome = ManualUpdateCheckError{
            ManualUpdateCheckErrorCode::Configuration, "invalid URL"};
        ManualUpdateCheckUseCase useCase(port);
        std::vector<ManualUpdateCheckEvent> events;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            events.push_back(std::move(event));
        });

        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Failed
                && eventNames(events) == std::vector<std::string>{"error"},
                "Synchronous failure was lost, duplicated, or shown as loading"))
        {
            return EXIT_FAILURE;
        }

        port.nextDispatch = ManualUpdateCheckDispatch::Started;
        port.inlineOutcome.reset();
        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started
                && port.policies.size() == 2,
                "A failed manual check prevented a later retry"))
        {
            return EXIT_FAILURE;
        }
    }

    // The application contract synthesizes one typed dispatch failure if an
    // adapter returns Failed without a completion, and suppresses callbacks
    // after its lifetime ends.
    {
        FakePort port;
        port.nextDispatch = ManualUpdateCheckDispatch::Failed;
        ManualUpdateCheckUseCase useCase(port);
        std::vector<ManualUpdateCheckEvent> events;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            events.push_back(std::move(event));
        });
        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Failed
                && events.size() == 1
                && std::get<ManualUpdateCheckError>(events.front()).code
                    == ManualUpdateCheckErrorCode::Dispatch,
                "Missing synchronous error completion was not made visible"))
        {
            return EXIT_FAILURE;
        }
    }

    FakePort port;
    std::vector<ManualUpdateCheckEvent> events;
    {
        ManualUpdateCheckUseCase useCase(port);
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            events.push_back(std::move(event));
        });
        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started,
                "Lifetime check did not start"))
        {
            return EXIT_FAILURE;
        }
    }
    port.finish(CachedResult);
    if (!require(events.size() == 1 && !port.observer,
            "Destroyed use case delivered or retained a callback"))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
