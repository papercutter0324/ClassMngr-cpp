#pragma once

#include "next/application/document_content_session.h"
#include "next/domain/document_content_reference.h"
#include "next/domain/operation_result.h"
#include "next/resources/resource_loader.h"

#include <QByteArrayView>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringConverter>

#include <cctype>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Platform
{

inline constexpr std::string_view kDocumentExportResourceScheme =
    "resource://documents/";

using DocumentExportResourceHandle =
    Resources::ScopedResource<Resources::ResourceFileStream>;
using DocumentExportResourceResult =
    Domain::Result<DocumentExportResourceHandle>;

// Opens only the selected cataloged Office export for one operation. The
// returned move-only handle owns its package file stream for the operation's
// lifetime.
class DocumentExportResourcePort final
{
public:
    explicit DocumentExportResourcePort(
        Resources::ResourceLoader& loader
        ) noexcept
        : m_loader(loader)
    {
    }

    DocumentExportResourcePort(const DocumentExportResourcePort&) = delete;
    DocumentExportResourcePort& operator=(const DocumentExportResourcePort&) = delete;
    DocumentExportResourcePort(DocumentExportResourcePort&&) = delete;
    DocumentExportResourcePort& operator=(DocumentExportResourcePort&&) = delete;

    [[nodiscard]] DocumentExportResourceResult openExport(
        const Domain::DocumentContentReference& exportReference,
        QString operationOwnerLabel
        ) const
    {
        const auto parsedPath = relativePath(exportReference);
        if (!parsedPath)
        {
            return DocumentExportResourceResult::failure(parsedPath.error());
        }

        const QString relative = parsedPath.value();
        const QString suffix = QFileInfo(relative).suffix();
        if (suffix.compare(QStringLiteral("pptx"), Qt::CaseInsensitive) != 0
            && suffix.compare(QStringLiteral("docx"), Qt::CaseInsensitive) != 0)
        {
            return DocumentExportResourceResult::failure(failure(
                Domain::ErrorCode::Validation,
                "The selected document export is not a PPTX or DOCX resource.",
                false
                ));
        }

        const QString logicalId = QStringLiteral("document.body:") + relative;
        const auto resourceId = Resources::ResourceId::fromString(logicalId);
        if (!resourceId)
        {
            return DocumentExportResourceResult::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "The selected document export cannot be represented as a resource ID.",
                false
                ));
        }

        const Resources::ResourceDescriptor* const descriptor =
            m_loader.catalog().find(*resourceId);
        if (!descriptor)
        {
            return DocumentExportResourceResult::failure(failure(
                Domain::ErrorCode::NotFound,
                "The selected document export is not present in the resource catalog.",
                true
                ));
        }

        const bool pptx = descriptor->expectedType() == Resources::ResourceType::Pptx
            && descriptor->expectedTypeName() == QStringLiteral("pptx");
        const bool docx = descriptor->expectedType() == Resources::ResourceType::Docx
            && descriptor->expectedTypeName() == QStringLiteral("docx");
        const QString expectedPackagePath =
            QStringLiteral("assets/documents/") + relative;
        if (descriptor->category() != QStringLiteral("documents")
            || descriptor->resourceKind() != QStringLiteral("document_body")
            || (!pptx && !docx)
            || descriptor->packageRelativePath() != expectedPackagePath)
        {
            return DocumentExportResourceResult::failure(failure(
                Domain::ErrorCode::Validation,
                "The selected resource catalog record is not a matching Office document body.",
                false
                ));
        }

        auto scope = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::Operation,
            std::move(operationOwnerLabel)
            );
        if (!scope)
        {
            return DocumentExportResourceResult::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "A nonempty operation owner label is required to open a document export.",
                false
                ));
        }

        auto request = Resources::ResourceRequest::create(
            *resourceId,
            std::move(*scope)
            );
        if (!request)
        {
            return DocumentExportResourceResult::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "The selected document export resource request is invalid.",
                false
                ));
        }

        auto stream = m_loader.openStream(*request);
        if (!stream)
        {
            return DocumentExportResourceResult::failure(
                mapResourceError(stream.error())
                );
        }

        return DocumentExportResourceResult::success(std::move(*stream));
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

    [[nodiscard]] static Domain::OperationError failure(
        const Domain::ErrorCode code,
        const char* message,
        const bool recoverable
        )
    {
        return {
            .code = code,
            .message = message,
            .recoverable = recoverable
        };
    }

    [[nodiscard]] static Domain::OperationError mapResourceError(
        const Resources::ResourceError& error
        )
    {
        using Resources::ResourceErrorCode;
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        bool recoverable = true;
        switch (error.code)
        {
        case ResourceErrorCode::NotFound:
        case ResourceErrorCode::MissingFile:
            code = Domain::ErrorCode::NotFound;
            break;
        case ResourceErrorCode::InvalidArgument:
        case ResourceErrorCode::TypeMismatch:
        case ResourceErrorCode::UnsafePath:
        case ResourceErrorCode::CorruptResource:
            code = Domain::ErrorCode::Validation;
            recoverable = false;
            break;
        case ResourceErrorCode::InvalidCatalog:
        case ResourceErrorCode::IoError:
        case ResourceErrorCode::UnsupportedOperation:
        case ResourceErrorCode::DecodeError:
        case ResourceErrorCode::RegistrationFailed:
            code = Domain::ErrorCode::Technical;
            break;
        }
        return {
            .code = code,
            .message = error.message.toUtf8().toStdString(),
            .recoverable = recoverable
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
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document export reference must be non-blank and bounded.",
                false
                ));
        }

        if (!value.starts_with(kDocumentExportResourceScheme))
        {
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document export reference must use resource://documents/.",
                false
                ));
        }

        const std::string_view relativeValue = value.substr(
            kDocumentExportResourceScheme.size()
            );
        if (isBlank(relativeValue)
            || relativeValue.find('\0') != std::string_view::npos)
        {
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document export reference must contain a relative path.",
                false
                ));
        }

        QStringDecoder utf8Decoder(
            QStringDecoder::Utf8,
            QStringConverter::Flag::Stateless
            );
        const QString decodedPath = utf8Decoder(
            QByteArrayView(
                relativeValue.data(),
                static_cast<qsizetype>(relativeValue.size())
                )
            );
        if (utf8Decoder.hasError())
        {
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document export reference must contain valid UTF-8.",
                false
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
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document export reference must be a relative resource path.",
                false
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
                return Domain::Result<QString>::failure(failure(
                    Domain::ErrorCode::InvalidInput,
                    "Document export reference contains an invalid path segment.",
                    false
                    ));
            }
        }

        return Domain::Result<QString>::success(std::move(normalizedPath));
    }

    Resources::ResourceLoader& m_loader;
};

} // namespace ClassMngr::Next::Platform
