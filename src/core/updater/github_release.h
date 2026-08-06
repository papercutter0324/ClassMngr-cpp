#pragma once

#include "core/result.h"
#include "core/updater/version.h"

#include <QDate>
#include <QString>
#include <QStringList>
#include <QUrl>

struct UpdateArtifact
{
    QString platformKey;
    QUrl url;
    QString fileName;
    QString sha256;
    qint64 sizeBytes = 0;
};

class GitHubRelease
{
public:
    GitHubRelease() = default;
    GitHubRelease(
        Version version,
        UpdateArtifact artifact,
        QDate releaseDate,
        QUrl releaseUrl
        );

    [[nodiscard]] static Result<GitHubRelease> latestCompatibleFromJson(
        const QByteArray& data,
        const QStringList& platformKeys = currentPlatformKeys()
        );

    [[nodiscard]] static QStringList currentPlatformKeys();
    [[nodiscard]] static bool isAllowedDownloadUrl(
        const QUrl& url
        );

    [[nodiscard]] Version version() const;
    [[nodiscard]] UpdateArtifact artifact() const;
    [[nodiscard]] QDate releaseDate() const;
    [[nodiscard]] QUrl releaseUrl() const;

private:
    Version m_version;
    UpdateArtifact m_artifact;
    QDate m_releaseDate;
    QUrl m_releaseUrl;
};
