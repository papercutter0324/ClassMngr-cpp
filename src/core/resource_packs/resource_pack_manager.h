#pragma once

#include "core/resource_packs/resource_pack_manifest.h"
#include "core/result.h"
#include "core/updater/version.h"

#include <QHash>
#include <QString>
#include <QStringList>

class ResourcePackManager;

// Keeps an RCC pack mounted while a caller is reading paths from it. Leases
// are deliberately move-only: resource paths must never outlive their owner.
class ResourcePackLease
{
public:
    ResourcePackLease() = default;
    ~ResourcePackLease();

    ResourcePackLease(const ResourcePackLease&) = delete;
    ResourcePackLease& operator=(const ResourcePackLease&) = delete;

    ResourcePackLease(ResourcePackLease&& other) noexcept;
    ResourcePackLease& operator=(ResourcePackLease&& other) noexcept;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString packId() const;
    [[nodiscard]] QString root() const;
    void reset();

private:
    friend class ResourcePackManager;

    ResourcePackLease(
        ResourcePackManager* manager,
        QString packId,
        QString root
        );

    ResourcePackManager* m_manager = nullptr;
    QString m_packId;
    QString m_root;
};

class ResourcePackManager
{
public:
    static ResourcePackManager& instance();

    explicit ResourcePackManager(
        QString storageDirectory = QString(),
        QString baselineDirectory = QString()
        );

    // Performs file-system discovery only. It never registers an RCC.
    [[nodiscard]] Status initialize();

    [[nodiscard]] Result<ResourcePackLease> acquire(
        const QString& packId
        );
    [[nodiscard]] bool isMounted(const QString& packId) const;

    // A root is exposed only while at least one lease is alive.
    [[nodiscard]] QString activeRoot(const QString& packId) const;
    [[nodiscard]] QStringList knownPackIds() const;
    [[nodiscard]] Version currentVersion(const QString& packId) const;
    [[nodiscard]] QString storageDirectory() const;
    [[nodiscard]] QString baselineDirectory() const;

    [[nodiscard]] Status stagePack(
        const ResourcePackArtifact& artifact,
        const QString& downloadedFilePath
        );

private:
    friend class ResourcePackLease;

    struct Definition
    {
        QString id;
        Version baselineVersion;
        bool updateable = false;
    };

    struct InstalledPack
    {
        QString filePath;
        Version version;
    };

    struct MountedPack
    {
        QString filePath;
        QString root;
        Version version;
        int leaseCount = 0;
    };

    [[nodiscard]] const Definition* definition(const QString& packId) const;
    [[nodiscard]] Status discoverInstalledPack(const Definition& definition);
    [[nodiscard]] Status mount(const Definition& definition);
    void release(const QString& packId);
    void removeStalePackFiles() const;

    QList<Definition> m_definitions;
    QHash<QString, InstalledPack> m_installedPacks;
    QHash<QString, MountedPack> m_mountedPacks;
    QString m_storageDirectory;
    QString m_baselineDirectory;
    bool m_initialized = false;
};
