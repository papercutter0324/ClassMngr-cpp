#pragma once

#include "core/resource_packs/resource_pack_manager.h"
#include "next/application/document_content_session.h"
#include "next/domain/operation_result.h"

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Platform
{

inline constexpr std::string_view kDocumentContentResourceScheme =
    "resource://documents/";

// A resolved document keeps the one pack lease that makes both paths valid.
// The value is move-only because releasing the lease invalidates the paths.
struct DocumentContentResource final
{
    ResourcePackLease resourceLease;
    QString primaryPath;
    std::optional<QString> exportPath;
};

using DocumentContentResourceResult =
    Domain::Result<DocumentContentResource>;

// Resolves application content references at the resource-pack boundary. The
// application layer supplies only its Qt-free reference values; this adapter
// owns the Qt path conversion, resource existence checks, and lease lifetime.
class DocumentContentResourcePort final
{
public:
    explicit DocumentContentResourcePort(
        ResourcePackManager& resourcePacks
        ) noexcept
        : m_resourcePacks(resourcePacks)
    {
    }

    DocumentContentResourcePort(
        const DocumentContentResourcePort&
        ) = delete;
    DocumentContentResourcePort& operator=(
        const DocumentContentResourcePort&
        ) = delete;
    DocumentContentResourcePort(
        DocumentContentResourcePort&&
        ) = delete;
    DocumentContentResourcePort& operator=(
        DocumentContentResourcePort&&
        ) = delete;

    [[nodiscard]] DocumentContentResourceResult resolve(
        const Application::DocumentContentReference& primaryReference,
        const std::optional<Application::DocumentContentReference>&
            exportReference = std::nullopt
        ) const
    {
        const auto primaryPath = relativePath(primaryReference);
        if (!primaryPath)
        {
            return DocumentContentResourceResult::failure(
                primaryPath.error()
                );
        }

        std::optional<QString> exportRelativePath;
        if (exportReference.has_value())
        {
            const auto parsedExportPath = relativePath(*exportReference);
            if (!parsedExportPath)
            {
                return DocumentContentResourceResult::failure(
                    parsedExportPath.error()
                    );
            }
            exportRelativePath = parsedExportPath.value();
        }

        auto lease = m_resourcePacks.acquire(
            QStringLiteral("documents")
            );
        if (!lease)
        {
            return DocumentContentResourceResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The documents resource pack could not be acquired: "
                    + lease.error().toUtf8().toStdString(),
                .recoverable = true
            });
        }

        const QString primaryResolvedPath = QDir(lease->root()).filePath(
            primaryPath.value()
            );
        if (!QFileInfo(primaryResolvedPath).isFile())
        {
            return DocumentContentResourceResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The primary document resource does not exist.",
                .recoverable = true
            });
        }

        std::optional<QString> exportResolvedPath;
        if (exportRelativePath.has_value())
        {
            const QString candidate = QDir(lease->root()).filePath(
                *exportRelativePath
                );
            if (!QFileInfo(candidate).isFile())
            {
                return DocumentContentResourceResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "The export document resource does not exist.",
                    .recoverable = true
                });
            }
            exportResolvedPath = candidate;
        }

        return DocumentContentResourceResult::success(
            DocumentContentResource{
                std::move(*lease),
                primaryResolvedPath,
                std::move(exportResolvedPath)
            }
            );
    }

private:
    [[nodiscard]] static bool isBlank(
        const std::string_view value
        ) noexcept
    {
        if (value.empty())
        {
            return true;
        }

        for (const char character : value)
        {
            if (std::isspace(static_cast<unsigned char>(character)) == 0)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static Domain::OperationError invalidReference(
        const char* message
        )
    {
        return {
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::Result<QString> relativePath(
        const Application::DocumentContentReference& reference
        )
    {
        const std::string_view value = reference.value();
        if (isBlank(value)
            || value.size() > Application::kDocumentContentMaxReferenceLength)
        {
            return Domain::Result<QString>::failure(
                invalidReference(
                    "Document content reference must be non-blank and bounded."
                    )
                );
        }

        if (!value.starts_with(kDocumentContentResourceScheme))
        {
            return Domain::Result<QString>::failure(
                invalidReference(
                    "Document content reference must use resource://documents/."
                    )
                );
        }

        const std::string_view relativeValue = value.substr(
            kDocumentContentResourceScheme.size()
            );
        if (isBlank(relativeValue)
            || relativeValue.find('\0') != std::string_view::npos)
        {
            return Domain::Result<QString>::failure(
                invalidReference(
                    "Document content reference must contain a relative path."
                    )
                );
        }

        const QString relativePath = QString::fromUtf8(
            relativeValue.data(),
            static_cast<qsizetype>(relativeValue.size())
            );
        QString normalizedPath = relativePath;
        normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

        if (normalizedPath.trimmed().isEmpty()
            || normalizedPath.startsWith(QLatin1Char('/'))
            || normalizedPath.contains(QLatin1Char(':'))
            || normalizedPath.contains(QLatin1Char('?'))
            || normalizedPath.contains(QLatin1Char('#'))
            || QDir::isAbsolutePath(normalizedPath))
        {
            return Domain::Result<QString>::failure(
                invalidReference(
                    "Document content reference must be a relative resource path."
                    )
                );
        }

        const QStringList components = normalizedPath.split(
            QLatin1Char('/'),
            Qt::KeepEmptyParts
            );
        for (const QString& component : components)
        {
            if (component.isEmpty()
                || component == QLatin1Char('.')
                || component == QLatin1String(".."))
            {
                return Domain::Result<QString>::failure(
                    invalidReference(
                        "Document content reference contains an invalid path segment."
                        )
                    );
            }
        }

        return Domain::Result<QString>::success(std::move(normalizedPath));
    }

    ResourcePackManager& m_resourcePacks;
};

} // namespace ClassMngr::Next::Platform
