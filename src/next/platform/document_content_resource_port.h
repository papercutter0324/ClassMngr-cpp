#pragma once

#include "next/application/document_content_session.h"
#include "next/domain/document_content_reference.h"
#include "next/domain/operation_result.h"
#include "next/resources/resource_loader.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringConverter>

#include <cctype>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Platform
{

inline constexpr std::string_view kDocumentContentResourceScheme =
    "resource://documents/";

// Owns the loader's scoped stream for one selected PDF body. The stream owns
// the open QFile; consumers may borrow its QIODevice only while this handle
// remains alive.
struct DocumentContentResource final
{
    Resources::ScopedResource<Resources::ResourceFileStream> primaryBody;
};

using DocumentContentResourceResult =
    Domain::Result<DocumentContentResource>;

// Resolves a selected document reference to its typed v2 PDF body record and
// opens only that body for the lifetime of the requested viewer session.
class DocumentContentResourcePort final
{
public:
    explicit DocumentContentResourcePort(
        Resources::ResourceLoader& loader
        ) noexcept
        : m_loader(loader)
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

    [[nodiscard]] DocumentContentResourceResult openPrimaryPdf(
        const Domain::DocumentContentReference& primaryReference,
        QString viewerSessionLabel
        ) const
    {
        const auto parsedPath = relativePath(primaryReference);
        if (!parsedPath)
        {
            return DocumentContentResourceResult::failure(
                parsedPath.error()
                );
        }

        const QString relative = parsedPath.value();
        if (QFileInfo(relative).suffix().compare(
                QStringLiteral("pdf"),
                Qt::CaseInsensitive
                ) != 0)
        {
            return DocumentContentResourceResult::failure(failure(
                Domain::ErrorCode::Validation,
                "The selected document body is not a PDF resource.",
                false
                ));
        }

        const QString logicalId =
            QStringLiteral("document.body:") + relative;
        const auto resourceId = Resources::ResourceId::fromString(logicalId);
        if (!resourceId)
        {
            return DocumentContentResourceResult::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "The selected document reference cannot be represented as a resource ID.",
                false
                ));
        }

        const Resources::ResourceDescriptor* const descriptor =
            m_loader.catalog().find(*resourceId);
        if (!descriptor)
        {
            return DocumentContentResourceResult::failure(failure(
                Domain::ErrorCode::NotFound,
                "The selected PDF body is not present in the resource catalog.",
                true
                ));
        }

        const QString expectedPackagePath =
            QStringLiteral("assets/documents/") + relative;
        if (descriptor->category() != QStringLiteral("documents")
            || descriptor->resourceKind() != QStringLiteral("document_body")
            || descriptor->expectedType() != Resources::ResourceType::Pdf
            || descriptor->expectedTypeName() != QStringLiteral("pdf")
            || descriptor->packageRelativePath() != expectedPackagePath)
        {
            return DocumentContentResourceResult::failure(failure(
                Domain::ErrorCode::Validation,
                "The selected resource catalog record is not a documents PDF body.",
                false
                ));
        }

        auto scope = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::ViewerSession,
            std::move(viewerSessionLabel)
            );
        if (!scope)
        {
            return DocumentContentResourceResult::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "A nonempty viewer session label is required to open a document body.",
                false
                ));
        }

        auto request = Resources::ResourceRequest::create(
            *resourceId,
            std::move(*scope)
            );
        if (!request)
        {
            return DocumentContentResourceResult::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "The selected PDF body resource request is invalid.",
                false
                ));
        }

        auto stream = m_loader.openStream(*request);
        if (!stream)
        {
            return DocumentContentResourceResult::failure(
                mapResourceError(stream.error())
                );
        }

        return DocumentContentResourceResult::success(
            DocumentContentResource{std::move(*stream)}
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
                "Document content reference must be non-blank and bounded.",
                false
                ));
        }

        if (!value.starts_with(kDocumentContentResourceScheme))
        {
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document content reference must use resource://documents/.",
                false
                ));
        }

        const std::string_view relativeValue = value.substr(
            kDocumentContentResourceScheme.size()
            );
        if (isBlank(relativeValue)
            || relativeValue.find('\0') != std::string_view::npos)
        {
            return Domain::Result<QString>::failure(failure(
                Domain::ErrorCode::InvalidInput,
                "Document content reference must contain a relative path.",
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
                "Document content reference must contain valid UTF-8.",
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
                "Document content reference must be a relative resource path.",
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
                    "Document content reference contains an invalid path segment.",
                    false
                    ));
            }
        }

        return Domain::Result<QString>::success(std::move(normalizedPath));
    }

    Resources::ResourceLoader& m_loader;
};

} // namespace ClassMngr::Next::Platform
