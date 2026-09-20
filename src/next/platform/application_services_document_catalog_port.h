#pragma once

#include "core/application_services.h"
#include "next/application/document_catalog_projection.h"

#include <QDir>
#include <QString>

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter that copies the legacy catalog into bounded application
// metadata. ApplicationServices remains caller-owned and no catalog pointer or
// document content crosses this boundary.
class ApplicationServicesDocumentCatalogPort final
{
public:
    explicit ApplicationServicesDocumentCatalogPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesDocumentCatalogPort(
        const ApplicationServicesDocumentCatalogPort&
        ) = delete;
    ApplicationServicesDocumentCatalogPort& operator=(
        const ApplicationServicesDocumentCatalogPort&
        ) = delete;
    ApplicationServicesDocumentCatalogPort(
        ApplicationServicesDocumentCatalogPort&&
        ) = delete;
    ApplicationServicesDocumentCatalogPort& operator=(
        ApplicationServicesDocumentCatalogPort&&
        ) = delete;

    [[nodiscard]] Domain::Result<Application::DocumentCatalogProjection>
    projection(
        const QString& localeName
        ) const
    {
        try
        {
            const DocumentCatalog* catalog = m_services.documentCatalog();
            if (!catalog)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The document catalog is unavailable."
                    );
            }

            Application::DocumentCatalogProjectionInput input;
            if (catalog->folders().size()
                    > static_cast<qsizetype>(
                        Application::kDocumentCatalogMaxFolderEntries
                        )
                || catalog->documents().size()
                    > static_cast<qsizetype>(
                        Application::kDocumentCatalogMaxDocumentEntries
                        ))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "The document catalog exceeds its bounded entry limits."
                    );
            }

            input.folders.reserve(
                static_cast<std::size_t>(catalog->folders().size())
                );
            for (const DocumentFolderDefinition& folder : catalog->folders())
            {
                const auto folderId = typedId<Domain::DocumentFolderId>(
                    folder.id,
                    Application::kDocumentCatalogMaxIdentifierLength
                    );
                if (!folderId)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A document folder has an invalid typed identifier."
                        );
                }

                const auto path = boundedUtf8(
                    folder.path,
                    Application::kDocumentCatalogMaxPathLength
                    );
                const auto parentPath = folder.parentPath.isEmpty()
                    ? std::optional<std::string>{std::string{}}
                    : boundedUtf8(
                        folder.parentPath,
                        Application::kDocumentCatalogMaxPathLength
                        );
                const auto key = boundedUtf8(
                    folder.id,
                    Application::kDocumentCatalogMaxKeyLength
                    );
                const auto displayName = boundedUtf8(
                    folder.sidebarNames.forLocale(localeName),
                    Application::kDocumentCatalogMaxDisplayNameLength
                    );
                if (!path || !parentPath || !key || !displayName
                    || folder.order < 0)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A document folder contains invalid or unbounded metadata."
                        );
                }

                input.folders.push_back({
                    *folderId,
                    *path,
                    *key,
                    *displayName,
                    folder.order,
                    *parentPath
                });
            }

            input.documents.reserve(
                static_cast<std::size_t>(catalog->documents().size())
                );
            for (const DocumentDefinition& document : catalog->documents())
            {
                const auto documentId = typedId<Domain::DocumentId>(
                    document.id,
                    Application::kDocumentCatalogMaxIdentifierLength
                    );
                if (!documentId)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A document has an invalid typed identifier."
                        );
                }

                const auto folder = std::find_if(
                    catalog->folders().cbegin(),
                    catalog->folders().cend(),
                    [&document](const DocumentFolderDefinition& candidate)
                    {
                        return candidate.path == document.pdf.path;
                    }
                    );
                if (folder == catalog->folders().cend())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "A document PDF path has no matching catalog folder."
                        );
                }

                const auto folderId = typedId<Domain::DocumentFolderId>(
                    folder->id,
                    Application::kDocumentCatalogMaxIdentifierLength
                    );
                const QString relativePdfPath = relativePath(
                    document.pdf.path,
                    document.pdf.fileName
                    );
                const auto path = boundedUtf8(
                    relativePdfPath,
                    Application::kDocumentCatalogMaxPathLength
                    );
                const auto key = boundedUtf8(
                    document.id,
                    Application::kDocumentCatalogMaxKeyLength
                    );
                const auto displayName = boundedUtf8(
                    document.sidebarNames.forLocale(localeName),
                    Application::kDocumentCatalogMaxDisplayNameLength
                    );
                const auto contentReference = resourceReference(relativePdfPath);
                if (!folderId || !path || !key || !displayName
                    || !contentReference || document.order < 0)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A document contains invalid or unbounded metadata."
                        );
                }

                const bool exportable = document.exportingEnabled
                    && document.exportFile.has_value();
                std::optional<Application::DocumentContentReference>
                    exportReference;
                if (exportable)
                {
                    const QString relativeExportPath = relativePath(
                        document.exportFile->path,
                        document.exportFile->fileName
                        );
                    const auto reference = resourceReference(relativeExportPath);
                    if (!reference)
                    {
                        return failure(
                            Domain::ErrorCode::InvalidInput,
                            "A document export reference is invalid or unbounded."
                            );
                    }
                    exportReference.emplace(*reference);
                }

                input.documents.push_back({
                    *documentId,
                    *folderId,
                    *path,
                    *key,
                    *displayName,
                    document.order,
                    document.printingEnabled,
                    exportable,
                    Application::DocumentContentReference(*contentReference),
                    std::move(exportReference)
                });
            }

            auto result = Application::DocumentCatalogProjection::create(
                std::move(input)
                );
            if (!result)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "The mapped document catalog projection failed validation."
                    );
            }
            return result;
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The document catalog could not be projected."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The document catalog could not be projected."
                );
        }
    }

private:
    template <typename TypedId>
    [[nodiscard]] static std::optional<TypedId> typedId(
        const QString& value,
        const std::size_t maximumBytes
        )
    {
        const auto text = boundedUtf8(value, maximumBytes);
        return text ? TypedId::fromString(*text) : std::nullopt;
    }

    [[nodiscard]] static std::optional<std::string> boundedUtf8(
        const QString& value,
        const std::size_t maximumBytes
        )
    {
        if (value.trimmed().isEmpty())
        {
            return std::nullopt;
        }

        const QByteArray bytes = value.toUtf8();
        if (static_cast<std::size_t>(bytes.size()) > maximumBytes)
        {
            return std::nullopt;
        }
        return bytes.toStdString();
    }

    [[nodiscard]] static QString relativePath(
        const QString& path,
        const QString& fileName
        )
    {
        return QDir::fromNativeSeparators(QDir(path).filePath(fileName));
    }

    [[nodiscard]] static std::optional<std::string> resourceReference(
        const QString& relativePath
        )
    {
        return boundedUtf8(
            QStringLiteral("resource://documents/") + relativePath,
            Application::kDocumentCatalogMaxReferenceLength
            );
    }

    [[nodiscard]] static Domain::Result<Application::DocumentCatalogProjection>
    failure(
        const Domain::ErrorCode code,
        const char* message
        )
    {
        return Domain::Result<Application::DocumentCatalogProjection>::failure({
            .code = code,
            .message = message,
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
