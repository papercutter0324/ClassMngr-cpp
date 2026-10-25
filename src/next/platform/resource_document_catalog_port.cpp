#include "next/platform/resource_document_catalog_port.h"

#include "next/resources/resource_loader.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

using Projection = Application::DocumentCatalogProjection;
using ProjectionResult = Domain::Result<Projection>;

struct AssetMetadata final
{
    QString path;
    QString fileName;
};

[[nodiscard]] ProjectionResult failure(
    const Domain::ErrorCode code,
    const char* message
    )
{
    return ProjectionResult::failure({
        .code = code,
        .message = message,
        .recoverable = false
    });
}

[[nodiscard]] std::optional<std::string> boundedText(
    const QString& value,
    const std::size_t maximumBytes
    )
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
    {
        return std::nullopt;
    }

    const QByteArray bytes = trimmed.toUtf8();
    if (static_cast<std::size_t>(bytes.size()) > maximumBytes)
    {
        return std::nullopt;
    }
    return bytes.toStdString();
}

[[nodiscard]] bool validIdentifierText(const QString& value)
{
    if (value.isEmpty())
    {
        return false;
    }

    for (const QChar character : value)
    {
        if (!character.isLetterOrNumber()
            && character != QLatin1Char('_')
            && character != QLatin1Char('-'))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::string> identifier(
    const QJsonValue& value
    )
{
    if (!value.isString())
    {
        return std::nullopt;
    }

    const QString text = value.toString().trimmed();
    const auto bounded = boundedText(
        text,
        Application::kDocumentCatalogMaxIdentifierLength
        );
    if (!bounded || !validIdentifierText(text))
    {
        return std::nullopt;
    }
    return bounded;
}

[[nodiscard]] std::optional<std::int32_t> orderValue(
    const QJsonValue& value
    )
{
    if (!value.isDouble())
    {
        return std::nullopt;
    }

    const qint64 order = value.toInteger(-1);
    if (order < 0
        || order > std::numeric_limits<std::int32_t>::max())
    {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(order);
}

[[nodiscard]] std::optional<QString> relativeDirectoryPath(
    const QJsonValue& value
    )
{
    if (!value.isString())
    {
        return std::nullopt;
    }

    QString path = value.toString().trimmed();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.isEmpty() || QDir::isAbsolutePath(path)
        || path.startsWith(QLatin1Char('/')))
    {
        return std::nullopt;
    }

    const QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const QString& part : parts)
    {
        if (part.isEmpty() || part == QStringLiteral(".")
            || part == QStringLiteral(".."))
        {
            return std::nullopt;
        }
    }

    path = QDir::cleanPath(path);
    const auto bounded = boundedText(
        path,
        Application::kDocumentCatalogMaxPathLength
        );
    if (!bounded || path == QStringLiteral(".")
        || path.startsWith(QStringLiteral("../"))
        || path.contains(QStringLiteral("/../")))
    {
        return std::nullopt;
    }
    return path;
}

[[nodiscard]] std::optional<QString> plainFileName(
    const QJsonValue& value
    )
{
    if (!value.isString())
    {
        return std::nullopt;
    }

    const QString fileName = value.toString().trimmed();
    if (fileName.isEmpty() || fileName == QStringLiteral(".")
        || fileName == QStringLiteral("..")
        || fileName.contains(QLatin1Char('/'))
        || fileName.contains(QLatin1Char('\\'))
        || !boundedText(fileName, Application::kDocumentCatalogMaxPathLength))
    {
        return std::nullopt;
    }
    return fileName;
}

[[nodiscard]] std::optional<AssetMetadata> assetMetadata(
    const QJsonValue& value
    )
{
    if (!value.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject object = value.toObject();
    const auto path = relativeDirectoryPath(
        object.value(QStringLiteral("path"))
        );
    const auto fileName = plainFileName(
        object.value(QStringLiteral("fileName"))
        );
    if (!path || !fileName)
    {
        return std::nullopt;
    }
    return AssetMetadata{*path, *fileName};
}

[[nodiscard]] QString assetRelativePath(const AssetMetadata& asset)
{
    return asset.path + QLatin1Char('/') + asset.fileName;
}

[[nodiscard]] std::optional<std::string> resourceReference(
    const AssetMetadata& asset
    )
{
    return boundedText(
        QStringLiteral("resource://documents/") + assetRelativePath(asset),
        Application::kDocumentCatalogMaxReferenceLength
        );
}

[[nodiscard]] QString normalizedLocale(QString localeName)
{
    localeName = localeName.trimmed();
    localeName.replace(QLatin1Char('-'), QLatin1Char('_'));
    return localeName;
}

[[nodiscard]] std::optional<QString> localizedName(
    const QJsonValue& value,
    const QString& localeName
    )
{
    if (!value.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject names = value.toObject();
    const auto defaultName = names.value(QStringLiteral("default"));
    if (!defaultName.isString()
        || !boundedText(
            defaultName.toString(),
            Application::kDocumentCatalogMaxDisplayNameLength
            ))
    {
        return std::nullopt;
    }

    std::unordered_map<std::string, QString> translations;
    translations.reserve(static_cast<std::size_t>(names.size()));
    for (auto it = names.constBegin(); it != names.constEnd(); ++it)
    {
        if (it.key() == QStringLiteral("default"))
        {
            continue;
        }
        if (!it.value().isString())
        {
            return std::nullopt;
        }

        const QString key = normalizedLocale(it.key());
        const auto translated = boundedText(
            it.value().toString(),
            Application::kDocumentCatalogMaxDisplayNameLength
            );
        const auto boundedKey = boundedText(
            key,
            Application::kDocumentCatalogMaxKeyLength
            );
        if (!translated || !boundedKey)
        {
            return std::nullopt;
        }
        translations.emplace(*boundedKey, QString::fromUtf8(
            translated->data(), static_cast<qsizetype>(translated->size())
            ));
    }

    const QString requestedLocale = normalizedLocale(localeName);
    const auto exactLocale = boundedText(
        requestedLocale,
        Application::kDocumentCatalogMaxKeyLength
        );
    if (exactLocale)
    {
        const auto exact = translations.find(*exactLocale);
        if (exact != translations.end())
        {
            return exact->second;
        }
    }

    const QString language = requestedLocale.section(QLatin1Char('_'), 0, 0);
    const auto languageKey = boundedText(
        language,
        Application::kDocumentCatalogMaxKeyLength
        );
    if (languageKey)
    {
        const auto languageMatch = translations.find(*languageKey);
        if (languageMatch != translations.end())
        {
            return languageMatch->second;
        }
    }

    return defaultName.toString().trimmed();
}

[[nodiscard]] std::optional<std::int32_t> schemaVersion(
    const QJsonValue& value
    )
{
    return orderValue(value);
}

[[nodiscard]] ProjectionResult projectMetadata(
    const QJsonDocument& document,
    const QString& localeName
    )
{
    if (!document.isObject())
    {
        return failure(
            Domain::ErrorCode::InvalidInput,
            "The document catalog metadata root must be an object."
            );
    }

    const QJsonObject root = document.object();
    const auto version = schemaVersion(
        root.value(QStringLiteral("schemaVersion"))
        );
    const QJsonValue foldersValue = root.value(QStringLiteral("folders"));
    const QJsonValue documentsValue = root.value(QStringLiteral("documents"));
    if (!version || *version != 1 || !foldersValue.isArray()
        || !documentsValue.isArray())
    {
        return failure(
            Domain::ErrorCode::InvalidInput,
            "The document catalog metadata schema is unsupported or incomplete."
            );
    }

    const QJsonArray folders = foldersValue.toArray();
    const QJsonArray documents = documentsValue.toArray();
    if (static_cast<std::size_t>(folders.size())
            > Application::kDocumentCatalogMaxFolderEntries
        || static_cast<std::size_t>(documents.size())
            > Application::kDocumentCatalogMaxDocumentEntries)
    {
        return failure(
            Domain::ErrorCode::InvalidInput,
            "The document catalog metadata exceeds its bounded entry limits."
            );
    }

    Application::DocumentCatalogProjectionInput input;
    input.folders.reserve(static_cast<std::size_t>(folders.size()));
    std::unordered_map<std::string, Domain::DocumentFolderId> folderIdsByPath;
    folderIdsByPath.reserve(static_cast<std::size_t>(folders.size()));

    for (const QJsonValue& folderValue : folders)
    {
        if (!folderValue.isObject())
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "The document catalog contains a malformed folder entry."
                );
        }

        const QJsonObject folder = folderValue.toObject();
        const auto idText = identifier(folder.value(QStringLiteral("id")));
        const auto path = relativeDirectoryPath(
            folder.value(QStringLiteral("path"))
            );
        const auto order = orderValue(folder.value(QStringLiteral("order")));
        const auto displayName = localizedName(
            folder.value(QStringLiteral("sidebarNames")),
            localeName
            );
        const auto id = idText
            ? Domain::DocumentFolderId::fromString(*idText)
            : std::nullopt;
        const auto pathText = path
            ? boundedText(*path, Application::kDocumentCatalogMaxPathLength)
            : std::nullopt;
        const auto key = idText
            ? boundedText(
                folder.value(QStringLiteral("id")).toString().trimmed(),
                Application::kDocumentCatalogMaxKeyLength
                )
            : std::nullopt;
        const auto nameText = displayName
            ? boundedText(*displayName, Application::kDocumentCatalogMaxDisplayNameLength)
            : std::nullopt;
        if (!id || !path || !order || !pathText || !key || !nameText)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "A document folder contains invalid or unbounded metadata."
                );
        }

        const int separator = path->lastIndexOf(QLatin1Char('/'));
        const QString parentPath = separator < 0
            ? QString{}
            : path->left(separator);
        const auto parentPathText = parentPath.isEmpty()
            ? std::optional<std::string>{std::string{}}
            : boundedText(parentPath, Application::kDocumentCatalogMaxPathLength);
        if (!parentPathText
            || !folderIdsByPath.emplace(*pathText, *id).second)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "A document folder path is duplicated or unbounded."
                );
        }

        input.folders.push_back({
            *id,
            Application::DocumentCatalogPath(*pathText),
            Application::DocumentFolderKey(*key),
            *nameText,
            Application::DocumentFolderOrder(*order),
            Application::DocumentCatalogPath(*parentPathText)
        });
    }

    input.documents.reserve(static_cast<std::size_t>(documents.size()));
    for (const QJsonValue& documentValue : documents)
    {
        if (!documentValue.isObject())
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "The document catalog contains a malformed document entry."
                );
        }

        const QJsonObject item = documentValue.toObject();
        const auto idText = identifier(item.value(QStringLiteral("id")));
        const auto id = idText
            ? Domain::DocumentId::fromString(*idText)
            : std::nullopt;
        const auto order = orderValue(item.value(QStringLiteral("order")));
        const auto displayName = localizedName(
            item.value(QStringLiteral("sidebarNames")),
            localeName
            );
        const auto primary = assetMetadata(item.value(QStringLiteral("pdf")));
        const QJsonValue printingValue = item.value(QStringLiteral("printingEnabled"));
        const QJsonValue exportingValue = item.value(QStringLiteral("exportingEnabled"));
        if (!id || !order || !displayName || !primary
            || !printingValue.isBool() || !exportingValue.isBool()
            || !primary->fileName.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "A document entry contains invalid or unbounded metadata."
                );
        }

        const QString primaryRelativePath = assetRelativePath(*primary);
        const auto path = boundedText(
            primaryRelativePath,
            Application::kDocumentCatalogMaxPathLength
            );
        const auto key = idText->size()
                <= Application::kDocumentCatalogMaxKeyLength
            ? idText
            : std::nullopt;
        const auto nameText = boundedText(
            *displayName,
            Application::kDocumentCatalogMaxDisplayNameLength
            );
        const auto primaryReference = resourceReference(*primary);
        const auto folderPath = boundedText(
            primary->path,
            Application::kDocumentCatalogMaxPathLength
            );
        const auto folder = folderPath
            ? folderIdsByPath.find(*folderPath)
            : folderIdsByPath.end();
        if (!path || !key || !nameText || !primaryReference
            || folder == folderIdsByPath.end())
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "A document entry references an invalid or unknown folder."
                );
        }

        const bool exportable = exportingValue.toBool();
        std::optional<Domain::DocumentContentReference> exportReference;
        if (exportable)
        {
            const auto exportAsset = assetMetadata(item.value(QStringLiteral("export")));
            const auto exportText = exportAsset
                ? resourceReference(*exportAsset)
                : std::nullopt;
            if (!exportAsset || !exportText)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "An exportable document has invalid export metadata."
                    );
            }
            exportReference.emplace(*exportText);
        }

        input.documents.push_back({
            *id,
            folder->second,
            Application::DocumentCatalogPath(*path),
            Application::DocumentEntryKey(*key),
            *nameText,
            Application::DocumentOrder(*order),
            printingValue.toBool(),
            exportable,
            Domain::DocumentContentReference(*primaryReference),
            std::move(exportReference)
        });
    }

    auto result = Projection::create(std::move(input));
    if (!result)
    {
        return failure(
            Domain::ErrorCode::InvalidInput,
            "The mapped document catalog projection failed validation."
            );
    }
    return result;
}

[[nodiscard]] Domain::ErrorCode domainErrorCode(
    const Resources::ResourceErrorCode code
    ) noexcept
{
    using Resources::ResourceErrorCode;
    switch (code)
    {
    case ResourceErrorCode::NotFound:
    case ResourceErrorCode::MissingFile:
        return Domain::ErrorCode::NotFound;
    case ResourceErrorCode::InvalidArgument:
    case ResourceErrorCode::TypeMismatch:
    case ResourceErrorCode::UnsafePath:
    case ResourceErrorCode::CorruptResource:
        return Domain::ErrorCode::InvalidInput;
    case ResourceErrorCode::InvalidCatalog:
    case ResourceErrorCode::IoError:
    case ResourceErrorCode::UnsupportedOperation:
    case ResourceErrorCode::DecodeError:
    case ResourceErrorCode::RegistrationFailed:
        return Domain::ErrorCode::Technical;
    }
    return Domain::ErrorCode::Technical;
}

} // namespace

Domain::Result<Application::DocumentCatalogProjection>
ResourceDocumentCatalogPort::projection(const QString& localeName) const
{
    try
    {
        const auto id = Resources::ResourceId::fromString(
            QStringLiteral("document.catalog-metadata")
            );
        if (!id)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The document catalog metadata resource ID is invalid."
                );
        }

        auto scope = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::Startup,
            QStringLiteral("DocumentCatalogMetadata")
            );
        if (!scope)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The document catalog metadata resource scope is invalid."
                );
        }

        auto request = Resources::ResourceRequest::create(*id, std::move(*scope));
        if (!request)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The document catalog metadata resource request is invalid."
                );
        }

        auto loaded = m_loader.loadJson(*request);
        if (!loaded)
        {
            return failure(
                domainErrorCode(loaded.error().code),
                "The document catalog metadata resource could not be loaded."
                );
        }
        return projectMetadata(loaded->get(), localeName);
    }
    catch (const std::exception&)
    {
        return failure(
            Domain::ErrorCode::Technical,
            "The document catalog metadata could not be projected."
            );
    }
    catch (...)
    {
        return failure(
            Domain::ErrorCode::Technical,
            "The document catalog metadata could not be projected."
            );
    }
}

} // namespace ClassMngr::Next::Platform
