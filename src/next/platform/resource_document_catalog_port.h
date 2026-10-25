#pragma once

#include "next/application/document_catalog_projection.h"
#include "next/domain/operation_result.h"

#include <QString>

namespace ClassMngr::Next::Resources
{
class ResourceLoader;
}

namespace ClassMngr::Next::Platform
{

// Reads and projects document catalog metadata through the typed resource
// boundary. Document content remains represented only by resource references.
class ResourceDocumentCatalogPort final
{
public:
    explicit ResourceDocumentCatalogPort(
        Resources::ResourceLoader& loader
        ) noexcept
        : m_loader(loader)
    {
    }

    ResourceDocumentCatalogPort(const ResourceDocumentCatalogPort&) = delete;
    ResourceDocumentCatalogPort& operator=(
        const ResourceDocumentCatalogPort&
        ) = delete;
    ResourceDocumentCatalogPort(ResourceDocumentCatalogPort&&) = delete;
    ResourceDocumentCatalogPort& operator=(ResourceDocumentCatalogPort&&) = delete;

    [[nodiscard]] Domain::Result<Application::DocumentCatalogProjection>
    projection(const QString& localeName) const;

private:
    Resources::ResourceLoader& m_loader;
};

} // namespace ClassMngr::Next::Platform
