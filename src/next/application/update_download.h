#pragma once

#include "next/application/update_artifact.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

namespace ClassMngr::Next::Application
{

enum class UpdateDownloadCleanupPolicy
{
    OrphansOnly,
    KeepOnlyArtifact,
    RemoveAll
};

struct UpdateDownloadPreparing final
{
    std::int64_t bytesAvailable = 0;
    std::int64_t bytesTotal = 0;
};

struct UpdateDownloadStarted final
{
};

struct UpdateDownloadProgress final
{
    std::int64_t bytesReceived = 0;
    std::int64_t bytesTotal = 0;
};

struct UpdateDownloadPaused final
{
    std::int64_t bytesReceived = 0;
    std::int64_t bytesTotal = 0;
};

struct UpdateDownloadVerifying final
{
};

struct UpdateDownloadSucceeded final
{
    std::string filePath;
};

struct UpdateDownloadFailed final
{
    std::string message;
};

struct UpdateDownloadDiscarded final
{
};

using UpdateDownloadEvent = std::variant<
    UpdateDownloadPreparing,
    UpdateDownloadStarted,
    UpdateDownloadProgress,
    UpdateDownloadPaused,
    UpdateDownloadVerifying,
    UpdateDownloadSucceeded,
    UpdateDownloadFailed,
    UpdateDownloadDiscarded
    >;
using UpdateDownloadObserver = std::function<void(UpdateDownloadEvent)>;

// The runtime owns this port longer than each dialog. Clearing the observer
// detaches a presentation; implementations must also tolerate late internal
// work without invoking an observer that has been cleared.
class UpdateDownloadPort
{
public:
    virtual ~UpdateDownloadPort() = default;

    virtual void setObserver(UpdateDownloadObserver observer) = 0;
    virtual void startOrResume(const UpdateArtifact& artifact) = 0;

    // Pause retains the partial payload and its resume metadata. Discard is a
    // separate, destructive user action.
    virtual void pause() = 0;
    virtual void discard() = 0;

    [[nodiscard]] virtual bool isBusy() const = 0;
    [[nodiscard]] virtual bool hasResumableDownload(
        const UpdateArtifact& artifact
        ) const = 0;
    [[nodiscard]] virtual std::int64_t resumableBytes(
        const UpdateArtifact& artifact
        ) const = 0;
    [[nodiscard]] virtual bool hasCompletedDownload(
        const UpdateArtifact& artifact
        ) const = 0;

    virtual void cleanup(
        UpdateDownloadCleanupPolicy policy,
        std::optional<UpdateArtifact> currentArtifact = std::nullopt
        ) = 0;
};

} // namespace ClassMngr::Next::Application
