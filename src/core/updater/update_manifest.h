#pragma once

#include "core/result.h"
#include "core/updater/version.h"

#include <QDate>
#include <QHash>
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

class UpdateManifest
{
public:
    [[nodiscard]] static Result<UpdateManifest> fromJson(
        const QByteArray& data,
        const QStringList& requiredPlatformKeys = currentPlatformKeys()
        );

    [[nodiscard]] static QStringList currentPlatformKeys();
    [[nodiscard]] static bool isAllowedDownloadUrl(
        const QUrl& url
        );

    [[nodiscard]] int schemaVersion() const;
    [[nodiscard]] QString channel() const;
    [[nodiscard]] Version latestVersion() const;
    [[nodiscard]] Version minimumSupportedVersion() const;
    [[nodiscard]] QDate releaseDate() const;
    [[nodiscard]] QUrl notesUrl() const;
    [[nodiscard]] QStringList platformKeys() const;

    [[nodiscard]] bool hasArtifactFor(
        const QString& platformKey
        ) const;

    [[nodiscard]] Result<UpdateArtifact> artifactForAny(
        const QStringList& platformKeys
        ) const;

private:
    static Result<UpdateArtifact> parseArtifact(
        const QString& platformKey,
        const QJsonObject& object
        );

private:
    int m_schemaVersion = 0;
    QString m_channel;
    Version m_latestVersion;
    Version m_minimumSupportedVersion;
    QDate m_releaseDate;
    QUrl m_notesUrl;
    QHash<QString, UpdateArtifact> m_artifacts;
};
