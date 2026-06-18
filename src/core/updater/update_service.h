#pragma once

#include "core/updater/update_configuration.h"
#include "core/updater/update_manifest.h"

#include <QNetworkAccessManager>
#include <QObject>

struct UpdateCheckResult
{
    Version currentVersion;
    Version latestVersion;
    Version minimumSupportedVersion;
    UpdateArtifact artifact;
    QDate releaseDate;
    QUrl notesUrl;
    bool updateAvailable = false;
    bool currentVersionSupported = true;
};

class UpdateService : public QObject
{
    Q_OBJECT

public:
    enum class FetchKind
    {
        Manifest,
        Signature
    };

    explicit UpdateService(
        UpdateConfiguration configuration = UpdateConfiguration::fromBuild(),
        QObject* parent = nullptr
        );

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] UpdateConfiguration configuration() const;

public slots:
    void checkForUpdates();

signals:
    void checkStarted();
    void checkSucceeded(
        const UpdateCheckResult& result
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
    void completeCheck();
    void fail(
        const QString& message
        );

    [[nodiscard]] QUrl resolvedSignatureUrl() const;
    [[nodiscard]] bool isAllowedManifestUrl(
        const QUrl& url
        ) const;

private:
    UpdateConfiguration m_configuration;
    QNetworkAccessManager m_network;
    bool m_busy = false;
    QByteArray m_manifestData;
    QByteArray m_signatureData;
};
