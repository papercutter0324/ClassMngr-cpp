#pragma once

#include "next/application/skipped_update_version_policy.h"

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

namespace ClassMngr::Next::Application
{

struct StartupUpdateArtifact final
{
    std::string platformKey;
    std::string url;
    std::string fileName;
    std::string sha256;
    std::int64_t sizeBytes = 0;
};

struct StartupUpdateCheckResult final
{
    UpdateVersion currentVersion;
    UpdateVersion latestVersion;
    StartupUpdateArtifact artifact;
    std::string releaseDateIso;
    std::string releaseUrl;
    std::int64_t checkedAtUtcMilliseconds = 0;
    bool updateAvailable = false;
};

enum class StartupUpdateCheckErrorCode
{
    Configuration,
    Dispatch,
    Network,
    InvalidResponse,
    InvalidCurrentVersion
};

struct StartupUpdateCheckError final
{
    StartupUpdateCheckErrorCode code = StartupUpdateCheckErrorCode::Dispatch;
    std::string message;
};

using StartupUpdateCheckOutcome =
    std::variant<StartupUpdateCheckResult, StartupUpdateCheckError>;
using StartupUpdateCheckCompletion =
    std::function<void(StartupUpdateCheckOutcome)>;

enum class StartupUpdateCheckDispatch
{
    Started,
    Busy,
    Failed
};

class StartupUpdateCheckPort
{
public:
    virtual ~StartupUpdateCheckPort() = default;

    // Always force a new check. Started accepts one terminal completion;
    // Failed delivers one error completion, possibly before returning.
    // Busy accepts no work and delivers no completion. The port owns the
    // callback until completion or destruction. Destroying the port/service
    // suppresses delivery; it does not promise cancellation of network work.
    // Calls and completions use the same owning thread.
    [[nodiscard]] virtual StartupUpdateCheckDispatch check(
        StartupUpdateCheckCompletion completion
        ) = 0;
};

} // namespace ClassMngr::Next::Application
