#pragma once

#include "next/application/document_content_session.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// These limits keep one projection compact enough for UI consumers to copy.
// An adapter must paginate or stage a larger catalog before creating a
// projection; it must not move the larger source representation across this
// boundary.
inline constexpr std::size_t kDocumentCatalogMaxIdentifierLength = 256;
inline constexpr std::size_t kDocumentCatalogMaxPathLength = 4'096;
inline constexpr std::size_t kDocumentCatalogMaxKeyLength = 256;
inline constexpr std::size_t kDocumentCatalogMaxDisplayNameLength = 256;
inline constexpr std::size_t kDocumentCatalogMaxReferenceLength =
    kDocumentContentMaxReferenceLength;
inline constexpr std::size_t kDocumentCatalogMaxFolderEntries = 4'096;
inline constexpr std::size_t kDocumentCatalogMaxDocumentEntries = 4'096;

// A folder is copied metadata only. Its identifier is intentionally a
// different type from a document identifier, so a document cannot
// accidentally use a folder id at a call site.
struct DocumentFolderMetadata final
{
    Domain::DocumentFolderId id;
    std::string path;
    std::string key;
    std::string displayName;
    std::int32_t order = 0;

    friend bool operator==(
        const DocumentFolderMetadata&,
        const DocumentFolderMetadata&
        ) = default;
};

// A document entry contains only bounded metadata and adapter-neutral
// references. DocumentContentReference never owns document bytes or a viewer
// object; the adapter owns those resources and releases them at its content
// session boundary. The optional export reference remains observably absent
// when no export asset exists.
struct DocumentEntryMetadata final
{
    Domain::DocumentId id;
    Domain::DocumentFolderId folderId;
    std::string path;
    std::string key;
    std::string displayName;
    std::int32_t order = 0;
    bool printable = false;
    bool exportable = false;
    DocumentContentReference contentReference;
    std::optional<DocumentContentReference> exportReference;

    [[nodiscard]] const std::optional<DocumentContentReference>&
    exportContentReference() const noexcept
    {
        return exportReference;
    }

    friend bool operator==(
        const DocumentEntryMetadata&,
        const DocumentEntryMetadata&
        ) = default;
};

using DocumentFolder = DocumentFolderMetadata;
using DocumentEntry = DocumentEntryMetadata;

struct DocumentCatalogProjectionInput final
{
    std::vector<DocumentFolderMetadata> folders;
    std::vector<DocumentEntryMetadata> documents;

    friend bool operator==(
        const DocumentCatalogProjectionInput&,
        const DocumentCatalogProjectionInput&
        ) = default;
};

using DocumentCatalogInput = DocumentCatalogProjectionInput;

namespace DocumentCatalogProjectionDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string& value
    ) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline bool isValidIdentifier(
    const std::string& value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kDocumentCatalogMaxIdentifierLength;
}

[[nodiscard]] inline bool isValidPath(
    const std::string& value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kDocumentCatalogMaxPathLength;
}

[[nodiscard]] inline bool isValidKey(
    const std::string& value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kDocumentCatalogMaxKeyLength;
}

[[nodiscard]] inline bool isValidDisplayName(
    const std::string& value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kDocumentCatalogMaxDisplayNameLength;
}

[[nodiscard]] inline bool isValidReference(
    const DocumentContentReference& reference
    ) noexcept
{
    return !isBlank(reference.value())
        && reference.value().size() <= kDocumentCatalogMaxReferenceLength;
}

template <typename TypedId>
[[nodiscard]] inline bool containsId(
    const std::vector<TypedId>& ids,
    const TypedId& candidate
    ) noexcept
{
    return std::find(ids.cbegin(), ids.cend(), candidate) != ids.cend();
}

[[nodiscard]] inline Domain::OperationError invalidInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

[[nodiscard]] inline Domain::Result<void> validateFolder(
    const DocumentFolderMetadata& folder,
    const std::vector<Domain::DocumentFolderId>& folderIds
    )
{
    if (!isValidIdentifier(folder.id.value()))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document folder identifier must be non-blank and bounded."
                )
            );
    }

    if (containsId(folderIds, folder.id))
    {
        return Domain::Result<void>::failure(
            invalidInput("Document folder identifiers must be unique.")
            );
    }

    if (!isValidPath(folder.path)
        || !isValidKey(folder.key)
        || !isValidDisplayName(folder.displayName))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document folder path, key, and display name must be bounded and non-blank."
                )
            );
    }

    if (folder.order < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Document folder order must not be negative.")
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateDocument(
    const DocumentEntryMetadata& document,
    const std::vector<Domain::DocumentFolderId>& folderIds,
    const std::vector<Domain::DocumentId>& documentIds
    )
{
    if (!isValidIdentifier(document.id.value()))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document identifier must be non-blank and bounded."
                )
            );
    }

    if (containsId(documentIds, document.id))
    {
        return Domain::Result<void>::failure(
            invalidInput("Document identifiers must be unique.")
            );
    }

    if (!isValidIdentifier(document.folderId.value()))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document folder identifier must be non-blank and bounded."
                )
            );
    }

    if (!containsId(folderIds, document.folderId))
    {
        return Domain::Result<void>::failure(
            invalidInput("Document references an unknown folder identifier.")
            );
    }

    if (!isValidPath(document.path)
        || !isValidKey(document.key)
        || !isValidDisplayName(document.displayName))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document path, key, and display name must be bounded and non-blank."
                )
            );
    }

    if (document.order < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Document order must not be negative.")
            );
    }

    if (!isValidReference(document.contentReference))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document content reference must be non-blank and bounded."
                )
            );
    }

    if (document.exportReference.has_value()
        && !isValidReference(*document.exportReference))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document export reference must be non-blank and bounded."
                )
            );
    }

    if (document.exportable != document.exportReference.has_value())
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document exportability must match the optional export reference."
                )
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateInput(
    const DocumentCatalogProjectionInput& input
    )
{
    if (input.folders.size() > kDocumentCatalogMaxFolderEntries
        || input.documents.size() > kDocumentCatalogMaxDocumentEntries)
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Document catalog folder or document collection exceeds its bounded limit."
                )
            );
    }

    std::vector<Domain::DocumentFolderId> folderIds;
    folderIds.reserve(input.folders.size());
    for (const auto& folder : input.folders)
    {
        const auto validation = validateFolder(folder, folderIds);
        if (!validation)
        {
            return validation;
        }

        folderIds.push_back(folder.id);
    }

    std::vector<Domain::DocumentId> documentIds;
    documentIds.reserve(input.documents.size());
    for (const auto& document : input.documents)
    {
        const auto validation = validateDocument(
            document,
            folderIds,
            documentIds
            );
        if (!validation)
        {
            return validation;
        }

        documentIds.push_back(document.id);
    }

    return Domain::Result<void>::success();
}

}

// The projection owns the copied metadata vectors and nothing else. A caller
// can release its parser/catalog source after create() returns, and a UI
// consumer can copy and release this value without retaining adapter state.
// Adapters retain ownership of content bytes, export bytes, and viewer
// objects; those resources are released through DocumentContentSession rather
// than through this metadata projection.
class DocumentCatalogProjection final
{
public:
    using Input = DocumentCatalogProjectionInput;
    using Folder = DocumentFolderMetadata;
    using Document = DocumentEntryMetadata;

    DocumentCatalogProjection() = default;

    [[nodiscard]] static Domain::Result<DocumentCatalogProjection> create(
        Input input
        )
    {
        const auto validation = DocumentCatalogProjectionDetail::validateInput(
            input
            );
        if (!validation)
        {
            return Domain::Result<DocumentCatalogProjection>::failure(
                validation.error()
                );
        }

        return Domain::Result<DocumentCatalogProjection>::success(
            DocumentCatalogProjection(std::move(input))
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& input
        )
    {
        return DocumentCatalogProjectionDetail::validateInput(input);
    }

    [[nodiscard]] const std::vector<Folder>& folders() const noexcept
    {
        return m_folders;
    }

    [[nodiscard]] const std::vector<Document>& documents() const noexcept
    {
        return m_documents;
    }

    [[nodiscard]] const std::vector<Document>& entries() const noexcept
    {
        return documents();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_folders.empty() && m_documents.empty();
    }

    friend bool operator==(
        const DocumentCatalogProjection&,
        const DocumentCatalogProjection&
        ) = default;

private:
    explicit DocumentCatalogProjection(
        Input input
        )
        : m_folders(std::move(input.folders)),
          m_documents(std::move(input.documents))
    {
    }

    std::vector<Folder> m_folders;
    std::vector<Document> m_documents;
};

using DocumentCatalogSnapshot = DocumentCatalogProjection;

}
