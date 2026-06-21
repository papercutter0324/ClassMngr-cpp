#pragma once

#include "core/resource_packs/resource_pack_manifest.h"
#include "core/result.h"
#include "core/updater/version.h"

#include <QHash>
#include <QString>
#include <QStringList>

class ResourcePackManager
{
public:
    static ResourcePackManager& instance();

    explicit ResourcePackManager(
        QString storageDirectory = QString()
        );

    [[nodiscard]] Status initialize();

    [[nodiscard]] QStringList knownPackIds() const;
    [[nodiscard]] QString activeRoot(
        const QString& packId
        ) const;
    [[nodiscard]] Version currentVersion(
        const QString& packId
        ) const;
    [[nodiscard]] QString storageDirectory() const;

    [[nodiscard]] Status stagePack(
        const ResourcePackArtifact& artifact,
        const QString& downloadedFilePath
        );

private:
    struct Definition
    {
        QString id;
        Version embeddedVersion;
    };

    [[nodiscard]] const Definition* definition(
        const QString& packId
        ) const;
    [[nodiscard]] Status loadInstalledPack(
        const Definition& definition
        );
    void removeStalePackFiles() const;

    QList<Definition> m_definitions;
    QHash<QString, QString> m_activeRoots;
    QHash<QString, Version> m_activeVersions;
    QHash<QString, QString> m_activeFiles;
    QString m_storageDirectory;
    bool m_initialized = false;
};
