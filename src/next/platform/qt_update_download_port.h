#pragma once

#include "next/application/update_download.h"

#include <QObject>

#include <memory>
#include <optional>

namespace ClassMngr::Next::Platform
{

// Qt/Core adapter for the application update-download boundary. It owns the
// existing downloader so verification, resume validation, and cleanup policy
// remain centralized in the updater engine.
class QtUpdateDownloadPort final : public QObject,
                                  public Application::UpdateDownloadPort
{
public:
    explicit QtUpdateDownloadPort(QObject* parent = nullptr);
    ~QtUpdateDownloadPort() override;

    void setObserver(Application::UpdateDownloadObserver observer) override;
    void startOrResume(
        const Application::UpdateArtifact& artifact
        ) override;
    void pause() override;
    void discard() override;
    [[nodiscard]] bool isBusy() const override;
    [[nodiscard]] bool hasResumableDownload(
        const Application::UpdateArtifact& artifact
        ) const override;
    [[nodiscard]] std::int64_t resumableBytes(
        const Application::UpdateArtifact& artifact
        ) const override;
    [[nodiscard]] bool hasCompletedDownload(
        const Application::UpdateArtifact& artifact
        ) const override;
    void cleanup(
        Application::UpdateDownloadCleanupPolicy policy,
        std::optional<Application::UpdateArtifact> currentArtifact = std::nullopt
        ) override;

private:
    struct Impl;
    void publish(Application::UpdateDownloadEvent event);

    std::unique_ptr<Impl> m_impl;
};

} // namespace ClassMngr::Next::Platform
