#pragma once

#include "core/resource_packs/resource_pack_manager.h"
#include "next/application/document_content_session.h"
#include "next/domain/document_content_reference.h"
#include "next/domain/operation_result.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringConverter>

#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Platform
{

inline constexpr std::string_view kLegacyDocumentContentResourceScheme =
    "resource://documents/";

// Pack-backed path boundary retained for the current legacy navigation flow.
// The ResourceLoader-backed Next port is separate and not yet used by the live
// legacy flow pending feature migration.
struct LegacyDocumentContentResource final
{
    ResourcePackLease resourceLease;
    QString primaryPath;
    std::optional<QString> exportPath;
};

using LegacyDocumentContentResourceResult =
    Domain::Result<LegacyDocumentContentResource>;

class LegacyDocumentContentResourcePort final
{
public:
    explicit LegacyDocumentContentResourcePort(
        ResourcePackManager& resourcePacks
        ) noexcept
        : m_resourcePacks(resourcePacks)
    {
    }

    LegacyDocumentContentResourcePort(
        const LegacyDocumentContentResourcePort&
        ) = delete;
    LegacyDocumentContentResourcePort& operator=(
        const LegacyDocumentContentResourcePort&
        ) = delete;
    LegacyDocumentContentResourcePort(
        LegacyDocumentContentResourcePort&&
        ) = delete;
    LegacyDocumentContentResourcePort& operator=(
        LegacyDocumentContentResourcePort&&
        ) = delete;

    [[nodiscard]] LegacyDocumentContentResourceResult resolve(
        const Domain::DocumentContentReference& primaryReference,
        const std::optional<Domain::DocumentContentReference>& exportReference =
            std::nullopt
        ) const
    {
        const auto primaryPath = relativePath(primaryReference);
        if (!primaryPath)
        {
            return LegacyDocumentContentResourceResult::failure(
                primaryPath.error()
                );
        }

        std::optional<QString> exportRelativePath;
        if (exportReference)
        {
            const auto parsedExportPath = relativePath(*exportReference);
            if (!parsedExportPath)
            {
                return LegacyDocumentContentResourceResult::failure(
                    parsedExportPath.error()
                    );
            }
            exportRelativePath = parsedExportPath.value();
        }

        auto lease = m_resourcePacks.acquire(QStringLiteral("documents"));
        if (!lease)
        {
            return LegacyDocumentContentResourceResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The documents resource pack could not be acquired: "
                    + lease.error().toUtf8().toStdString(),
                .recoverable = true
            });
        }

        const QString resolvedPrimaryPath = QDir(lease->root()).filePath(
            primaryPath.value()
            );
        if (!QFileInfo(resolvedPrimaryPath).isFile())
        {
            return LegacyDocumentContentResourceResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The primary document resource does not exist.",
                .recoverable = true
            });
        }

        std::optional<QString> resolvedExportPath;
        if (exportRelativePath)
        {
            const QString candidate = QDir(lease->root()).filePath(
                *exportRelativePath
                );
            if (!QFileInfo(candidate).isFile())
            {
                return LegacyDocumentContentResourceResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "The export document resource does not exist.",
                    .recoverable = true
                });
            }
            resolvedExportPath = candidate;
        }

        return LegacyDocumentContentResourceResult::success(
            LegacyDocumentContentResource{
                std::move(*lease),
                resolvedPrimaryPath,
                std::move(resolvedExportPath)
            }
            );
    }

private:
    [[nodiscard]] static bool isBlank(const std::string_view value) noexcept
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
        const Domain::DocumentContentReference& reference
        )
    {
        const std::string_view value = reference.value();
        if (isBlank(value)
            || value.size() > Application::kDocumentContentMaxReferenceLength)
        {
            return Domain::Result<QString>::failure(invalidReference(
                "Document content reference must be non-blank and bounded."
                ));
        }

        if (!value.starts_with(kLegacyDocumentContentResourceScheme))
        {
            return Domain::Result<QString>::failure(invalidReference(
                "Document content reference must use resource://documents/."
                ));
        }

        const std::string_view relativeValue = value.substr(
            kLegacyDocumentContentResourceScheme.size()
            );
        if (isBlank(relativeValue)
            || relativeValue.find('\0') != std::string_view::npos)
        {
            return Domain::Result<QString>::failure(invalidReference(
                "Document content reference must contain a relative path."
                ));
        }

        QStringDecoder utf8Decoder(
            QStringDecoder::Utf8,
            QStringConverter::Flag::Stateless
            );
        const QString decodedPath = utf8Decoder(QByteArrayView(
            relativeValue.data(),
            static_cast<qsizetype>(relativeValue.size())
            ));
        if (utf8Decoder.hasError())
        {
            return Domain::Result<QString>::failure(invalidReference(
                "Document content reference must contain valid UTF-8."
                ));
        }

        QString normalizedPath = decodedPath;
        normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (normalizedPath.trimmed().isEmpty()
            || normalizedPath.startsWith(QLatin1Char('/'))
            || normalizedPath.contains(QLatin1Char(':'))
            || normalizedPath.contains(QLatin1Char('?'))
            || normalizedPath.contains(QLatin1Char('#'))
            || normalizedPath.contains(QChar::Null)
            || QDir::isAbsolutePath(normalizedPath))
        {
            return Domain::Result<QString>::failure(invalidReference(
                "Document content reference must be a relative resource path."
                ));
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
                return Domain::Result<QString>::failure(invalidReference(
                    "Document content reference contains an invalid path segment."
                    ));
            }
        }

        return Domain::Result<QString>::success(std::move(normalizedPath));
    }

    ResourcePackManager& m_resourcePacks;
};

} // namespace ClassMngr::Next::Platform
