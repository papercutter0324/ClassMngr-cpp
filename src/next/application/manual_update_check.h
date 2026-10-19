#pragma once

#include "next/application/skipped_update_version_policy.h"
#include "next/application/update_artifact_platform_key.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

namespace ClassMngr::Next::Application
{

struct ManualUpdateArtifact final
{
    UpdateArtifactPlatformKey platformKey;
    std::string url;
    std::string fileName;
    std::string sha256;
    std::int64_t sizeBytes = 0;
};

struct ManualUpdateCheckResult final
{
    UpdateVersion currentVersion;
    UpdateVersion latestVersion;
    ManualUpdateArtifact artifact;
    std::string releaseDateIso;
    std::string releaseUrl;
    std::int64_t checkedAtUtcMilliseconds = 0;
    bool updateAvailable = false;
};

enum class ManualUpdateCheckErrorCode
{
    Configuration,
    Dispatch,
    Network,
    InvalidResponse,
    InvalidCurrentVersion
};

struct ManualUpdateCheckError final
{
    ManualUpdateCheckErrorCode code = ManualUpdateCheckErrorCode::Dispatch;
    std::string message;
};

using ManualUpdateCheckOutcome =
    std::variant<ManualUpdateCheckResult, ManualUpdateCheckError>;
using ManualUpdateCheckCompletion =
    std::function<void(ManualUpdateCheckOutcome)>;

struct ManualUpdateCheckStarted final
{
};

using ManualUpdateCheckEvent = std::variant<
    ManualUpdateCheckStarted,
    ManualUpdateCheckResult,
    ManualUpdateCheckError
    >;
using ManualUpdateCheckObserver =
    std::function<void(ManualUpdateCheckEvent)>;

enum class ManualUpdateCheckPolicy
{
    IfStale,
    Force
};

enum class ManualUpdateCheckDispatch
{
    Started,
    FreshCache,
    Busy,
    Failed
};

struct ManualUpdateCheckSnapshot final
{
    std::optional<ManualUpdateCheckResult> cachedResult;
    bool resultFresh = false;
    bool requestInProgress = false;
};

class ManualUpdateCheckPort
{
public:
    virtual ~ManualUpdateCheckPort() = default;

    [[nodiscard]] virtual ManualUpdateCheckSnapshot snapshot() const = 0;

    // A fresh IfStale check returns FreshCache without a completion. Busy also
    // accepts no work. Started and Failed each deliver exactly one completion;
    // Failed may deliver it before returning. The service owns the request and
    // destroying this port suppresses its callbacks without canceling work.
    [[nodiscard]] virtual ManualUpdateCheckDispatch check(
        ManualUpdateCheckPolicy policy,
        ManualUpdateCheckCompletion completion
        ) = 0;

    // Reports check events initiated outside this port, such as the automatic
    // startup check. The observer never owns or cancels those requests.
    virtual void setObserver(ManualUpdateCheckObserver observer) = 0;
};

} // namespace ClassMngr::Next::Application
