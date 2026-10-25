#include "resource_catalog.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <limits>
#include <map>
#include <set>
#include <utility>

namespace ClassMngr::Next::Resources
{
namespace
{

constexpr qint64 MaximumCatalogBytes = 16 * 1024 * 1024;

ResourceError catalogError(QString message)
{
    return {ResourceErrorCode::InvalidCatalog, std::move(message)};
}

std::optional<QString> stringField(
    const QJsonObject& object,
    const QString& key
    )
{
    const QJsonValue value = object.value(key);
    if (!value.isString() || value.toString().isEmpty())
    {
        return std::nullopt;
    }
    return value.toString();
}

bool isSha256(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}$"));
    return pattern.match(value).hasMatch();
}

std::optional<qint64> nonNegativeInteger(
    const QJsonObject& object,
    const QString& key
    )
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
    {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (number < 0.0 || number > static_cast<double>(std::numeric_limits<qint64>::max())
        || number != static_cast<double>(static_cast<qint64>(number)))
    {
        return std::nullopt;
    }
    return static_cast<qint64>(number);
}

std::optional<ResourceType> parseType(const QString& value)
{
    if (value == QStringLiteral("json")) return ResourceType::Json;
    if (value == QStringLiteral("pdf")) return ResourceType::Pdf;
    if (value == QStringLiteral("pptx")) return ResourceType::Pptx;
    if (value == QStringLiteral("docx")) return ResourceType::Docx;
    if (value == QStringLiteral("odp")) return ResourceType::Odp;
    if (value == QStringLiteral("odt")) return ResourceType::Odt;
    if (value == QStringLiteral("ods")) return ResourceType::Ods;
    if (value == QStringLiteral("xlsx")) return ResourceType::Xlsx;
    if (value == QStringLiteral("svg")) return ResourceType::Svg;
    if (value == QStringLiteral("png")) return ResourceType::Png;
    if (value == QStringLiteral("icns")) return ResourceType::Icns;
    if (value == QStringLiteral("ico")) return ResourceType::Ico;
    if (value == QStringLiteral("ttf")) return ResourceType::Ttf;
    if (value == QStringLiteral("ttc")) return ResourceType::Ttc;
    if (value == QStringLiteral("stylesheet")) return ResourceType::Stylesheet;
    if (value == QStringLiteral("qm")) return ResourceType::Qm;
    if (value == QStringLiteral("qml_module_component"))
    {
        return ResourceType::QmlModuleComponent;
    }
    return std::nullopt;
}

QString expectedExtension(ResourceType type)
{
    switch (type)
    {
    case ResourceType::Json: return QStringLiteral(".json");
    case ResourceType::Pdf: return QStringLiteral(".pdf");
    case ResourceType::Pptx: return QStringLiteral(".pptx");
    case ResourceType::Docx: return QStringLiteral(".docx");
    case ResourceType::Odp: return QStringLiteral(".odp");
    case ResourceType::Odt: return QStringLiteral(".odt");
    case ResourceType::Ods: return QStringLiteral(".ods");
    case ResourceType::Xlsx: return QStringLiteral(".xlsx");
    case ResourceType::Svg: return QStringLiteral(".svg");
    case ResourceType::Png: return QStringLiteral(".png");
    case ResourceType::Icns: return QStringLiteral(".icns");
    case ResourceType::Ico: return QStringLiteral(".ico");
    case ResourceType::Ttf: return QStringLiteral(".ttf");
    case ResourceType::Ttc: return QStringLiteral(".ttc");
    case ResourceType::Stylesheet: return QStringLiteral(".qss");
    case ResourceType::Qm: return QStringLiteral(".qm");
    case ResourceType::QmlModuleComponent: return QStringLiteral(".qml");
    }
    return {};
}

bool isCanonicalRelativePosixPath(const QString& path)
{
    if (path.isEmpty() || path.contains(QLatin1Char('\\')) || path.contains(QChar::Null)
        || path.startsWith(QLatin1Char('/')) || path.contains(QStringLiteral("//")))
    {
        return false;
    }
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')
        && path.at(0).isLetter())
    {
        return false;
    }

    const QStringList parts = path.split(QLatin1Char('/'));
    for (const QString& part : parts)
    {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral(".."))
        {
            return false;
        }
    }

    return QDir::cleanPath(path) == path;
}

bool validateGeneration(
    const QJsonObject& record,
    const QString& expectedGenerator
    )
{
    const QJsonValue generationValue = record.value(QStringLiteral("generation"));
    if (!generationValue.isObject())
    {
        return false;
    }
    const QJsonObject generation = generationValue.toObject();
    return generation.value(QStringLiteral("status")).toString()
            == QStringLiteral("expected_generated_output")
        && generation.value(QStringLiteral("generator")).toString() == expectedGenerator;
}

}

ResourceResult<ResourceDescriptor> ResourceCatalog::parseRecord(const QJsonValue& value)
{
    if (!value.isObject())
    {
        return std::unexpected(catalogError(QStringLiteral("catalog record must be an object")));
    }

    const QJsonObject record = value.toObject();
    const auto logicalId = stringField(record, QStringLiteral("logical_id"));
    const auto path = stringField(record, QStringLiteral("package_relative_path"));
    const auto category = stringField(record, QStringLiteral("category"));
    const auto resourceKind = stringField(record, QStringLiteral("resource_kind"));
    const auto expectedTypeName = stringField(record, QStringLiteral("expected_type"));
    const auto owner = stringField(record, QStringLiteral("owner"));
    const auto startupNecessity = stringField(record, QStringLiteral("startup_necessity"));
    const auto locationName = stringField(record, QStringLiteral("location"));
    if (!logicalId || !path || !category || !resourceKind || !expectedTypeName || !owner
        || !startupNecessity || !locationName || !isCanonicalRelativePosixPath(*path))
    {
        return std::unexpected(catalogError(
            QStringLiteral("catalog record has missing fields or a non-canonical path")
            ));
    }

    const auto id = ResourceId::fromString(*logicalId);
    const auto expectedType = parseType(*expectedTypeName);
    if (!id || !expectedType)
    {
        return std::unexpected(catalogError(
            QStringLiteral("catalog record has an invalid logical ID or expected type")
            ));
    }

    const QJsonValue loadingValue = record.value(QStringLiteral("loading"));
    const QJsonValue streamabilityValue = record.value(QStringLiteral("streamability"));
    const QJsonValue provenanceValue = record.value(QStringLiteral("provenance"));
    if (!loadingValue.isObject() || !streamabilityValue.isObject()
        || !provenanceValue.isObject())
    {
        return std::unexpected(catalogError(
            QStringLiteral("catalog record is missing loading, streamability, or provenance metadata")
            ));
    }

    const QJsonObject loading = loadingValue.toObject();
    const auto loadingTier = stringField(loading, QStringLiteral("tier"));
    const auto loadingPolicy = stringField(loading, QStringLiteral("policy"));
    const auto lifetime = stringField(loading, QStringLiteral("lifetime"));
    const auto releasePolicy = stringField(loading, QStringLiteral("release_policy"));
    const QJsonObject streamability = streamabilityValue.toObject();
    const auto streamabilityStatus = stringField(streamability, QStringLiteral("status"));
    const auto streamabilityMethod = stringField(streamability, QStringLiteral("method_ref"));
    const QJsonObject provenance = provenanceValue.toObject();
    const QJsonValue sourceValue = provenance.value(QStringLiteral("source"));
    if (!loadingTier || !loadingPolicy || !lifetime || !releasePolicy
        || !streamabilityStatus || !streamabilityMethod || !sourceValue.isObject())
    {
        return std::unexpected(catalogError(
            QStringLiteral("catalog record has incomplete required metadata")
            ));
    }

    const QJsonObject source = sourceValue.toObject();
    const auto sourcePath = stringField(source, QStringLiteral("path"));
    const auto sourceBytes = nonNegativeInteger(source, QStringLiteral("bytes"));
    const auto sourceSha256 = stringField(source, QStringLiteral("sha256"));
    if (!sourcePath || !sourceBytes || !sourceSha256 || !isSha256(*sourceSha256))
    {
        return std::unexpected(catalogError(
            QStringLiteral("catalog record has invalid source provenance")
            ));
    }

    ResourceLocation location;
    QString runtimeUrl;
    QString generationStatus;
    QString generationGenerator;
    std::optional<qint64> expectedBytes;
    std::optional<QByteArray> expectedSha256;
    if (*locationName == QStringLiteral("resource_file"))
    {
        location = ResourceLocation::PackageFile;
        if (record.contains(QStringLiteral("runtime_url"))
            || *expectedType == ResourceType::QmlModuleComponent
            || QFileInfo(*path).suffix().compare(
                expectedExtension(*expectedType).mid(1), Qt::CaseInsensitive
                ) != 0)
        {
            return std::unexpected(catalogError(
                QStringLiteral("package resource type does not match its path extension")
                ));
        }

        const bool generatedTranslation =
            *resourceKind == QStringLiteral("translation_source_for_generated_qm");
        const QJsonValue bakedValue = provenance.value(QStringLiteral("baked"));
        if (generatedTranslation)
        {
            if (*expectedType != ResourceType::Qm
                || !path->startsWith(QStringLiteral("assets/translations/"))
                || !validateGeneration(record, QStringLiteral("qt_linguist"))
                || !bakedValue.isUndefined())
            {
                return std::unexpected(catalogError(
                    QStringLiteral("generated translation record is inconsistent")
                    ));
            }
            const QJsonObject generation = record.value(QStringLiteral("generation")).toObject();
            generationStatus = generation.value(QStringLiteral("status")).toString();
            generationGenerator = generation.value(QStringLiteral("generator")).toString();
        }
        else
        {
            if (bakedValue.isUndefined() || !bakedValue.isObject()
                || record.contains(QStringLiteral("generation")))
            {
                return std::unexpected(catalogError(
                    QStringLiteral("direct package record is missing baked provenance")
                    ));
            }
            const QJsonObject baked = bakedValue.toObject();
            const auto bytes = nonNegativeInteger(baked, QStringLiteral("bytes"));
            const auto sha256 = stringField(baked, QStringLiteral("sha256"));
            if (!bytes || !sha256 || !isSha256(*sha256))
            {
                return std::unexpected(catalogError(
                    QStringLiteral("direct package record has invalid baked provenance")
                    ));
            }
            expectedBytes = *bytes;
            expectedSha256 = QByteArray::fromHex(sha256->toLatin1());
        }
    }
    else if (*locationName == QStringLiteral("qt_resource"))
    {
        location = ResourceLocation::QtResource;
        const auto url = stringField(record, QStringLiteral("runtime_url"));
        if (*expectedType != ResourceType::QmlModuleComponent
            || *resourceKind != QStringLiteral("calendar_qml_module_source")
            || !path->startsWith(QStringLiteral("qt/qml/ClassMngr/Calendar/"))
            || QFileInfo(*path).suffix().compare(QStringLiteral("qml"), Qt::CaseInsensitive) != 0
            || !url || *url != QStringLiteral("qrc:/") + *path
            || !validateGeneration(record, QStringLiteral("qt_qml_module"))
            || !provenance.value(QStringLiteral("baked")).isUndefined())
        {
            return std::unexpected(catalogError(
                QStringLiteral("Qt resource record has inconsistent URL, type, or generation metadata")
                ));
        }
        runtimeUrl = *url;
        const QJsonObject generation = record.value(QStringLiteral("generation")).toObject();
        generationStatus = generation.value(QStringLiteral("status")).toString();
        generationGenerator = generation.value(QStringLiteral("generator")).toString();
    }
    else
    {
        return std::unexpected(catalogError(QStringLiteral("unknown catalog resource location")));
    }

    if (*streamabilityStatus != QStringLiteral("no")
        && *streamabilityStatus != QStringLiteral("candidate_file_backed")
        && *streamabilityStatus != QStringLiteral("consumer_dependent"))
    {
        return std::unexpected(catalogError(QStringLiteral("unknown streamability status")));
    }

    return ResourceDescriptor(
        *id,
        *path,
        location,
        *expectedType,
        *expectedTypeName,
        *category,
        *resourceKind,
        *owner,
        *startupNecessity,
        *loadingTier,
        *loadingPolicy,
        *lifetime,
        *releasePolicy,
        *streamabilityStatus,
        *streamabilityMethod,
        runtimeUrl,
        *sourcePath,
        *sourceBytes,
        QByteArray::fromHex(sourceSha256->toLatin1()),
        generationStatus,
        generationGenerator,
        expectedBytes,
        expectedSha256
        );
}

struct ResourceCatalog::Data final
{
    explicit Data(std::vector<ResourceDescriptor> inputRecords)
        : records(std::move(inputRecords))
    {
        for (std::size_t index = 0; index < records.size(); ++index)
        {
            indices.emplace(records[index].id().value(), index);
        }
    }

    std::vector<ResourceDescriptor> records;
    std::map<QString, std::size_t> indices;
};

std::optional<ResourceId> ResourceId::fromString(const QString& value)
{
    if (value.isEmpty())
    {
        return std::nullopt;
    }
    for (const QChar character : value)
    {
        if (character.isNull() || !character.isPrint())
        {
            return std::nullopt;
        }
    }
    return ResourceId(value);
}

std::optional<ResourceId> ResourceId::fromString(std::string_view value)
{
    const QString decoded = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    if (decoded.toUtf8() != QByteArray(value.data(), static_cast<qsizetype>(value.size())))
    {
        return std::nullopt;
    }
    return fromString(decoded);
}

ResourceResult<ResourceCatalog> ResourceCatalog::load(
    const QString& catalogFilePath,
    const QString& resourceRoot
    )
{
    if (!QDir::isAbsolutePath(catalogFilePath) || !QDir::isAbsolutePath(resourceRoot))
    {
        return std::unexpected(ResourceError{
            ResourceErrorCode::InvalidArgument,
            QStringLiteral("catalog path and resource root must be absolute")
        });
    }

    const QFileInfo rootInfo(resourceRoot);
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || !rootInfo.isDir())
    {
        return std::unexpected(ResourceError{
            ResourceErrorCode::InvalidArgument,
            QStringLiteral("resource root must be an existing directory")
        });
    }

    const QFileInfo catalogInfo(catalogFilePath);
    if (catalogInfo.isSymLink() || !catalogInfo.isFile())
    {
        return std::unexpected(ResourceError{
            ResourceErrorCode::InvalidCatalog,
            QStringLiteral("catalog must be a regular, non-symlink file")
        });
    }
    const QString canonicalCatalog = catalogInfo.canonicalFilePath();
    if (canonicalCatalog.isEmpty() || catalogInfo.size() > MaximumCatalogBytes)
    {
        return std::unexpected(catalogError(QStringLiteral("catalog path is invalid or too large")));
    }

    QFile catalogFile(canonicalCatalog);
    if (!catalogFile.open(QIODevice::ReadOnly))
    {
        return std::unexpected(catalogError(
            QStringLiteral("cannot read resource catalog: ") + catalogFile.errorString()
            ));
    }
    const QByteArray bytes = catalogFile.readAll();
    if (catalogFile.error() != QFileDevice::NoError)
    {
        return std::unexpected(catalogError(
            QStringLiteral("cannot finish reading resource catalog: ") + catalogFile.errorString()
            ));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::unexpected(catalogError(
            QStringLiteral("resource catalog JSON is invalid: ") + parseError.errorString()
            ));
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("ClassMngrNext.ResourceCatalog")
        || root.value(QStringLiteral("schema_version")).toInt(-1) != 1
        || root.value(QStringLiteral("package_path_base")).toString()
            != QStringLiteral("ClassMngrNext::Resources")
        || root.value(QStringLiteral("path_format")).toString()
            != QStringLiteral("relative-posix")
        || !root.value(QStringLiteral("records")).isArray())
    {
        return std::unexpected(catalogError(QStringLiteral("unsupported resource catalog schema")));
    }

    std::vector<ResourceDescriptor> records;
    std::set<QString> ids;
    std::set<QString> exactPaths;
    std::set<QString> portablePaths;
    const QJsonArray recordsArray = root.value(QStringLiteral("records")).toArray();
    records.reserve(static_cast<std::size_t>(recordsArray.size()));
    for (const QJsonValue& value : recordsArray)
    {
        auto parsed = parseRecord(value);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        ResourceDescriptor descriptor = std::move(*parsed);
        const QString id = descriptor.id().value();
        const QString path = descriptor.packageRelativePath();
        const QString portablePath = path.normalized(QString::NormalizationForm_C).toCaseFolded();
        if (!ids.insert(id).second)
        {
            return std::unexpected(catalogError(QStringLiteral("duplicate resource logical ID")));
        }
        if (!exactPaths.insert(path).second || !portablePaths.insert(portablePath).second)
        {
            return std::unexpected(catalogError(
                QStringLiteral("duplicate or portable-colliding resource path")
                ));
        }
        records.push_back(std::move(descriptor));
    }

    auto data = std::make_shared<const Data>(std::move(records));
    return ResourceCatalog(canonicalRoot, std::move(data));
}

const ResourceDescriptor* ResourceCatalog::find(const ResourceId& id) const noexcept
{
    const auto found = m_data->indices.find(id.value());
    if (found == m_data->indices.end())
    {
        return nullptr;
    }
    return &m_data->records[found->second];
}

const std::vector<ResourceDescriptor>& ResourceCatalog::records() const noexcept
{
    return m_data->records;
}

}
