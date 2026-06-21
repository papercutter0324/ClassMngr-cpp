#pragma once

#include "core/resource_packs/resource_pack_configuration.h"
#include "core/resource_packs/resource_pack_manifest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;

class ResourcePackUpdateService : public QObject
{
    Q_OBJECT

public:
    enum class FetchKind
    {
        Manifest,
        Signature
    };

    explicit ResourcePackUpdateService(
        ResourcePackConfiguration configuration = ResourcePackConfiguration::fromBuild(),
        QObject* parent = nullptr
        );

    [[nodiscard]] bool isBusy() const;

public slots:
    void checkForUpdates();

signals:
    void checkStarted();
    void packStaged(
        const QString& packId,
        const QString& version
        );
    void checkSucceeded(
        const QStringList& stagedPackIds
        );
    void checkFailed(
        const QString& message
        );

private:
    void fetch(
        const QUrl& url,
        FetchKind kind
        );
    void handleFetched(
        FetchKind kind,
        const QByteArray& data
        );
    void prepareDownloads();
    void startNextDownload();
    void handleDownloadReadyRead();
    void handleDownloadFinished();
    void finish();
    void fail(
        const QString& message
        );
    void resetDownload();

    [[nodiscard]] QUrl resolvedSignatureUrl() const;
    [[nodiscard]] bool isAllowedManifestUrl(
        const QUrl& url
        ) const;

    ResourcePackConfiguration m_configuration;
    QNetworkAccessManager m_network;
    QNetworkReply* m_downloadReply = nullptr;
    QFile m_downloadFile;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QList<ResourcePackArtifact> m_pendingArtifacts;
    ResourcePackArtifact m_currentArtifact;
    QStringList m_stagedPackIds;
    QByteArray m_manifestData;
    QByteArray m_signatureData;
    qint64 m_bytesWritten = 0;
    bool m_busy = false;
};
