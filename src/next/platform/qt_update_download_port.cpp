#include "next/platform/qt_update_download_port.h"

#include "core/updater/update_downloader.h"

#include <QByteArray>
#include <QUrl>

#include <utility>

namespace ClassMngr::Next::Platform
{

struct QtUpdateDownloadPort::Impl final
{
    UpdateDownloader* downloader = nullptr;
    Application::UpdateDownloadObserver observer;
};

namespace
{

UpdateArtifact toCoreArtifact(
    const Application::UpdateArtifact& artifact
    )
{
    return {
        .platformKey = QString::fromStdString(artifact.platformKey.value()),
        .url = QUrl::fromEncoded(QByteArray::fromStdString(artifact.url.value())),
        .fileName = QString::fromStdString(artifact.fileName.rawValue()),
        .sha256 = QString::fromStdString(artifact.sha256.value()),
        .sizeBytes = artifact.sizeBytes.value()
    };
}

UpdateDownloader::CleanupMode toCoreCleanupMode(
    const Application::UpdateDownloadCleanupPolicy policy
    )
{
    switch (policy)
    {
    case Application::UpdateDownloadCleanupPolicy::OrphansOnly:
        return UpdateDownloader::CleanupMode::OrphansOnly;
    case Application::UpdateDownloadCleanupPolicy::KeepOnlyArtifact:
        return UpdateDownloader::CleanupMode::KeepOnlyArtifact;
    case Application::UpdateDownloadCleanupPolicy::RemoveAll:
        return UpdateDownloader::CleanupMode::RemoveAll;
    }
    return UpdateDownloader::CleanupMode::OrphansOnly;
}

} // namespace

QtUpdateDownloadPort::QtUpdateDownloadPort(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->downloader = new UpdateDownloader(this);
    connect(m_impl->downloader, &UpdateDownloader::downloadPreparing, this,
        [this](const qint64 bytesAvailable, const qint64 bytesTotal)
        {
            publish(Application::UpdateDownloadPreparing{
                bytesAvailable, bytesTotal});
        });
    connect(m_impl->downloader, &UpdateDownloader::downloadStarted, this,
        [this] { publish(Application::UpdateDownloadStarted{}); });
    connect(m_impl->downloader, &UpdateDownloader::downloadProgress, this,
        [this](const qint64 bytesReceived, const qint64 bytesTotal)
        {
            publish(Application::UpdateDownloadProgress{
                bytesReceived, bytesTotal});
        });
    connect(m_impl->downloader, &UpdateDownloader::downloadPaused, this,
        [this](const qint64 bytesReceived, const qint64 bytesTotal)
        {
            publish(Application::UpdateDownloadPaused{
                bytesReceived, bytesTotal});
        });
    connect(m_impl->downloader, &UpdateDownloader::downloadVerifying, this,
        [this] { publish(Application::UpdateDownloadVerifying{}); });
    connect(m_impl->downloader, &UpdateDownloader::downloadSucceeded, this,
        [this](const QString& filePath)
        {
            publish(Application::UpdateDownloadSucceeded{
                filePath.toStdString()});
        });
    connect(m_impl->downloader, &UpdateDownloader::downloadFailed, this,
        [this](const QString& message)
        {
            publish(Application::UpdateDownloadFailed{
                message.toStdString()});
        });
    connect(m_impl->downloader, &UpdateDownloader::downloadDiscarded, this,
        [this] { publish(Application::UpdateDownloadDiscarded{}); });
}

QtUpdateDownloadPort::~QtUpdateDownloadPort()
{
    m_impl->observer = {};
    delete m_impl->downloader;
    m_impl->downloader = nullptr;
}

void QtUpdateDownloadPort::setObserver(
    Application::UpdateDownloadObserver observer
    )
{
    m_impl->observer = std::move(observer);
}

void QtUpdateDownloadPort::startOrResume(
    const Application::UpdateArtifact& artifact
    )
{
    m_impl->downloader->download(toCoreArtifact(artifact));
}

void QtUpdateDownloadPort::pause()
{
    m_impl->downloader->pause();
}

void QtUpdateDownloadPort::discard()
{
    m_impl->downloader->discard();
}

bool QtUpdateDownloadPort::isBusy() const
{
    return m_impl->downloader->isBusy();
}

bool QtUpdateDownloadPort::hasResumableDownload(
    const Application::UpdateArtifact& artifact
    ) const
{
    return m_impl->downloader->hasResumableDownload(toCoreArtifact(artifact));
}

std::int64_t QtUpdateDownloadPort::resumableBytes(
    const Application::UpdateArtifact& artifact
    ) const
{
    return m_impl->downloader->resumableBytes(toCoreArtifact(artifact));
}

bool QtUpdateDownloadPort::hasCompletedDownload(
    const Application::UpdateArtifact& artifact
    ) const
{
    return m_impl->downloader->hasCompletedDownload(toCoreArtifact(artifact));
}

void QtUpdateDownloadPort::cleanup(
    const Application::UpdateDownloadCleanupPolicy policy,
    std::optional<Application::UpdateArtifact> currentArtifact
    )
{
    std::optional<UpdateArtifact> mappedArtifact;
    if (currentArtifact)
    {
        mappedArtifact = toCoreArtifact(*currentArtifact);
    }
    UpdateDownloader::cleanupDownloads(
        toCoreCleanupMode(policy), mappedArtifact);
}

void QtUpdateDownloadPort::publish(Application::UpdateDownloadEvent event)
{
    const auto observer = m_impl->observer;
    if (observer)
    {
        observer(std::move(event));
    }
}

} // namespace ClassMngr::Next::Platform
