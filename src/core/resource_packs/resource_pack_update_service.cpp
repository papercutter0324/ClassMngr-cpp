#include "resource_pack_update_service.h"

#include "core/network/http_request_policy.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/updater/update_signature_verifier.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <utility>

namespace
{
constexpr qint64 MaximumManifestBytes = 1024 * 1024;

QString fetchKindName(
    ResourcePackUpdateService::FetchKind kind
    )
{
    return kind == ResourcePackUpdateService::FetchKind::Manifest
        ? QObject::tr("resource-pack manifest")
        : QObject::tr("resource-pack manifest signature");
}
}

ResourcePackUpdateService::ResourcePackUpdateService(
    ResourcePackConfiguration configuration,
    QObject* parent
    )
    : QObject(parent)
    , m_configuration(std::move(configuration))
{
}

bool ResourcePackUpdateService::isBusy() const
{
    return m_busy;
}

void ResourcePackUpdateService::checkForUpdates()
{
    if (m_busy)
    {
        return;
    }

    if (!m_configuration.hasManifestUrl())
    {
        fail(
            tr("Resource-pack manifest URL is not configured.")
            );
        return;
    }

    if (!isAllowedManifestUrl(m_configuration.manifestUrl))
    {
        fail(
            tr("Resource-pack manifest URL must use HTTPS.")
            );
        return;
    }

    if (
        m_configuration.requireSignature
        && m_configuration.publicKeyPem.trimmed().isEmpty()
        )
    {
        fail(
            tr("Resource-pack public key is not configured.")
            );
        return;
    }

    m_busy =
        true;
    m_manifestData.clear();
    m_signatureData.clear();
    m_pendingArtifacts.clear();
    m_stagedPackIds.clear();

    emit checkStarted();

    fetch(
        m_configuration.manifestUrl,
        FetchKind::Manifest
        );
}

void ResourcePackUpdateService::fetch(
    const QUrl& url,
    FetchKind kind
    )
{
    QNetworkRequest request(url);
    HttpRequestPolicy::applySafeRedirectPolicy(request);

    auto* reply =
        m_network.get(request);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, kind]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                fail(
                    tr("Unable to download %1: %2")
                        .arg(
                            fetchKindName(kind),
                            reply->errorString()
                            )
                    );
                return;
            }

            if (
                !HttpRequestPolicy::isSuccessfulStatus(
                    reply->attribute(
                        QNetworkRequest::HttpStatusCodeAttribute
                        )
                    )
                )
            {
                fail(
                    tr("Unable to download %1: HTTP %2")
                        .arg(fetchKindName(kind))
                        .arg(
                            reply->attribute(
                                QNetworkRequest::HttpStatusCodeAttribute
                                ).toInt()
                            )
                    );
                return;
            }

            const QByteArray data =
                reply->readAll();

            if (data.size() > MaximumManifestBytes)
            {
                fail(
                    tr("Downloaded %1 is too large.")
                        .arg(fetchKindName(kind))
                    );
                return;
            }

            handleFetched(kind, data);
        }
        );
}

void ResourcePackUpdateService::handleFetched(
    FetchKind kind,
    const QByteArray& data
    )
{
    if (kind == FetchKind::Manifest)
    {
        m_manifestData =
            data;

        if (m_configuration.requireSignature)
        {
            const QUrl signatureUrl =
                resolvedSignatureUrl();

            if (!isAllowedManifestUrl(signatureUrl))
            {
                fail(
                    tr("Resource-pack signature URL is not configured or is not HTTPS.")
                    );
                return;
            }

            fetch(
                signatureUrl,
                FetchKind::Signature
                );
            return;
        }

        prepareDownloads();
        return;
    }

    m_signatureData =
        data;
    prepareDownloads();
}

void ResourcePackUpdateService::prepareDownloads()
{
    if (m_configuration.requireSignature)
    {
        if (
            const Status status =
                UpdateSignatureVerifier::verifyDetachedSignature(
                    m_manifestData,
                    m_signatureData,
                    m_configuration.publicKeyPem
                    );
            !status
            )
        {
            fail(status.error());
            return;
        }
    }

    ResourcePackManager& manager =
        ResourcePackManager::instance();

    const auto manifest =
        ResourcePackManifest::fromJson(
            m_manifestData,
            manager.knownPackIds()
            );

    if (!manifest)
    {
        fail(manifest.error());
        return;
    }

    for (const QString& packId : manager.knownPackIds())
    {
        const auto artifact =
            manifest->artifact(packId);

        if (!artifact)
        {
            fail(artifact.error());
            return;
        }

        if (artifact->version > manager.currentVersion(packId))
        {
            m_pendingArtifacts.append(*artifact);
        }
    }

    startNextDownload();
}

void ResourcePackUpdateService::startNextDownload()
{
    if (m_pendingArtifacts.isEmpty())
    {
        finish();
        return;
    }

    m_currentArtifact =
        m_pendingArtifacts.takeFirst();
    m_hash.reset();
    m_bytesWritten =
        0;

    const QString partialPath =
        QDir(ResourcePackManager::instance().storageDirectory()).filePath(
            QStringLiteral(".%1-%2.rcc.part")
                .arg(
                    m_currentArtifact.id,
                    m_currentArtifact.version.toString()
                    )
            );

    QFile::remove(partialPath);
    m_downloadFile.setFileName(partialPath);

    if (!m_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        fail(
            tr("Unable to prepare resource-pack download for '%1'.")
                .arg(m_currentArtifact.id)
            );
        return;
    }

    QNetworkRequest request(m_currentArtifact.url);
    HttpRequestPolicy::applySafeRedirectPolicy(request);

    m_downloadReply =
        m_network.get(request);

    connect(
        m_downloadReply,
        &QNetworkReply::readyRead,
        this,
        &ResourcePackUpdateService::handleDownloadReadyRead
        );
    connect(
        m_downloadReply,
        &QNetworkReply::finished,
        this,
        &ResourcePackUpdateService::handleDownloadFinished
        );
}

void ResourcePackUpdateService::handleDownloadReadyRead()
{
    if (!m_downloadReply || !m_downloadFile.isOpen())
    {
        return;
    }

    const QByteArray data =
        m_downloadReply->readAll();

    if (data.isEmpty())
    {
        return;
    }

    if (
        m_bytesWritten + data.size() > m_currentArtifact.sizeBytes
        || m_downloadFile.write(data) != data.size()
        )
    {
        fail(
            tr("Unable to write resource pack '%1'.")
                .arg(m_currentArtifact.id)
            );
        return;
    }

    m_hash.addData(data);
    m_bytesWritten +=
        data.size();
}

void ResourcePackUpdateService::handleDownloadFinished()
{
    if (!m_downloadReply)
    {
        return;
    }

    handleDownloadReadyRead();

    if (!m_downloadReply)
    {
        return;
    }

    const QNetworkReply::NetworkError networkError =
        m_downloadReply->error();
    const QString networkErrorString =
        m_downloadReply->errorString();
    const QVariant statusCode =
        m_downloadReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
            );

    m_downloadReply->deleteLater();
    m_downloadReply =
        nullptr;

    if (m_downloadFile.isOpen())
    {
        m_downloadFile.close();
    }

    if (networkError != QNetworkReply::NoError)
    {
        fail(
            tr("Unable to download resource pack '%1': %2")
                .arg(
                    m_currentArtifact.id,
                    networkErrorString
                    )
            );
        return;
    }

    if (!HttpRequestPolicy::isSuccessfulStatus(statusCode))
    {
        fail(
            tr("Unable to download resource pack '%1': HTTP %2")
                .arg(m_currentArtifact.id)
                .arg(statusCode.toInt())
            );
        return;
    }

    if (m_bytesWritten != m_currentArtifact.sizeBytes)
    {
        fail(
            tr("Resource pack '%1' size did not match the manifest.")
                .arg(m_currentArtifact.id)
            );
        return;
    }

    const QString actualHash =
        QString::fromLatin1(
            m_hash.result().toHex()
            );

    if (actualHash.compare(m_currentArtifact.sha256, Qt::CaseInsensitive) != 0)
    {
        fail(
            tr("Resource pack '%1' checksum did not match the manifest.")
                .arg(m_currentArtifact.id)
            );
        return;
    }

    const QString downloadedPath =
        m_downloadFile.fileName();

    const Status stageStatus =
        ResourcePackManager::instance().stagePack(
            m_currentArtifact,
            downloadedPath
            );

    if (!stageStatus)
    {
        fail(stageStatus.error());
        return;
    }

    const QString stagedId =
        m_currentArtifact.id;
    const QString stagedVersion =
        m_currentArtifact.version.toString();

    m_stagedPackIds.append(stagedId);
    resetDownload();

    emit packStaged(
        stagedId,
        stagedVersion
        );

    startNextDownload();
}

void ResourcePackUpdateService::finish()
{
    const QStringList stagedPackIds =
        m_stagedPackIds;

    m_busy =
        false;
    m_manifestData.clear();
    m_signatureData.clear();
    m_pendingArtifacts.clear();
    m_stagedPackIds.clear();

    emit checkSucceeded(stagedPackIds);
}

void ResourcePackUpdateService::fail(
    const QString& message
    )
{
    const QString partialPath =
        m_downloadFile.fileName();

    if (m_downloadReply)
    {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply =
            nullptr;
    }

    if (m_downloadFile.isOpen())
    {
        m_downloadFile.close();
    }

    if (!partialPath.isEmpty())
    {
        QFile::remove(partialPath);
    }

    resetDownload();
    m_busy =
        false;
    m_pendingArtifacts.clear();

    emit checkFailed(message);
}

void ResourcePackUpdateService::resetDownload()
{
    m_downloadFile.setFileName(QString());
    m_currentArtifact =
        ResourcePackArtifact();
    m_hash.reset();
    m_bytesWritten =
        0;
}

QUrl ResourcePackUpdateService::resolvedSignatureUrl() const
{
    if (
        m_configuration.signatureUrl.isValid()
        && !m_configuration.signatureUrl.isRelative()
        && !m_configuration.signatureUrl.isEmpty()
        )
    {
        return m_configuration.signatureUrl;
    }

    QUrl derived =
        m_configuration.manifestUrl;
    const QString path =
        derived.path();

    if (path.endsWith(QStringLiteral(".json")))
    {
        derived.setPath(
            path.left(path.size() - 5)
                + QStringLiteral(".sig")
            );
        return derived;
    }

    return QUrl();
}

bool ResourcePackUpdateService::isAllowedManifestUrl(
    const QUrl& url
    ) const
{
    return HttpRequestPolicy::isAllowedSecureUrl(url);
}
