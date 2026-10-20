#include "next/application/manual_update_check_use_case.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
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
    .artifact = {
        .fileName = UpdateArtifactFileName("ClassMngr-2.0.0.exe"),
        .sha256 = UpdateArtifactSha256(" sha256:MiXeD \t\n")
    },
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
    static_assert(std::is_default_constructible_v<UpdateArtifactFileName>);
    static_assert(std::is_constructible_v<UpdateArtifactFileName, std::string>);
    static_assert(!std::is_convertible_v<std::string, UpdateArtifactFileName>);
    static_assert(!std::is_convertible_v<UpdateArtifactFileName, std::string>);
    static_assert(!std::is_convertible_v<std::int64_t, UpdateArtifactSizeBytes>);
    static_assert(!std::is_convertible_v<UpdateArtifactSizeBytes, std::int64_t>);
    static_assert(!std::is_convertible_v<
        std::int64_t, UpdateCheckUtcMilliseconds>);
    static_assert(!std::is_convertible_v<
        UpdateCheckUtcMilliseconds, std::int64_t>);

    const UpdateArtifactFileName defaultFileName;
    if (!require(defaultFileName.rawValue().empty()
            && ManualUpdateArtifact{}.fileName.rawValue().empty()
            && UpdateArtifactFileName(std::string{})
                == UpdateArtifactFileName(std::string{})
            && UpdateArtifactFileName("ClassMngr.zip")
                == UpdateArtifactFileName("ClassMngr.zip")
            && UpdateArtifactFileName("ClassMngr.zip")
                != UpdateArtifactFileName("classmngr.zip"),
            "Artifact filename value default or exact equality changed"))
    {
        return EXIT_FAILURE;
    }

    const UpdateCheckUtcMilliseconds defaultCheckedAt;
    if (!require(defaultCheckedAt.value() == 0
            && ManualUpdateCheckResult{}.checkedAtUtcMilliseconds.value() == 0
            && UpdateCheckUtcMilliseconds(-17) == UpdateCheckUtcMilliseconds(-17)
            && UpdateCheckUtcMilliseconds(-17) != UpdateCheckUtcMilliseconds(17),
            "Update-check timestamp default or exact equality changed"))
    {
        return EXIT_FAILURE;
    }

    const std::vector<std::int64_t> checkedAtValues{
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::min() + 1,
        -17,
        0,
        23,
        std::numeric_limits<std::int64_t>::max() - 1,
        std::numeric_limits<std::int64_t>::max()
    };
    for (const std::int64_t checkedAt : checkedAtValues)
    {
        FakePort port;
        ManualUpdateCheckUseCase useCase(port);
        std::vector<std::int64_t> observedCheckedAt;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            if (const auto* result = std::get_if<ManualUpdateCheckResult>(&event))
            {
                observedCheckedAt.push_back(
                    result->checkedAtUtcMilliseconds.value());
            }
        });
        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started,
                "Manual timestamp check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.finish(ManualUpdateCheckResult{
            .checkedAtUtcMilliseconds = UpdateCheckUtcMilliseconds(checkedAt)
        });
        const auto cached = port.snapshot().cachedResult;
        if (!require(observedCheckedAt == std::vector{checkedAt}
                && cached.has_value()
                && cached->checkedAtUtcMilliseconds.value() == checkedAt,
                "Manual result or cached state changed signed check milliseconds"))
        {
            return EXIT_FAILURE;
        }
    }

    const UpdateArtifactSizeBytes defaultSize;
    if (!require(defaultSize.value() == 0
            && ManualUpdateArtifact{}.sizeBytes.value() == 0
            && UpdateArtifactSizeBytes(-17) == UpdateArtifactSizeBytes(-17)
            && UpdateArtifactSizeBytes(-17) != UpdateArtifactSizeBytes(17),
            "Artifact size value default or exact equality changed"))
    {
        return EXIT_FAILURE;
    }

    const std::vector<std::int64_t> artifactSizes{
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::min() + 1,
        -17,
        0,
        23,
        std::numeric_limits<std::int64_t>::max() - 1,
        std::numeric_limits<std::int64_t>::max()
    };
    for (const std::int64_t sizeBytes : artifactSizes)
    {
        FakePort port;
        ManualUpdateCheckUseCase useCase(port);
        std::vector<std::int64_t> observedSizes;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            if (const auto* result = std::get_if<ManualUpdateCheckResult>(&event))
            {
                observedSizes.push_back(result->artifact.sizeBytes.value());
            }
        });
        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started,
                "Manual artifact-size check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.finish(ManualUpdateCheckResult{
            .artifact = {.sizeBytes = UpdateArtifactSizeBytes(sizeBytes)}
        });
        const auto cached = port.snapshot().cachedResult;
        if (!require(observedSizes == std::vector{sizeBytes}
                && cached.has_value()
                && cached->artifact.sizeBytes.value() == sizeBytes,
                "Manual result or cached state changed a signed artifact size"))
        {
            return EXIT_FAILURE;
        }
    }

    static_assert(!std::is_convertible_v<std::string, UpdateArtifactSha256>);
    static_assert(!std::is_convertible_v<UpdateArtifactSha256, std::string>);

    const UpdateArtifactSha256 defaultSha256;
    if (!require(defaultSha256.value().empty()
            && UpdateArtifactSha256("mixedCase") == UpdateArtifactSha256("mixedCase")
            && UpdateArtifactSha256("mixedCase") != UpdateArtifactSha256("mixedcase"),
            "SHA-256 value default or exact equality changed"))
    {
        return EXIT_FAILURE;
    }

    const std::vector<std::string> sha256Values{
        "",
        "a",
        "AbCdEf0123456789",
        " sha256:MiXeD \t\n",
        std::string{"x\0y", 3}
    };
    for (const auto& sha256 : sha256Values)
    {
        FakePort port;
        ManualUpdateCheckUseCase useCase(port);
        std::vector<std::string> observedSha256Values;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            if (const auto* result = std::get_if<ManualUpdateCheckResult>(&event))
            {
                observedSha256Values.push_back(result->artifact.sha256.value());
            }
        });

        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started,
                "Manual digest check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.finish(ManualUpdateCheckResult{
            .artifact = {.sha256 = UpdateArtifactSha256(sha256)}
        });
        if (!require(observedSha256Values == std::vector<std::string>{sha256}
                && port.snapshot().cachedResult.has_value()
                && port.snapshot().cachedResult->artifact.sha256.value() == sha256,
                "Manual result or cached state changed SHA-256 text"))
        {
            return EXIT_FAILURE;
        }
    }

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

    const std::vector<std::string> artifactFileNames{
        "",
        "ClassMngr-\xE2\x9C\x93.zip"
    };
    for (const auto& fileName : artifactFileNames)
    {
        FakePort port;
        ManualUpdateCheckUseCase useCase(port);
        std::vector<std::string> observedFileNames;
        useCase.setObserver([&](ManualUpdateCheckEvent event)
        {
            if (const auto* result = std::get_if<ManualUpdateCheckResult>(&event))
            {
                observedFileNames.push_back(
                    result->artifact.fileName.rawValue());
            }
        });

        if (!require(useCase.retry() == ManualUpdateCheckDispatch::Started,
                "Manual filename check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.finish(ManualUpdateCheckResult{
            .artifact = {
                .fileName = UpdateArtifactFileName(fileName)
            }
        });
        const auto cached = port.snapshot().cachedResult;
        if (!require(observedFileNames == std::vector<std::string>{fileName}
                && cached.has_value()
                && cached->artifact.fileName.rawValue() == fileName,
                "Manual result or cached state changed an opaque artifact filename"))
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
            || !require(std::get<ManualUpdateCheckResult>(events.front())
                    .artifact.sha256.value() == " sha256:MiXeD \t\n",
                "Fresh cached result changed SHA-256 text")
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
