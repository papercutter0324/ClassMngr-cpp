#include "next/application/startup_update_check_use_case.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

class FakePort final : public StartupUpdateCheckPort
{
public:
    StartupUpdateCheckDispatch dispatch = StartupUpdateCheckDispatch::Started;
    StartupUpdateCheckCompletion pending;
    std::optional<StartupUpdateCheckOutcome> inlineOutcome;
    int calls = 0;

    StartupUpdateCheckDispatch check(StartupUpdateCheckCompletion completion) override
    {
        ++calls;
        pending = std::move(completion);
        if (inlineOutcome)
        {
            pending(*inlineOutcome);
            pending(*inlineOutcome); // A duplicate must never reach the caller.
        }
        return dispatch;
    }
};

constexpr StartupUpdateCheckInput Eligible{
    .startupComplete = true,
    .checkOnStartup = true,
    .preferences = {.automaticChecksEnabled = true},
    .hasReleasesApiUrl = true
};

bool require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    static_assert(!std::is_convertible_v<std::int64_t, UpdateArtifactSizeBytes>);
    static_assert(!std::is_convertible_v<UpdateArtifactSizeBytes, std::int64_t>);

    const UpdateArtifactSizeBytes defaultSize;
    if (!require(defaultSize.value() == 0
            && StartupUpdateArtifact{}.sizeBytes.value() == 0
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
        StartupUpdateCheckUseCase useCase(port);
        bool preserved = false;
        if (!require(useCase.start(Eligible, [&](StartupUpdateCheckOutcome outcome)
                {
                    const auto* result = std::get_if<StartupUpdateCheckResult>(&outcome);
                    preserved = result
                        && result->artifact.sizeBytes.value() == sizeBytes;
                }) == StartupUpdateCheckDisposition::Started,
                "Startup artifact-size check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.pending(StartupUpdateCheckResult{
            .artifact = {.sizeBytes = UpdateArtifactSizeBytes(sizeBytes)}
        });
        if (!require(preserved,
                "Startup result changed a signed artifact size"))
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
        StartupUpdateCheckUseCase useCase(port);
        bool preserved = false;
        const auto disposition = useCase.start(Eligible,
            [&](StartupUpdateCheckOutcome outcome)
            {
                const auto* result = std::get_if<StartupUpdateCheckResult>(&outcome);
                preserved = result && result->artifact.sha256.value() == sha256;
            });
        if (!require(disposition == StartupUpdateCheckDisposition::Started,
                "Startup digest check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.pending(StartupUpdateCheckResult{
            .artifact = {.sha256 = UpdateArtifactSha256(sha256)}
        });
        if (!require(preserved, "Startup result changed SHA-256 text"))
        {
            return EXIT_FAILURE;
        }
    }

    // Platform keys are opaque bytes at the Application boundary. Preserve
    // empty, unknown, case-distinct, alias, malformed UTF-8, and embedded-NUL
    // values through successful startup results.
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
        StartupUpdateCheckUseCase useCase(port);
        bool preserved = false;
        const auto disposition = useCase.start(Eligible,
            [&](StartupUpdateCheckOutcome outcome)
            {
                const auto* result = std::get_if<StartupUpdateCheckResult>(&outcome);
                preserved = result
                    && result->artifact.platformKey.value() == platformKey;
            });
        if (!require(disposition == StartupUpdateCheckDisposition::Started,
                "Opaque-key startup check did not start"))
        {
            return EXIT_FAILURE;
        }

        port.pending(StartupUpdateCheckResult{
            .artifact = {
                .platformKey = UpdateArtifactPlatformKey(platformKey)
            }
        });
        if (!require(preserved, "Startup result changed an opaque platform key"))
        {
            return EXIT_FAILURE;
        }
    }

    // Each gate is independently required, and a corrected ineligible input
    // must leave the one eligible dispatch available.
    for (int missingGate = 0; missingGate < 4; ++missingGate)
    {
        FakePort port;
        StartupUpdateCheckUseCase useCase(port);
        auto input = Eligible;
        switch (missingGate)
        {
        case 0: input.startupComplete = false; break;
        case 1: input.checkOnStartup = false; break;
        case 2: input.preferences.automaticChecksEnabled = false; break;
        case 3: input.hasReleasesApiUrl = false; break;
        }
        if (!require(useCase.start(input, {}) == StartupUpdateCheckDisposition::NotEligible
                && port.calls == 0, "Ineligible startup dispatched")
            || !require(useCase.start(Eligible, {}) == StartupUpdateCheckDisposition::Started
                && port.calls == 1, "Corrected eligibility could not dispatch")
            || !require(useCase.start(Eligible, {}) == StartupUpdateCheckDisposition::AlreadyAttempted
                && port.calls == 1, "Startup dispatched more than once"))
        {
            return EXIT_FAILURE;
        }
    }

    for (const bool updateAvailable : {false, true})
    {
        FakePort port;
        StartupUpdateCheckUseCase useCase(port);
        int completions = 0;
        bool mapped = false;
        if (!require(useCase.start(Eligible, [&](StartupUpdateCheckOutcome outcome)
                {
                    ++completions;
                    const auto* result = std::get_if<StartupUpdateCheckResult>(&outcome);
                    mapped = result && result->updateAvailable == updateAvailable
                        && result->latestVersion == UpdateVersion{2, 3, 4}
                        && result->artifact.fileName == "installer.exe";
                }) == StartupUpdateCheckDisposition::Started && completions == 0,
                "Started must wait for terminal completion"))
        {
            return EXIT_FAILURE;
        }
        const StartupUpdateCheckResult result{
            .latestVersion = {2, 3, 4},
            .artifact = {.fileName = "installer.exe"},
            .updateAvailable = updateAvailable
        };
        port.pending(result);
        port.pending(StartupUpdateCheckError{StartupUpdateCheckErrorCode::Network, "late"});
        if (!require(completions == 1 && mapped, "Update/no-update result was lost or duplicated"))
        {
            return EXIT_FAILURE;
        }
    }

    for (const auto code : {StartupUpdateCheckErrorCode::Network,
            StartupUpdateCheckErrorCode::InvalidResponse,
            StartupUpdateCheckErrorCode::InvalidCurrentVersion})
    {
        FakePort port;
        StartupUpdateCheckUseCase useCase(port);
        int completions = 0;
        bool mapped = false;
        const auto dispatch = useCase.start(Eligible, [&](StartupUpdateCheckOutcome outcome)
            {
                ++completions;
                const auto* error = std::get_if<StartupUpdateCheckError>(&outcome);
                mapped = error && error->code == code && error->message == "failure";
            });
        port.pending(StartupUpdateCheckError{code, "failure"});
        port.pending(StartupUpdateCheckResult{});
        if (!require(dispatch == StartupUpdateCheckDisposition::Started
                && completions == 1 && mapped, "Async error was lost or duplicated"))
        {
            return EXIT_FAILURE;
        }
    }

    for (const auto dispatch : {StartupUpdateCheckDispatch::Failed,
            StartupUpdateCheckDispatch::Busy})
    {
        FakePort port;
        port.dispatch = dispatch;
        port.inlineOutcome = StartupUpdateCheckError{
            StartupUpdateCheckErrorCode::Configuration, "invalid URL"};
        StartupUpdateCheckUseCase useCase(port);
        int completions = 0;
        bool mapped = false;
        const auto disposition = useCase.start(Eligible, [&](StartupUpdateCheckOutcome outcome)
            {
                ++completions;
                const auto* error = std::get_if<StartupUpdateCheckError>(&outcome);
                mapped = error && error->code == StartupUpdateCheckErrorCode::Configuration;
            });
        const bool failed = dispatch == StartupUpdateCheckDispatch::Failed;
        if (!require(disposition == (failed ? StartupUpdateCheckDisposition::Failed
                    : StartupUpdateCheckDisposition::Busy)
                && completions == (failed ? 1 : 0) && (!failed || mapped),
                "Synchronous failure/busy delivery contract violated")
            || !require(useCase.start(Eligible, {}) == StartupUpdateCheckDisposition::AlreadyAttempted
                && port.calls == 1, "Busy/failed eligible attempt did not consume one-shot"))
        {
            return EXIT_FAILURE;
        }
    }

    FakePort port;
    int completions = 0;
    {
        StartupUpdateCheckUseCase useCase(port);
        if (!require(useCase.start(Eligible, [&](StartupUpdateCheckOutcome) { ++completions; })
                == StartupUpdateCheckDisposition::Started, "Lifetime test did not start"))
        {
            return EXIT_FAILURE;
        }
    }
    port.pending(StartupUpdateCheckResult{});
    return require(completions == 0, "Destroyed use case delivered a callback")
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
