#pragma once

#include "resource_descriptor.h"
#include "resource_error.h"

#include <QJsonValue>
#include <QString>

#include <memory>
#include <vector>

namespace ClassMngr::Next::Resources
{

class ResourceCatalog final
{
public:
    ResourceCatalog(const ResourceCatalog&) = default;
    ResourceCatalog(ResourceCatalog&&) noexcept = default;
    ResourceCatalog& operator=(const ResourceCatalog&) = delete;
    ResourceCatalog& operator=(ResourceCatalog&&) = delete;

    [[nodiscard]] static ResourceResult<ResourceCatalog> load(
        const QString& catalogFilePath,
        const QString& resourceRoot
        );

    // The returned descriptor is borrowed and remains valid for this catalog's lifetime.
    [[nodiscard]] const ResourceDescriptor* find(
        const ResourceId& id
        ) const noexcept;

    [[nodiscard]] const std::vector<ResourceDescriptor>& records() const noexcept;

    [[nodiscard]] const QString& resourceRoot() const noexcept
    {
        return m_resourceRoot;
    }

private:
    struct Data;

    [[nodiscard]] static ResourceResult<ResourceDescriptor> parseRecord(
        const QJsonValue& value
        );

    ResourceCatalog(
        QString resourceRoot,
        std::shared_ptr<const Data> data
        )
        : m_resourceRoot(std::move(resourceRoot))
        , m_data(std::move(data))
    {
    }

    QString m_resourceRoot;
    std::shared_ptr<const Data> m_data;
};

}
