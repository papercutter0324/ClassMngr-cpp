#include "core/resource_packs/resource_pack_manager.h"

#include <utility>

ResourcePackManager& ResourcePackManager::instance()
{
    static ResourcePackManager manager;
    return manager;
}

ResourcePackManager::ResourcePackManager(
    QString storageDirectory,
    QString baselineDirectory
    )
    : m_storageDirectory(std::move(storageDirectory))
    , m_baselineDirectory(std::move(baselineDirectory))
{
}

QString ResourcePackManager::activeRoot(
    const QString&
    ) const
{
    return {};
}
