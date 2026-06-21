#pragma once

#include "core/result.h"
#include "core/updater/version.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QUrl>

struct ResourcePackArtifact
{
    QString id;
    Version version;
    QUrl url;
    QString fileName;
    QString sha256;
    qint64 sizeBytes = 0;
};

class ResourcePackManifest
{
public:
    [[nodiscard]] static Result<ResourcePackManifest> fromJson(
        const QByteArray& data,
        const QStringList& requiredPackIds = {}
        );

    [[nodiscard]] static bool isAllowedDownloadUrl(
        const QUrl& url
        );

    [[nodiscard]] int schemaVersion() const;
    [[nodiscard]] QStringList packIds() const;
    [[nodiscard]] bool hasPack(
        const QString& packId
        ) const;
    [[nodiscard]] Result<ResourcePackArtifact> artifact(
        const QString& packId
        ) const;

private:
    int m_schemaVersion = 0;
    QHash<QString, ResourcePackArtifact> m_artifacts;
};
