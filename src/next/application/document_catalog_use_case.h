#pragma once

#include "next/application/document_catalog_projection.h"

#include <algorithm>

namespace ClassMngr::Next::Application
{

// Coordinates immutable catalog metadata with one caller-owned mutable
// content session. The projection and session both outlive this coordinator;
// no catalog, viewer, or document-content ownership is retained here.
class DocumentCatalogUseCase final
{
public:
    using SessionToken = DocumentContentSession::SessionToken;

    explicit DocumentCatalogUseCase(
        const DocumentCatalogProjection& projection,
        DocumentContentSession& contentSession
        ) noexcept
        : m_projection(projection),
          m_contentSession(contentSession)
    {
    }

    [[nodiscard]] Domain::Result<DocumentEntryMetadata> resolveDocument(
        const Domain::DocumentId& documentId
        ) const
    {
        const auto document = std::find_if(
            m_projection.documents().cbegin(),
            m_projection.documents().cend(),
            [&documentId](const DocumentEntryMetadata& candidate)
            {
                return candidate.id == documentId;
            }
            );

        if (document == m_projection.documents().cend())
        {
            return Domain::Result<DocumentEntryMetadata>::failure(
                notFound("Document was not found in the catalog.")
                );
        }

        return Domain::Result<DocumentEntryMetadata>::success(*document);
    }

    [[nodiscard]] Domain::Result<SessionToken> requestPrimaryContent(
        const Domain::DocumentId& documentId
        ) const
    {
        const auto document = resolveDocument(documentId);
        if (!document)
        {
            return Domain::Result<SessionToken>::failure(document.error());
        }

        return m_contentSession.request(document.value().contentReference);
    }

    [[nodiscard]] Domain::Result<SessionToken> requestExportContent(
        const Domain::DocumentId& documentId
        ) const
    {
        const auto document = resolveDocument(documentId);
        if (!document)
        {
            return Domain::Result<SessionToken>::failure(document.error());
        }

        if (!document.value().exportReference.has_value())
        {
            return Domain::Result<SessionToken>::failure(
                notFound("Document has no optional export reference.")
                );
        }

        return m_contentSession.request(*document.value().exportReference);
    }

private:
    [[nodiscard]] static Domain::OperationError notFound(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::NotFound,
            .message = message,
            .recoverable = false
        };
    }

    const DocumentCatalogProjection& m_projection;
    DocumentContentSession& m_contentSession;
};

} // namespace ClassMngr::Next::Application
