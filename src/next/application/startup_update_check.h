#pragma once

#include "next/application/skipped_update_version_policy.h"
#include "next/application/update_artifact_platform_key.h"
#include "next/application/update_artifact_sha256.h"
#include "next/application/update_artifact_size_bytes.h"
#include "next/application/update_check_utc_milliseconds.h"

#include <functional>
#include <string>
#include <variant>

namespace ClassMngr::Next::Application
{

struct StartupUpdateArtifact final
{
    UpdateArtifactPlatformKey platformKey;
    std::string url;
    std::string fileName;
    UpdateArtifactSha256 sha256;
    UpdateArtifactSizeBytes sizeBytes;
};

struct StartupUpdateCheckResult final
{
    UpdateVersion currentVersion;
    UpdateVersion latestVersion;
    StartupUpdateArtifact artifact;
    std::string releaseDateIso;
    std::string releaseUrl;
    UpdateCheckUtcMilliseconds checkedAtUtcMilliseconds;
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
