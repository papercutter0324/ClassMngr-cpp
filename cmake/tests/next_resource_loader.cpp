#include "next/resources/resource_loader.h"
#include "next/bootstrap/qt_resource_diagnostics_sink.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <optional>
#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#define NOMINMAX
#include <windows.h>
#endif

using namespace ClassMngr::Next::Resources;

namespace
{

namespace Bootstrap = ClassMngr::Next::Bootstrap;

bool fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return false;
}

enum class SymlinkFixtureResult
{
    Created,
    Unsupported,
    Failed
};

SymlinkFixtureResult createFileSymlink(
    const QString& targetPath,
    const QString& linkPath
    )
{
#if defined(Q_OS_WIN)
    const std::wstring target = targetPath.toStdWString();
    const std::wstring link = linkPath.toStdWString();
    if (CreateSymbolicLinkW(
            link.c_str(),
            target.c_str(),
            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
            ))
    {
        return SymlinkFixtureResult::Created;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_PRIVILEGE_NOT_HELD)
    {
        return SymlinkFixtureResult::Unsupported;
    }
    fail(QStringLiteral("could not create genuine file symlink fixture; "
                        "CreateSymbolicLinkW error ")
         + QString::number(error));
    return SymlinkFixtureResult::Failed;
#elif defined(Q_OS_UNIX)
    const QByteArray target = QFile::encodeName(targetPath);
    const QByteArray link = QFile::encodeName(linkPath);
    if (::symlink(target.constData(), link.constData()) == 0)
    {
        return SymlinkFixtureResult::Created;
    }
    fail(QStringLiteral("could not create genuine file symlink fixture; symlink error ")
         + QString::fromLocal8Bit(std::strerror(errno)));
    return SymlinkFixtureResult::Failed;
#else
    Q_UNUSED(targetPath);
    Q_UNUSED(linkPath);
    return SymlinkFixtureResult::Unsupported;
#endif
}

std::optional<ResourceId> resourceId(const char* value)
{
    return ResourceId::fromString(QString::fromLatin1(value));
}

std::optional<ResourceRequest> resourceRequest(
    const ResourceId& id,
    ResourceScopeKind kind,
    const QString& ownerLabel
    )
{
    auto request = ResourceRequest::create(id, kind, ownerLabel);
    if (!request)
    {
        fail(QStringLiteral("resource request was rejected: ") + request.error().message);
        return std::nullopt;
    }
    return std::move(*request);
}

bool scopeMatches(
    const ResourceScope& scope,
    ResourceScopeKind kind,
    const QString& ownerLabel
    )
{
    return scope.kind() == kind && scope.ownerLabel() == ownerLabel;
}

bool testResourceDiagnosticSerialization()
{
    const auto id = resourceId("resource:diagnostic-probe.png");
    const auto requestScope = ResourceScope::create(
        ResourceScopeKind::Page,
        QStringLiteral("classes")
        );
    const auto cacheScope = ResourceScope::create(
        ResourceScopeKind::Startup,
        QStringLiteral("shared-image-cache")
        );
    if (!id || !requestScope || !cacheScope)
    {
        return fail(QStringLiteral("resource diagnostic serializer fixture was invalid"));
    }

    ResourceDiagnostic diagnostic{*id, *requestScope};
    diagnostic.catalogOwner = QStringLiteral("ClassMngrNext bootstrap");
    diagnostic.catalogCategory = QStringLiteral("visual");
    diagnostic.resourceKind = QStringLiteral("icon");
    diagnostic.expectedType = QStringLiteral("png");
    diagnostic.location = QStringLiteral("package_file");
    diagnostic.elapsed = std::chrono::nanoseconds(123456);
    diagnostic.sourceBytes = 4096;
    diagnostic.decodedBytes = 2400;
    diagnostic.sourceImageDimensions = QSize(120, 80);
    diagnostic.decodedImageDimensions = QSize(30, 20);
    diagnostic.fontFamilyNames = QStringList{QStringLiteral("Caveat")};
    diagnostic.succeeded = false;
    diagnostic.errorCode = ResourceErrorCode::DecodeError;

    const QByteArray first = Bootstrap::qtResourceDiagnosticRecord(diagnostic);
    const QByteArray second = Bootstrap::qtResourceDiagnosticRecord(diagnostic);
    if (first != second)
    {
        return fail(QStringLiteral("resource diagnostic serialization was not deterministic"));
    }

    const QJsonObject serialized = QJsonDocument::fromJson(first).object();
    const QJsonObject request = serialized.value(
        QStringLiteral("request_scope")).toObject();
    const QJsonObject catalog = serialized.value(
        QStringLiteral("catalog_resource")).toObject();
    const QJsonObject payload = serialized.value(
        QStringLiteral("payload_accounting")).toObject();
    const QJsonObject image = serialized.value(QStringLiteral("image")).toObject();
    const QJsonObject font = serialized.value(QStringLiteral("font")).toObject();
    const QJsonObject cache = serialized.value(QStringLiteral("cache")).toObject();
    if (serialized.value(QStringLiteral("resource_id")).toString() != id->value()
        || request.value(QStringLiteral("kind")).toString() != QStringLiteral("page")
        || request.value(QStringLiteral("owner")).toString() != QStringLiteral("classes")
        || catalog.value(QStringLiteral("owner")).toString()
            != QStringLiteral("ClassMngrNext bootstrap")
        || serialized.value(QStringLiteral("outcome")).toString()
            != QStringLiteral("failed")
        || serialized.value(QStringLiteral("error_code")).toString()
            != QStringLiteral("decode_error")
        || payload.value(QStringLiteral("source_catalog_bytes")).toInt() != 4096
        || payload.value(QStringLiteral("decoded_payload_bytes")).toInt() != 2400
        || image.value(QStringLiteral("source_dimensions")).toObject()
            .value(QStringLiteral("width_pixels")).toInt() != 120
        || image.value(QStringLiteral("decoded_dimensions")).toObject()
            .value(QStringLiteral("height_pixels")).toInt() != 20
        || font.value(QStringLiteral("family_names")).toArray().size() != 1
        || cache.value(QStringLiteral("configured")).toBool()
        || !cache.value(QStringLiteral("hit")).isNull()
        || !cache.value(QStringLiteral("entry_count")).isNull()
        || !cache.value(QStringLiteral("accounted_payload_bytes")).isNull()
        || !cache.value(QStringLiteral("capacity_payload_bytes")).isNull())
    {
        return fail(QStringLiteral("resource diagnostic serialized fields were incorrect"));
    }

    diagnostic.succeeded = true;
    diagnostic.errorCode.reset();
    diagnostic.cacheConfigured = true;
    diagnostic.cacheEntryCount = 0;
    diagnostic.cacheAccountedBytes = 0;
    diagnostic.cacheCapacityBytes = 8192;
    const QJsonObject configuredEmptyCache = QJsonDocument::fromJson(
        Bootstrap::qtResourceDiagnosticRecord(diagnostic)).object()
        .value(QStringLiteral("cache")).toObject();
    if (!configuredEmptyCache.value(QStringLiteral("configured")).toBool()
        || configuredEmptyCache.value(QStringLiteral("hit")).toBool()
        || configuredEmptyCache.value(QStringLiteral("entry_count")).toInt() != 0
        || configuredEmptyCache.value(QStringLiteral(
            "accounted_payload_bytes")).toInt() != 0
        || configuredEmptyCache.value(QStringLiteral(
            "capacity_payload_bytes")).toInt() != 8192)
    {
        return fail(QStringLiteral(
            "configured empty cache was confused with an unavailable cache"));
    }

    diagnostic.cacheHit = true;
    diagnostic.cacheEntryCount = 1;
    diagnostic.cacheEntryAccountedBytes = 2400;
    diagnostic.cacheAccountedBytes = 2400;
    diagnostic.cacheEntry = ResourceCache::EntryInfo{
        ResourceCacheValueKind::Image,
        2400,
        *cacheScope,
        30
    };
    const QJsonObject configuredCache = QJsonDocument::fromJson(
        Bootstrap::qtResourceDiagnosticRecord(diagnostic)).object()
        .value(QStringLiteral("cache")).toObject();
    const QJsonObject cacheEntry = configuredCache.value(
        QStringLiteral("entry")).toObject();
    if (!configuredCache.value(QStringLiteral("hit")).toBool()
        || configuredCache.value(QStringLiteral("entry_count")).toInt() != 1
        || cacheEntry.value(QStringLiteral("kind")).toString()
            != QStringLiteral("image")
        || cacheEntry.value(QStringLiteral("owner_scope")).toObject()
            .value(QStringLiteral("owner")).toString()
            != QStringLiteral("shared-image-cache")
        || cacheEntry.value(QStringLiteral(
            "image_maximum_dimension_pixels")).toInt() != 30)
    {
        return fail(QStringLiteral("resource diagnostic lost cache-entry ownership"));
    }
    return true;
}

bool diagnosticMatches(
    const ResourceDiagnostic& diagnostic,
    const ResourceId& id,
    ResourceScopeKind kind,
    const QString& ownerLabel
    )
{
    return diagnostic.resourceId == id
        && scopeMatches(diagnostic.scope, kind, ownerLabel);
}

bool stageFile(
    const QString& sourceRoot,
    const QString& packageRoot,
    const QString& relativePath,
    const QString& alternateSource = {}
    )
{
    const QString sourcePath = alternateSource.isEmpty()
        ? QDir(sourceRoot).filePath(relativePath)
        : QDir(alternateSource).filePath(QFileInfo(relativePath).fileName());
    const QString destination = QDir(packageRoot).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    {
        return fail(QStringLiteral("cannot create fixture directory for ") + relativePath);
    }
    QFile::remove(destination);
    if (!QFile::copy(sourcePath, destination))
    {
        return fail(QStringLiteral("cannot stage fixture ") + sourcePath);
    }
    return true;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

bool testResourceCacheCore()
{
    bool rejectedZeroCapacity = false;
    try
    {
        ResourceCache invalid(0);
    }
    catch (const std::invalid_argument&)
    {
        rejectedZeroCapacity = true;
    }
    if (!rejectedZeroCapacity)
    {
        return fail(QStringLiteral("resource cache accepted a zero byte capacity"));
    }

    const auto firstId = resourceId("resource:cache-first");
    const auto secondId = resourceId("resource:cache-second");
    const auto thirdId = resourceId("resource:cache-third");
    const auto largeImageId = resourceId("resource:cache-large-image");
    const auto largeJsonId = resourceId("resource:cache-large-json");
    const auto documentBodyId = resourceId("document.body:cache-document.pdf");
    if (!firstId || !secondId || !thirdId || !largeImageId || !largeJsonId
        || !documentBodyId)
    {
        return fail(QStringLiteral("resource cache test IDs were rejected"));
    }
    auto scope = ResourceScope::create(
        ResourceScopeKind::Feature,
        QStringLiteral("CacheFixture")
        );
    if (!scope)
    {
        return fail(QStringLiteral("resource cache test scope was rejected"));
    }

    ResourceCache cache(16);
    if (cache.capacityBytes() != 16 || cache.accountedBytes() != 0 || cache.entryCount() != 0)
    {
        return fail(QStringLiteral("resource cache did not retain its explicit byte capacity"));
    }
    cache.insertText(*firstId, QStringLiteral("AAAA"), *scope);
    cache.insertText(*secondId, QStringLiteral("BBBB"), *scope);
    if (cache.accountedBytes() != 16 || cache.entryCount() != 2)
    {
        return fail(QStringLiteral("resource cache byte accounting was incorrect"));
    }
    const auto firstSnapshot = cache.snapshot(*firstId);
    if (firstSnapshot.capacityBytes != 16 || firstSnapshot.accountedBytes != 16
        || !firstSnapshot.entry || firstSnapshot.entry->kind != ResourceCacheValueKind::Text
        || firstSnapshot.entry->accountedBytes != 8)
    {
        return fail(QStringLiteral("resource cache snapshot was not a coherent value view"));
    }

    const auto firstHit = cache.findText(*firstId);
    if (!firstHit || *firstHit != QStringLiteral("AAAA"))
    {
        return fail(QStringLiteral("resource cache could not return a text hit"));
    }
    cache.insertText(*thirdId, QStringLiteral("CCCC"), *scope);
    if (cache.entryInfo(*secondId).has_value()
        || !cache.entryInfo(*firstId).has_value()
        || !cache.entryInfo(*thirdId).has_value()
        || cache.accountedBytes() != 16)
    {
        return fail(QStringLiteral("resource cache did not update recency before LRU eviction"));
    }

    const auto oversizedTextId = resourceId("resource:cache-oversized-text");
    if (!oversizedTextId)
    {
        return fail(QStringLiteral("oversized resource cache test ID was rejected"));
    }
    cache.insertText(*oversizedTextId, QString(9, QChar(u'x')), *scope);
    cache.insertText(*documentBodyId, QStringLiteral("document"), *scope);
    const QJsonDocument oversizedJson = QJsonDocument::fromJson(
        QByteArrayLiteral("{\"message\":\"larger than the cache budget\"}")
        );
    if (oversizedJson.isNull())
    {
        return fail(QStringLiteral("oversized JSON cache fixture could not be parsed"));
    }
    cache.insertJson(*largeJsonId, oversizedJson, *scope);
    QImage oversizedImage(4, 4, QImage::Format_RGBA8888);
    cache.insertImage(*largeImageId, oversizedImage, *scope);
    if (cache.entryInfo(*oversizedTextId).has_value()
        || cache.entryInfo(*largeJsonId).has_value()
        || cache.entryInfo(*documentBodyId).has_value()
        || cache.entryInfo(*largeImageId).has_value()
        || cache.entryCount() != 2
        || cache.accountedBytes() != 16)
    {
        return fail(QStringLiteral("oversized cache values were retained or evicted resident values"));
    }

    ResourceCache isolatedCache(16);
    if (isolatedCache.findText(*firstId).has_value())
    {
        return fail(QStringLiteral("resource cache entries leaked between instances"));
    }
    isolatedCache.insertText(*firstId, QStringLiteral("other"), *scope);
    const auto isolatedValue = isolatedCache.findText(*firstId);
    const auto originalValue = cache.findText(*firstId);
    const auto metadata = cache.entryInfo(*firstId);
    if (!isolatedValue || *isolatedValue != QStringLiteral("other")
        || !originalValue || *originalValue != QStringLiteral("AAAA")
        || !metadata || metadata->kind != ResourceCacheValueKind::Text
        || !scopeMatches(metadata->scope, ResourceScopeKind::Feature, QStringLiteral("CacheFixture")))
    {
        return fail(QStringLiteral("resource cache instances or retained owner metadata were incorrect"));
    }
    return true;
}

bool testBoundedImageCacheIdentity()
{
    QTemporaryDir packageRoot;
    if (!packageRoot.isValid())
    {
        return fail(QStringLiteral("could not create bounded-image cache package fixture"));
    }

    constexpr int sourceWidth = 800;
    constexpr int sourceHeight = 400;
    constexpr int maximumDimension = 100;
    const QString relativeImagePath = QStringLiteral("assets/test/cache-bound.png");
    const QString imagePath = QDir(packageRoot.path()).filePath(relativeImagePath);
    if (!QDir().mkpath(QFileInfo(imagePath).absolutePath()))
    {
        return fail(QStringLiteral("could not create synthetic image package directory"));
    }

    QImage sourceImage(sourceWidth, sourceHeight, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(31, 92, 173, 255));
    QByteArray imageBytes;
    QBuffer imageBuffer(&imageBytes);
    if (!imageBuffer.open(QIODevice::WriteOnly) || !sourceImage.save(&imageBuffer, "PNG")
        || imageBytes.isEmpty() || !writeBytes(imagePath, imageBytes))
    {
        return fail(QStringLiteral("could not encode the synthetic bounded-cache image"));
    }

    const QByteArray imageDigest = QCryptographicHash::hash(
        imageBytes,
        QCryptographicHash::Sha256
        ).toHex();
    const QString digestText = QString::fromLatin1(imageDigest);
    const QJsonObject sourceMetadata{
        {QStringLiteral("bytes"), imageBytes.size()},
        {QStringLiteral("path"), QStringLiteral("synthetic/cache-bound.png")},
        {QStringLiteral("sha256"), digestText}
    };
    const QJsonObject bakedMetadata{
        {QStringLiteral("bytes"), imageBytes.size()},
        {QStringLiteral("sha256"), digestText}
    };
    const QJsonObject record{
        {QStringLiteral("category"), QStringLiteral("test")},
        {QStringLiteral("expected_type"), QStringLiteral("png")},
        {QStringLiteral("loading"), QJsonObject{
            {QStringLiteral("tier"), QStringLiteral("feature_on_demand")},
            {QStringLiteral("policy"), QStringLiteral("decode synthetic bounded-cache image")},
            {QStringLiteral("lifetime"), QStringLiteral("resource-loader test")},
            {QStringLiteral("release_policy"), QStringLiteral("release with test cache")}
        }},
        {QStringLiteral("location"), QStringLiteral("resource_file")},
        {QStringLiteral("logical_id"), QStringLiteral("resource:assets/test/cache-bound.png")},
        {QStringLiteral("owner"), QStringLiteral("ResourceLoader cache identity test")},
        {QStringLiteral("package_relative_path"), relativeImagePath},
        {QStringLiteral("provenance"), QJsonObject{
            {QStringLiteral("baked"), bakedMetadata},
            {QStringLiteral("source"), sourceMetadata}
        }},
        {QStringLiteral("resource_kind"), QStringLiteral("test_image")},
        {QStringLiteral("startup_necessity"), QStringLiteral("not_required_until_test_use")},
        {QStringLiteral("streamability"), QJsonObject{
            {QStringLiteral("method_ref"), QStringLiteral("test_fixture")},
            {QStringLiteral("status"), QStringLiteral("no")}
        }}
    };
    const QJsonObject catalogDocument{
        {QStringLiteral("format"), QStringLiteral("ClassMngrNext.ResourceCatalog")},
        {QStringLiteral("package_path_base"), QStringLiteral("ClassMngrNext::Resources")},
        {QStringLiteral("path_format"), QStringLiteral("relative-posix")},
        {QStringLiteral("records"), QJsonArray{record}},
        {QStringLiteral("schema_version"), 1}
    };
    const QString catalogPath = QDir(packageRoot.path()).filePath(
        QStringLiteral("resource-catalog.json")
        );
    if (!writeBytes(
            catalogPath,
            QJsonDocument(catalogDocument).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write synthetic bounded-cache catalog"));
    }

    const auto id = resourceId("resource:assets/test/cache-bound.png");
    if (!id)
    {
        return fail(QStringLiteral("synthetic bounded-cache resource ID was rejected"));
    }
    auto request = resourceRequest(
        *id,
        ResourceScopeKind::Page,
        QStringLiteral("BoundedImageCacheIdentity")
        );
    if (!request)
    {
        return false;
    }

    auto boundedFirstCatalog = ResourceCatalog::load(catalogPath, packageRoot.path());
    if (!boundedFirstCatalog)
    {
        return fail(QStringLiteral("bounded-first synthetic catalog failed to load: ")
            + boundedFirstCatalog.error().message);
    }
    ResourceCache boundedFirstCache(8 * 1024 * 1024);
    std::vector<ResourceDiagnostic> boundedFirstDiagnostics;
    ResourceLoader boundedFirstLoader(
        std::move(*boundedFirstCatalog),
        boundedFirstCache,
        [&boundedFirstDiagnostics](const ResourceDiagnostic& diagnostic) {
            boundedFirstDiagnostics.push_back(diagnostic);
        }
        );
    auto boundedImage = boundedFirstLoader.loadBoundedImage(*request, maximumDimension);
    if (!boundedImage
        || boundedImage->get().size() != QSize(maximumDimension, maximumDimension / 2))
    {
        return fail(QStringLiteral("bounded-first request did not decode to 100x50"));
    }
    auto unboundedAfterBounded = boundedFirstLoader.loadImage(*request);
    const auto unboundedAfterBoundedInfo = boundedFirstCache.entryInfo(*id);
    if (!unboundedAfterBounded
        || unboundedAfterBounded->get().size() != QSize(sourceWidth, sourceHeight)
        || boundedFirstDiagnostics.size() != 2
        || boundedFirstDiagnostics.back().cacheHit
        || !unboundedAfterBoundedInfo
        || unboundedAfterBoundedInfo->imageMaximumDimension != 0)
    {
        return fail(QStringLiteral(
            "unbounded request reused the smaller bounded image cache entry"
            ));
    }

    auto unboundedFirstCatalog = ResourceCatalog::load(catalogPath, packageRoot.path());
    if (!unboundedFirstCatalog)
    {
        return fail(QStringLiteral("unbounded-first synthetic catalog failed to load: ")
            + unboundedFirstCatalog.error().message);
    }
    ResourceCache unboundedFirstCache(8 * 1024 * 1024);
    std::vector<ResourceDiagnostic> unboundedFirstDiagnostics;
    ResourceLoader unboundedFirstLoader(
        std::move(*unboundedFirstCatalog),
        unboundedFirstCache,
        [&unboundedFirstDiagnostics](const ResourceDiagnostic& diagnostic) {
            unboundedFirstDiagnostics.push_back(diagnostic);
        }
        );
    auto unboundedImage = unboundedFirstLoader.loadImage(*request);
    if (!unboundedImage || unboundedImage->get().size() != QSize(sourceWidth, sourceHeight))
    {
        return fail(QStringLiteral("unbounded-first request did not preserve 800x400"));
    }
    auto boundedAfterUnbounded = unboundedFirstLoader.loadBoundedImage(
        *request,
        maximumDimension
        );
    const auto boundedAfterUnboundedInfo = unboundedFirstCache.entryInfo(*id);
    if (!boundedAfterUnbounded
        || boundedAfterUnbounded->get().size() != QSize(maximumDimension, maximumDimension / 2)
        || unboundedFirstDiagnostics.size() != 2
        || unboundedFirstDiagnostics.back().cacheHit
        || !boundedAfterUnboundedInfo
        || boundedAfterUnboundedInfo->imageMaximumDimension != maximumDimension)
    {
        return fail(QStringLiteral(
            "bounded request reused the differently bounded/full-resolution image cache entry"
            ));
    }

    return true;
}

std::optional<QByteArray> sameSizeValidJsonMutation(const QByteArray& original)
{
    QByteArray changed = original;
    bool inString = false;
    bool escaped = false;
    for (qsizetype index = 0; index < changed.size(); ++index)
    {
        const unsigned char byte = static_cast<unsigned char>(changed.at(index));
        if (!inString)
        {
            if (byte == '"')
            {
                inString = true;
            }
            continue;
        }
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (byte == '\\')
        {
            escaped = true;
            continue;
        }
        if (byte == '"')
        {
            inString = false;
            continue;
        }
        if (byte >= 'a' && byte <= 'z')
        {
            changed[index] = static_cast<char>(byte - 'a' + 'A');
            return changed;
        }
        if (byte >= 'A' && byte <= 'Z')
        {
            changed[index] = static_cast<char>(byte - 'A' + 'a');
            return changed;
        }
    }
    return std::nullopt;
}

bool testVerifiedInterFileBackedFontRegistration()
{
    struct ExpectedFace final
    {
        const char* fileName;
        qint64 bytes;
        const char* sha256;
        const char* style;
        int weight;
        bool italic;
    };
    constexpr std::array<ExpectedFace, 6> expectedFaces{{
        {"Inter-Regular.ttf", 411640, "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82", "Regular", 400, false},
        {"Inter-Medium.ttf", 417300, "97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872", "Medium", 500, false},
        {"Inter-SemiBold.ttf", 419744, "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3", "SemiBold", 600, false},
        {"Inter-Bold.ttf", 420428, "288316099b1e0a47a4716d159098005eef7c0066921f34e3200393dbdb01947f", "Bold", 700, false},
        {"Inter-BoldItalic.ttf", 425296, "948405a16cdc62701da5f4005ed068ca5f4d27061d98f7974ccfc37831d9581d", "Bold Italic", 700, true},
        {"Inter-Black.ttf", 419968, "6342d3ea6dc088b43867f615e807d898adf100c93edb978b8e52c5eb71a264da", "Black", 900, false},
    }};
    QTemporaryDir packageRoot;
    if (!packageRoot.isValid())
    {
        return fail(QStringLiteral("could not create Inter font package fixture root"));
    }

    auto catalog = ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        packageRoot.path()
        );
    if (!catalog)
    {
        return fail(QStringLiteral("Inter font catalog load failed: ")
            + catalog.error().message);
    }
    const QString bakedRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);

    std::vector<ResourceDiagnostic> diagnostics;
    ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );

    std::vector<ResourceRequest> requests;
    std::vector<QString> stagedPaths;
    requests.reserve(expectedFaces.size());
    stagedPaths.reserve(expectedFaces.size());
    for (const ExpectedFace& face : expectedFaces)
    {
        const QString relativePath = QStringLiteral("assets/fonts/")
            + QString::fromUtf8(face.fileName);
        if (!stageFile(bakedRoot, packageRoot.path(), relativePath))
        {
            return false;
        }
        const QByteArray logicalIdBytes = (QStringLiteral("resource:") + relativePath).toUtf8();
        const auto id = resourceId(logicalIdBytes.constData());
        const ResourceDescriptor* descriptor = id ? loader.catalog().find(*id) : nullptr;
        const QString stagedPath = QFileInfo(
            QDir(packageRoot.path()).filePath(relativePath)
            ).canonicalFilePath();
        if (!id || descriptor == nullptr || stagedPath.isEmpty()
            || descriptor->location() != ResourceLocation::PackageFile
            || descriptor->expectedType() != ResourceType::Ttf
            || descriptor->expectedBytes() != std::optional<qint64>(face.bytes)
            || !descriptor->expectedSha256().has_value()
            || descriptor->expectedSha256()->toHex()
                != QByteArray(face.sha256))
        {
            return fail(QStringLiteral("Inter static face lacks its exact verified catalog identity: ")
                + relativePath);
        }
        QFile stagedFile(stagedPath);
        if (!stagedFile.open(QIODevice::ReadOnly))
        {
            return fail(QStringLiteral("could not verify staged Inter face: ") + relativePath);
        }
        const QByteArray stagedBytes = stagedFile.readAll();
        if (stagedBytes.size() != face.bytes
            || QCryptographicHash::hash(stagedBytes, QCryptographicHash::Sha256).toHex()
                != QByteArray(face.sha256))
        {
            return fail(QStringLiteral("staged Inter face differs from its verified identity: ")
                + relativePath);
        }
        const auto request = resourceRequest(
            *id,
            ResourceScopeKind::Feature,
            QStringLiteral("InterStaticFileBackedRegistration")
            );
        if (!request)
        {
            return fail(QStringLiteral("could not create a request for Inter face: ") + relativePath);
        }
        requests.push_back(*request);
        stagedPaths.push_back(stagedPath);
    }

    int dataRegistrationId = -1;
    QStringList dataBackedFamilies;
    {
        auto dataBacked = loader.loadFont(requests.front());
        if (!dataBacked || !dataBacked->get().sourceFilePath().isEmpty())
        {
            return fail(QStringLiteral("data-backed Inter registration behavior changed"));
        }
        dataRegistrationId = dataBacked->get().registrationId();
        dataBackedFamilies = dataBacked->get().familyNames();
    }
    if (dataRegistrationId < 0 || dataBackedFamilies.isEmpty()
        || !QFontDatabase::applicationFontFamilies(dataRegistrationId).isEmpty())
    {
        return fail(QStringLiteral("data-backed Inter registration was not released cleanly"));
    }
    diagnostics.clear();

    std::array<int, expectedFaces.size()> registrationIds{};
    {
        std::vector<ScopedResource<RegisteredFont>> registrations;
        registrations.reserve(expectedFaces.size());
        for (std::size_t index = 0; index < expectedFaces.size(); ++index)
        {
            auto loaded = loader.loadFileBackedFont(requests.at(index));
            if (!loaded)
            {
                return fail(QStringLiteral("verified file-backed Inter registration failed: ")
                    + loaded.error().message);
            }
            auto registration = std::move(*loaded);
            const RegisteredFont& font = registration.get();
            registrationIds.at(index) = font.registrationId();
            const QStringList returnedFamilies = font.familyNames();
            if (registrationIds.at(index) < 0
                || font.sourceFilePath() != stagedPaths.at(index)
                || returnedFamilies != QFontDatabase::applicationFontFamilies(
                    registrationIds.at(index))
                || returnedFamilies != dataBackedFamilies
                || returnedFamilies.isEmpty()
                || returnedFamilies.first() != QStringLiteral("Inter")
                || !returnedFamilies.contains(QStringLiteral("Inter")))
            {
                return fail(QStringLiteral(
                    "file-backed Inter face did not retain its canonical path or family identity: ")
                    + QString::fromUtf8(expectedFaces.at(index).fileName));
            }
            if (diagnostics.size() != index + 1
                || !diagnosticMatches(
                    diagnostics.back(),
                    requests.at(index).id(),
                    ResourceScopeKind::Feature,
                    QStringLiteral("InterStaticFileBackedRegistration")
                    )
                || !diagnostics.back().succeeded
                || diagnostics.back().errorCode.has_value()
                || !diagnostics.back().fontFamilyNames.has_value()
                || *diagnostics.back().fontFamilyNames != returnedFamilies
                || diagnostics.back().decodedBytes.has_value())
            {
                return fail(QStringLiteral(
                    "file-backed Inter face changed ResourceLoader diagnostic semantics: ")
                    + QString::fromUtf8(expectedFaces.at(index).fileName));
            }
            registrations.emplace_back(std::move(registration));
        }

        const QStringList styles = QFontDatabase::styles(QStringLiteral("Inter"));
        for (const ExpectedFace& face : expectedFaces)
        {
            const QString style = QString::fromUtf8(face.style);
            const QFont resolved = QFontDatabase::font(
                QStringLiteral("Inter"), style, 12
                );
            if (!styles.contains(style)
                || static_cast<int>(QFontDatabase::weight(QStringLiteral("Inter"), style))
                    != face.weight
                || QFontDatabase::italic(QStringLiteral("Inter"), style) != face.italic
                || static_cast<int>(resolved.weight()) != face.weight
                || resolved.italic() != face.italic)
            {
                return fail(QStringLiteral("Inter face style or weight changed: ") + style);
            }
        }
        if (diagnostics.size() != expectedFaces.size())
        {
            return fail(QStringLiteral("file-backed Inter face registrations were not all diagnosed"));
        }
    }

    for (const int registrationId : registrationIds)
    {
        if (registrationId < 0
            || !QFontDatabase::applicationFontFamilies(registrationId).isEmpty())
        {
            return fail(QStringLiteral("an Inter static face registration was not removed with its owner"));
        }
    }

    QFile stagedFont(stagedPaths.front());
    if (!stagedFont.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("could not read Inter Regular for catalog-integrity test"));
    }
    QByteArray modifiedFont = stagedFont.readAll();
    stagedFont.close();
    if (modifiedFont.isEmpty())
    {
        return fail(QStringLiteral("staged Inter font was empty"));
    }
    const qsizetype changedIndex = modifiedFont.size() / 2;
    modifiedFont[changedIndex] = static_cast<char>(modifiedFont.at(changedIndex) ^ 0x01);
    if (!writeBytes(stagedPaths.front(), modifiedFont))
    {
        return fail(QStringLiteral("could not corrupt staged Inter Regular for integrity test"));
    }
    diagnostics.clear();
    const auto corrupted = loader.loadFileBackedFont(requests.front());
    if (corrupted
        || corrupted.error().code != ResourceErrorCode::CorruptResource
        || diagnostics.size() != 1
        || diagnostics.back().succeeded
        || diagnostics.back().errorCode != ResourceErrorCode::CorruptResource
        || !QFontDatabase::applicationFontFamilies(registrationIds.front()).isEmpty())
    {
        return fail(QStringLiteral(
            "file-backed Inter registration accepted a package file that failed catalog verification"
            ));
    }
    return true;
}

bool testFileBackedFontRejectsCatalogSizeOverLimit()
{
    constexpr qint64 maximumFontBytes = 128LL * 1024 * 1024;
    const qint64 overLimitBytes = maximumFontBytes + 1;
    QTemporaryDir packageRoot;
    if (!packageRoot.isValid())
    {
        return fail(QStringLiteral("could not create oversized-font catalog fixture root"));
    }

    const QString relativePath = QStringLiteral("assets/test/oversized-font.ttc");
    const QString digest = QString(64, QLatin1Char('0'));
    const QJsonObject record{
        {QStringLiteral("category"), QStringLiteral("test")},
        {QStringLiteral("expected_type"), QStringLiteral("ttc")},
        {QStringLiteral("loading"), QJsonObject{
            {QStringLiteral("tier"), QStringLiteral("feature_on_demand")},
            {QStringLiteral("policy"), QStringLiteral("reject oversized test font")},
            {QStringLiteral("lifetime"), QStringLiteral("resource-loader test")},
            {QStringLiteral("release_policy"), QStringLiteral("release with test scope")}
        }},
        {QStringLiteral("location"), QStringLiteral("resource_file")},
        {QStringLiteral("logical_id"), QStringLiteral("resource:assets/test/oversized-font.ttc")},
        {QStringLiteral("owner"), QStringLiteral("ResourceLoader font size bound test")},
        {QStringLiteral("package_relative_path"), relativePath},
        {QStringLiteral("provenance"), QJsonObject{
            {QStringLiteral("baked"), QJsonObject{
                {QStringLiteral("bytes"), static_cast<double>(overLimitBytes)},
                {QStringLiteral("sha256"), digest}
            }},
            {QStringLiteral("source"), QJsonObject{
                {QStringLiteral("bytes"), static_cast<double>(overLimitBytes)},
                {QStringLiteral("path"), QStringLiteral("synthetic/oversized-font.ttc")},
                {QStringLiteral("sha256"), digest}
            }}
        }},
        {QStringLiteral("resource_kind"), QStringLiteral("test_font")},
        {QStringLiteral("startup_necessity"), QStringLiteral("not_required_until_test_use")},
        {QStringLiteral("streamability"), QJsonObject{
            {QStringLiteral("method_ref"), QStringLiteral("test_fixture")},
            {QStringLiteral("status"), QStringLiteral("no")}
        }}
    };
    const QJsonObject catalogDocument{
        {QStringLiteral("format"), QStringLiteral("ClassMngrNext.ResourceCatalog")},
        {QStringLiteral("package_path_base"), QStringLiteral("ClassMngrNext::Resources")},
        {QStringLiteral("path_format"), QStringLiteral("relative-posix")},
        {QStringLiteral("records"), QJsonArray{record}},
        {QStringLiteral("schema_version"), 1}
    };
    const QString catalogPath = QDir(packageRoot.path()).filePath(
        QStringLiteral("resource-catalog.json")
        );
    if (!writeBytes(
            catalogPath,
            QJsonDocument(catalogDocument).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write oversized-font catalog fixture"));
    }

    const auto id = resourceId("resource:assets/test/oversized-font.ttc");
    const auto request = id
        ? resourceRequest(
            *id,
            ResourceScopeKind::Feature,
            QStringLiteral("OversizedFileBackedFont")
            )
        : std::nullopt;
    auto catalog = ResourceCatalog::load(catalogPath, packageRoot.path());
    if (!id || !request || !catalog)
    {
        return fail(QStringLiteral("oversized-font catalog fixture was invalid")
            + (catalog ? QString() : QStringLiteral(": ") + catalog.error().message));
    }

    std::vector<ResourceDiagnostic> diagnostics;
    ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    const auto loaded = loader.loadFileBackedFont(*request);
    if (loaded
        || loaded.error().code != ResourceErrorCode::UnsupportedOperation
        || diagnostics.size() != 1
        || !diagnosticMatches(
            diagnostics.back(),
            *id,
            ResourceScopeKind::Feature,
            QStringLiteral("OversizedFileBackedFont")
            )
        || diagnostics.back().succeeded
        || diagnostics.back().errorCode != ResourceErrorCode::UnsupportedOperation)
    {
        return fail(QStringLiteral(
            "file-backed font catalog size above 128 MiB was not rejected before path access"
            ));
    }
    return true;
}

bool testLoader(QCoreApplication& application)
{
    QTemporaryDir temporaryRoot;
    if (!temporaryRoot.isValid())
    {
        return fail(QStringLiteral("could not create resource package fixture root"));
    }

    auto loadedCatalog = ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        temporaryRoot.path()
        );
    if (!loadedCatalog)
    {
        return fail(QStringLiteral("catalog load failed: ") + loadedCatalog.error().message);
    }

    const auto absentCatalogId = resourceId(
        "resource:assets/not-in-catalog/resource-loader-probe.json"
        );
    auto absentCatalogRequest = absentCatalogId
        ? resourceRequest(
            *absentCatalogId,
            ResourceScopeKind::Operation,
            QStringLiteral("ResourceLoaderMissingCatalogId")
            )
        : std::nullopt;
    if (!absentCatalogId || loadedCatalog->find(*absentCatalogId) != nullptr
        || !absentCatalogRequest)
    {
        return fail(QStringLiteral("missing-catalog resource fixture was invalid"));
    }
    std::vector<ResourceDiagnostic> absentCatalogDiagnostics;
    ResourceLoader absentCatalogLoader(
        *loadedCatalog,
        [&absentCatalogDiagnostics](const ResourceDiagnostic& diagnostic) {
            absentCatalogDiagnostics.push_back(diagnostic);
        }
        );
    const auto absentCatalogResult = absentCatalogLoader.loadText(*absentCatalogRequest);
    if (absentCatalogResult
        || absentCatalogResult.error().code != ResourceErrorCode::NotFound
        || absentCatalogDiagnostics.size() != 1
        || absentCatalogDiagnostics.front().resourceId != *absentCatalogId
        || !scopeMatches(
            absentCatalogDiagnostics.front().scope,
            ResourceScopeKind::Operation,
            QStringLiteral("ResourceLoaderMissingCatalogId")
            )
        || absentCatalogDiagnostics.front().succeeded
        || absentCatalogDiagnostics.front().errorCode != ResourceErrorCode::NotFound
        || absentCatalogDiagnostics.front().sourceBytes.has_value()
        || absentCatalogDiagnostics.front().cacheConfigured
        || absentCatalogDiagnostics.front().cacheEntryCount.has_value()
        || absentCatalogDiagnostics.front().cacheAccountedBytes.has_value())
    {
        return fail(QStringLiteral(
            "missing catalog resource did not return NotFound with one failed diagnostic"
            ));
    }

    if (loadedCatalog->records().size() != 199)
    {
        return fail(QStringLiteral("generated catalog did not contain 199 runtime records"));
    }

    int packageFileCount = 0;
    int generatedTranslationCount = 0;
    int qtResourceCount = 0;
    for (const auto& record : loadedCatalog->records())
    {
        if (record.location() == ResourceLocation::PackageFile)
        {
            ++packageFileCount;
            if (record.expectedType() == ResourceType::Qm)
            {
                ++generatedTranslationCount;
            }
        }
        else if (record.location() == ResourceLocation::QtResource)
        {
            ++qtResourceCount;
        }
    }
    if (packageFileCount != 197 || generatedTranslationCount != 5 || qtResourceCount != 2)
    {
        return fail(QStringLiteral(
            "catalog must contain 197 package-file records including 5 generated translations, "
            "and 2 Qt-resource records"
            ));
    }

    ResourceLoader loader(std::move(*loadedCatalog));
    const QString packageRoot = temporaryRoot.path();
    const QString bakedRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString translationDirectory = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY
        );

    // Only stage requested examples. The rest of the catalog, including every
    // PDF body, is absent while initialization and the first loads succeed.
    if (!stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/campuses/bundang.json")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/campuses/bundang/bundang_building.png")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/fonts/Caveat-wght.ttf")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/styles/dark.qss")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/documents/Book Reports/Book Report Grading Rubric.pdf")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"),
            translationDirectory
            ))
    {
        return false;
    }

    const auto jsonId = resourceId("resource:assets/campuses/bundang.json");
    const auto imageId = resourceId("resource:assets/campuses/bundang/bundang_building.png");
    const auto fontId = resourceId("resource:assets/fonts/Caveat-wght.ttf");
    const auto stylesheetId = resourceId("resource:assets/styles/dark.qss");
    const auto translationId = resourceId("resource:assets/translations/ClassMngr_ko_KR.ts");
    const auto pdfId = resourceId(
        "document.body:Book Reports/Book Report Grading Rubric.pdf"
        );
    const auto qmlId = resourceId("calendar.qml:EventCalendar.qml");
    if (!jsonId || !imageId || !fontId || !stylesheetId || !translationId || !pdfId || !qmlId)
    {
        return fail(QStringLiteral("test resource IDs were rejected"));
    }

    static_assert(!std::is_default_constructible_v<ResourceScope>);
    static_assert(!std::is_default_constructible_v<ResourceRequest>);
    static_assert(!std::is_copy_constructible_v<ScopedResource<QString>>);
    if (ResourceScope::create(ResourceScopeKind::Startup, QString{}).has_value()
        || ResourceScope::create(ResourceScopeKind::Page, QStringLiteral(" \t\n ")).has_value()
        || ResourceRequest::create(
            *jsonId,
            ResourceScopeKind::Operation,
            QString{}
            ).has_value())
    {
        return fail(QStringLiteral("scope or request accepted a missing or blank owner label"));
    }

    auto jsonRequest = resourceRequest(
        *jsonId,
        ResourceScopeKind::Startup,
        QStringLiteral("CampusCatalog")
        );
    auto imageRequest = resourceRequest(
        *imageId,
        ResourceScopeKind::Page,
        QStringLiteral("CampusDetails")
        );
    auto fontRequest = resourceRequest(
        *fontId,
        ResourceScopeKind::Feature,
        QStringLiteral("Typography")
        );
    auto stylesheetRequest = resourceRequest(
        *stylesheetId,
        ResourceScopeKind::Feature,
        QStringLiteral("Theme")
        );
    auto translationRequest = resourceRequest(
        *translationId,
        ResourceScopeKind::Feature,
        QStringLiteral("KoreanTranslation")
        );
    auto pdfRequest = resourceRequest(
        *pdfId,
        ResourceScopeKind::Operation,
        QStringLiteral("RubricPreview")
        );
    auto qmlRequest = resourceRequest(
        *qmlId,
        ResourceScopeKind::Page,
        QStringLiteral("EventCalendar")
        );
    auto qrcStreamRequest = resourceRequest(
        *qmlId,
        ResourceScopeKind::Operation,
        QStringLiteral("CalendarPreview")
        );
    if (!jsonRequest || !imageRequest || !fontRequest || !stylesheetRequest
        || !translationRequest || !pdfRequest || !qmlRequest || !qrcStreamRequest)
    {
        return false;
    }

    auto cachedCatalog = ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        packageRoot
        );
    if (!cachedCatalog)
    {
        return fail(QStringLiteral("cached loader catalog load failed: ")
            + cachedCatalog.error().message);
    }
    ResourceCache loaderCache(64 * 1024 * 1024);
    std::vector<ResourceDiagnostic> cacheDiagnostics;
    ResourceLoader cachedLoader(
        std::move(*cachedCatalog),
        loaderCache,
        [&cacheDiagnostics](const ResourceDiagnostic& diagnostic) {
            cacheDiagnostics.push_back(diagnostic);
        }
        );

    ResourceCache emptyLoaderCache(4096);
    std::vector<ResourceDiagnostic> emptyCacheDiagnostics;
    ResourceLoader emptyCacheLoader(
        cachedLoader.catalog(),
        emptyLoaderCache,
        [&emptyCacheDiagnostics](const ResourceDiagnostic& diagnostic) {
            emptyCacheDiagnostics.push_back(diagnostic);
        }
        );
    const auto emptyCacheResolvedFile = emptyCacheLoader.resolveFile(*pdfRequest);
    if (!emptyCacheResolvedFile
        || emptyCacheDiagnostics.size() != 1
        || !emptyCacheDiagnostics.back().succeeded
        || !emptyCacheDiagnostics.back().cacheConfigured
        || emptyCacheDiagnostics.back().cacheEntryCount != 0
        || emptyCacheDiagnostics.back().cacheAccountedBytes != 0
        || emptyCacheDiagnostics.back().cacheCapacityBytes != 4096
        || emptyCacheDiagnostics.back().cacheEntry.has_value())
    {
        return fail(QStringLiteral(
            "configured empty ResourceLoader cache was reported as unavailable"
            ));
    }

    auto cachedJson = cachedLoader.loadJson(*jsonRequest);
    if (!cachedJson || !cachedJson->get().isObject())
    {
        return fail(QStringLiteral("cached loader JSON load failed: ")
            + (cachedJson ? QStringLiteral("document was not an object")
                          : cachedJson.error().message));
    }
    const qint64 jsonWeight = static_cast<qint64>(
        cachedJson->get().toJson(QJsonDocument::Compact).size()
        );
    const ResourceDescriptor* cachedJsonDescriptor = cachedLoader.catalog().find(*jsonId);
    const auto jsonCacheSnapshot = loaderCache.snapshot(*jsonId);
    if (cacheDiagnostics.size() != 1 || cachedJsonDescriptor == nullptr
        || !jsonCacheSnapshot.entry
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *jsonId,
            ResourceScopeKind::Startup,
            QStringLiteral("CampusCatalog")
            )
        || !cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().errorCode.has_value()
        || cacheDiagnostics.back().cacheHit
        || !cacheDiagnostics.back().cacheConfigured
        || cacheDiagnostics.back().cacheEntryCount != jsonCacheSnapshot.entryCount
        || cacheDiagnostics.back().elapsed.count() < 0
        || cacheDiagnostics.back().sourceBytes != cachedJsonDescriptor->sourceBytes()
        || cacheDiagnostics.back().catalogOwner != cachedJsonDescriptor->owner()
        || cacheDiagnostics.back().catalogCategory != cachedJsonDescriptor->category()
        || cacheDiagnostics.back().resourceKind != cachedJsonDescriptor->resourceKind()
        || cacheDiagnostics.back().expectedType != cachedJsonDescriptor->expectedTypeName()
        || cacheDiagnostics.back().location != QStringLiteral("package_file")
        || cacheDiagnostics.back().decodedBytes != jsonWeight
        || jsonCacheSnapshot.entry->accountedBytes != jsonWeight
        || cacheDiagnostics.back().cacheEntryAccountedBytes
            != jsonCacheSnapshot.entry->accountedBytes
        || !cacheDiagnostics.back().cacheEntry
        || cacheDiagnostics.back().cacheEntry->kind != jsonCacheSnapshot.entry->kind
        || cacheDiagnostics.back().cacheEntry->scope.kind()
            != jsonCacheSnapshot.entry->scope.kind()
        || cacheDiagnostics.back().cacheEntry->scope.ownerLabel()
            != jsonCacheSnapshot.entry->scope.ownerLabel()
        || cacheDiagnostics.back().cacheAccountedBytes != jsonCacheSnapshot.accountedBytes
        || cacheDiagnostics.back().cacheCapacityBytes != jsonCacheSnapshot.capacityBytes)
    {
        return fail(QStringLiteral("JSON cache-miss diagnostic fields were incorrect"));
    }
    const auto jsonMetadata = loaderCache.entryInfo(*jsonId);
    auto jsonHitRequest = resourceRequest(
        *jsonId,
        ResourceScopeKind::Operation,
        QStringLiteral("JsonCacheProbe")
        );
    if (!jsonHitRequest || !jsonMetadata
        || jsonMetadata->kind != ResourceCacheValueKind::Json
        || jsonMetadata->accountedBytes != jsonWeight
        || !scopeMatches(
            jsonMetadata->scope,
            ResourceScopeKind::Startup,
            QStringLiteral("CampusCatalog")
            ))
    {
        return fail(QStringLiteral("JSON cache accounting or owner metadata was incorrect"));
    }
    auto cachedJsonHit = cachedLoader.loadJson(*jsonHitRequest);
    if (!cachedJsonHit || !cachedJsonHit->get().isObject()
        || !scopeMatches(
            cachedJsonHit->scope(),
            ResourceScopeKind::Operation,
            QStringLiteral("JsonCacheProbe")
            ))
    {
        return fail(QStringLiteral("JSON cache hit did not return a newly scoped owning handle"));
    }
    if (cacheDiagnostics.size() != 2
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *jsonId,
            ResourceScopeKind::Operation,
            QStringLiteral("JsonCacheProbe")
            )
        || !cacheDiagnostics.back().succeeded
        || !cacheDiagnostics.back().cacheHit
        || cacheDiagnostics.back().cacheEntryCount != loaderCache.entryCount()
        || cacheDiagnostics.back().decodedBytes != jsonWeight
        || cacheDiagnostics.back().cacheEntryAccountedBytes != jsonWeight
        || !cacheDiagnostics.back().cacheEntry
        || cacheDiagnostics.back().cacheEntry->kind != ResourceCacheValueKind::Json
        || cacheDiagnostics.back().cacheEntry->scope.ownerLabel()
            != QStringLiteral("CampusCatalog"))
    {
        return fail(QStringLiteral("JSON cache-hit diagnostic did not use the current owner"));
    }

    const QString cachedJsonPath = QDir(packageRoot).filePath(
        QStringLiteral("assets/campuses/bundang.json")
        );
    const QByteArray originalCachedJson = [&]() {
        QFile file(cachedJsonPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            return QByteArray{};
        }
        return file.readAll();
    }();
    const auto changedCachedJson = sameSizeValidJsonMutation(originalCachedJson);
    if (originalCachedJson.isEmpty() || !changedCachedJson)
    {
        return fail(QStringLiteral("could not prepare cached JSON integrity fixture"));
    }
    if (!writeBytes(cachedJsonPath, *changedCachedJson))
    {
        writeBytes(cachedJsonPath, originalCachedJson);
        return fail(QStringLiteral("could not write cached JSON integrity fixture"));
    }
    auto corruptedCacheHit = cachedLoader.loadJson(*jsonHitRequest);
    const bool restoredCachedJson = writeBytes(cachedJsonPath, originalCachedJson);
    if (!restoredCachedJson)
    {
        return fail(QStringLiteral("could not restore cached JSON integrity fixture"));
    }
    if (corruptedCacheHit
        || corruptedCacheHit.error().code != ResourceErrorCode::CorruptResource
        || !corruptedCacheHit.error().message.contains(QStringLiteral("SHA-256")))
    {
        return fail(QStringLiteral("JSON cache hit bypassed catalog integrity validation"));
    }
    if (cacheDiagnostics.size() != 3
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *jsonId,
            ResourceScopeKind::Operation,
            QStringLiteral("JsonCacheProbe")
            )
        || cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().errorCode != ResourceErrorCode::CorruptResource
        || cacheDiagnostics.back().cacheHit
        || cacheDiagnostics.back().decodedBytes.has_value())
    {
        return fail(QStringLiteral("corrupt cached JSON diagnostic did not report its error"));
    }

    auto cachedImage = cachedLoader.loadImage(*imageRequest);
    if (!cachedImage || cachedImage->get().isNull())
    {
        return fail(QStringLiteral("cached loader image load failed: ")
            + (cachedImage ? QStringLiteral("image was null") : cachedImage.error().message));
    }
    if (cacheDiagnostics.size() != 4
        || cacheDiagnostics.back().decodedBytes != cachedImage->get().sizeInBytes()
        || cacheDiagnostics.back().cacheHit
        || cacheDiagnostics.back().sourceImageDimensions != cachedImage->get().size()
        || cacheDiagnostics.back().decodedImageDimensions != cachedImage->get().size()
        || !cacheDiagnostics.back().succeeded)
    {
        return fail(QStringLiteral("image cache-miss diagnostic size or outcome was incorrect"));
    }
    const auto imageMetadata = loaderCache.entryInfo(*imageId);
    if (!imageMetadata || imageMetadata->kind != ResourceCacheValueKind::Image
        || imageMetadata->accountedBytes != cachedImage->get().sizeInBytes()
        || loaderCache.accountedBytes() > loaderCache.capacityBytes())
    {
        return fail(QStringLiteral("decoded image cache accounting exceeded or misreported its budget"));
    }
    auto imageHitRequest = resourceRequest(
        *imageId,
        ResourceScopeKind::Operation,
        QStringLiteral("ImageCacheProbe")
        );
    if (!imageHitRequest)
    {
        return false;
    }
    auto cachedImageHit = cachedLoader.loadImage(*imageHitRequest);
    if (!cachedImageHit || cachedImageHit->get().isNull()
        || !scopeMatches(
            cachedImageHit->scope(),
            ResourceScopeKind::Operation,
            QStringLiteral("ImageCacheProbe")
            ))
    {
        return fail(QStringLiteral("image cache hit did not return a newly scoped owning handle"));
    }
    if (cacheDiagnostics.size() != 5
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *imageId,
            ResourceScopeKind::Operation,
            QStringLiteral("ImageCacheProbe")
            )
        || !cacheDiagnostics.back().cacheHit
        || cacheDiagnostics.back().decodedBytes != cachedImageHit->get().sizeInBytes()
        || cacheDiagnostics.back().sourceImageDimensions != cachedImageHit->get().size()
        || cacheDiagnostics.back().decodedImageDimensions != cachedImageHit->get().size()
        || !cacheDiagnostics.back().cacheEntry
        || cacheDiagnostics.back().cacheEntry->scope.ownerLabel()
            != QStringLiteral("CampusDetails"))
    {
        return fail(QStringLiteral("image cache-hit diagnostic did not report its current scope"));
    }

    const qsizetype beforeStylesheetEvents = static_cast<qsizetype>(cacheDiagnostics.size());
    auto cachedStylesheet = cachedLoader.loadStylesheet(*stylesheetRequest);
    if (!cachedStylesheet
        || static_cast<qsizetype>(cacheDiagnostics.size()) != beforeStylesheetEvents + 1)
    {
        return fail(QStringLiteral("stylesheet loading failed or emitted duplicate diagnostics"));
    }
    if (!diagnosticMatches(
            cacheDiagnostics.back(),
            *stylesheetId,
            ResourceScopeKind::Feature,
            QStringLiteral("Theme")
            )
        || !cacheDiagnostics.back().succeeded)
    {
        return fail(QStringLiteral("stylesheet diagnostic did not report its logical request"));
    }
    auto cachedQmlText = cachedLoader.loadText(*qmlRequest);
    if (!cachedQmlText)
    {
        return fail(QStringLiteral("cached loader failed to decode QML text"));
    }
    if (static_cast<qsizetype>(cacheDiagnostics.size()) != beforeStylesheetEvents + 2
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *qmlId,
            ResourceScopeKind::Page,
            QStringLiteral("EventCalendar")
            )
        || cacheDiagnostics.back().decodedBytes
            != std::optional<qint64>(
                static_cast<qint64>(cachedQmlText->get().size()) * qint64(sizeof(QChar))
                ))
    {
        return fail(QStringLiteral("QML text diagnostic was missing or mis-scoped"));
    }
    const auto stylesheetMetadata = loaderCache.entryInfo(*stylesheetId);
    const auto qmlMetadata = loaderCache.entryInfo(*qmlId);
    if (!stylesheetMetadata || !qmlMetadata
        || stylesheetMetadata->kind != ResourceCacheValueKind::Text
        || qmlMetadata->kind != ResourceCacheValueKind::Text)
    {
        return fail(QStringLiteral("decoded text resources were not retained in the cache"));
    }

    const qsizetype cachedEntryCount = loaderCache.entryCount();
    const qint64 cachedAccountedBytes = loaderCache.accountedBytes();
    const qsizetype beforeFontEvents = static_cast<qsizetype>(cacheDiagnostics.size());
    QStringList loadedFontFamilies;
    {
        auto uncachedFont = cachedLoader.loadFont(*fontRequest);
        if (!uncachedFont || uncachedFont->get().familyNames().isEmpty()
            || !uncachedFont->get().sourceFilePath().isEmpty())
        {
            return fail(QStringLiteral("cache bypass font load failed: ")
                + (uncachedFont ? QStringLiteral("no family names")
                                : uncachedFont.error().message));
        }
        loadedFontFamilies = uncachedFont->get().familyNames();
    }
    if (static_cast<qsizetype>(cacheDiagnostics.size()) != beforeFontEvents + 1
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *fontId,
            ResourceScopeKind::Feature,
            QStringLiteral("Typography")
            )
        || !cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().decodedBytes.has_value()
        || !cacheDiagnostics.back().fontFamilyNames
        || *cacheDiagnostics.back().fontFamilyNames != loadedFontFamilies
        || !cacheDiagnostics.back().catalogOwner
        || cacheDiagnostics.back().catalogOwner->isEmpty())
    {
        return fail(QStringLiteral("font diagnostic decoded size should remain unavailable"));
    }
    const qsizetype beforeTranslationEvents = static_cast<qsizetype>(cacheDiagnostics.size());
    {
        auto uncachedTranslation = cachedLoader.loadTranslation(*translationRequest, application);
        if (!uncachedTranslation || !uncachedTranslation->get().isInstalled())
        {
            return fail(QStringLiteral("cache bypass translation load failed: ")
                + (uncachedTranslation ? QStringLiteral("translation was not installed")
                                       : uncachedTranslation.error().message));
        }
    }
    if (static_cast<qsizetype>(cacheDiagnostics.size()) != beforeTranslationEvents + 1
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *translationId,
            ResourceScopeKind::Feature,
            QStringLiteral("KoreanTranslation")
            )
        || !cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().decodedBytes.has_value())
    {
        return fail(QStringLiteral("translation diagnostic decoded size should remain unavailable"));
    }
    const qsizetype beforeStreamEvents = static_cast<qsizetype>(cacheDiagnostics.size());
    {
        auto uncachedPdfStream = cachedLoader.openStream(*pdfRequest);
        if (!uncachedPdfStream || !uncachedPdfStream->get().isOpen()
            || loaderCache.entryCount() != cachedEntryCount
            || loaderCache.accountedBytes() != cachedAccountedBytes
            || loaderCache.entryInfo(*pdfId).has_value())
        {
            return fail(QStringLiteral(
                "font, translation, or PDF stream paths changed cache contents"
                ));
        }
    }
    const ResourceDescriptor* pdfDescriptor = cachedLoader.catalog().find(*pdfId);
    if (static_cast<qsizetype>(cacheDiagnostics.size()) != beforeStreamEvents + 1
        || pdfDescriptor == nullptr
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *pdfId,
            ResourceScopeKind::Operation,
            QStringLiteral("RubricPreview")
            )
        || !cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().decodedBytes.has_value()
        || cacheDiagnostics.back().sourceBytes != pdfDescriptor->sourceBytes())
    {
        return fail(QStringLiteral("PDF stream diagnostic read a decoded size or lost source size"));
    }

    const qsizetype beforeResolveFileEvents =
        static_cast<qsizetype>(cacheDiagnostics.size());
    const auto resolvedPdfFile = cachedLoader.resolveFile(*pdfRequest);
    if (!resolvedPdfFile
        || !QFileInfo(resolvedPdfFile->get()).isFile())
    {
        return fail(QStringLiteral("PDF resolveFile request did not return a package file"));
    }
    if (static_cast<qsizetype>(cacheDiagnostics.size()) != beforeResolveFileEvents + 1
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *pdfId,
            ResourceScopeKind::Operation,
            QStringLiteral("RubricPreview")
            )
        || !cacheDiagnostics.back().succeeded
        || !cacheDiagnostics.back().cacheConfigured
        || cacheDiagnostics.back().sourceBytes != pdfDescriptor->sourceBytes()
        || cacheDiagnostics.back().decodedBytes.has_value()
        || cacheDiagnostics.back().catalogOwner != pdfDescriptor->owner()
        || cacheDiagnostics.back().expectedType != pdfDescriptor->expectedTypeName()
        || cacheDiagnostics.back().location != QStringLiteral("package_file"))
    {
        return fail(QStringLiteral(
            "resolveFile did not report its source-only resource diagnostic"));
    }

    const qsizetype beforeTypeMismatchEvents = static_cast<qsizetype>(cacheDiagnostics.size());
    auto diagnosticTypeMismatch = cachedLoader.loadImage(*jsonRequest);
    if (diagnosticTypeMismatch
        || diagnosticTypeMismatch.error().code != ResourceErrorCode::TypeMismatch
        || static_cast<qsizetype>(cacheDiagnostics.size()) != beforeTypeMismatchEvents + 1
        || cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().errorCode != ResourceErrorCode::TypeMismatch
        || cacheDiagnostics.back().decodedBytes.has_value())
    {
        return fail(QStringLiteral("type-mismatch diagnostic did not report the error outcome"));
    }

    std::vector<ResourceDiagnostic> isolatedDiagnostics;
    ResourceLoader isolatedDiagnosticsLoader(
        cachedLoader.catalog(),
        [&isolatedDiagnostics](const ResourceDiagnostic& diagnostic) {
            isolatedDiagnostics.push_back(diagnostic);
        }
        );
    const qsizetype cachedEventCountBeforeIsolatedLoad =
        static_cast<qsizetype>(cacheDiagnostics.size());
    auto isolatedText = isolatedDiagnosticsLoader.loadText(*qmlRequest);
    if (!isolatedText || isolatedDiagnostics.size() != 1
        || static_cast<qsizetype>(cacheDiagnostics.size()) != cachedEventCountBeforeIsolatedLoad
        || !diagnosticMatches(
            isolatedDiagnostics.back(),
            *qmlId,
            ResourceScopeKind::Page,
            QStringLiteral("EventCalendar")
            )
        || !isolatedDiagnostics.back().succeeded
        || isolatedDiagnostics.back().cacheConfigured
        || isolatedDiagnostics.back().cacheEntryCount.has_value()
        || isolatedDiagnostics.back().cacheEntry.has_value()
        || isolatedDiagnostics.back().cacheAccountedBytes.has_value()
        || isolatedDiagnostics.back().cacheCapacityBytes.has_value())
    {
        return fail(QStringLiteral("resource diagnostic sinks were shared across loader instances"));
    }

    ResourceLoader throwingDiagnosticsLoader(
        cachedLoader.catalog(),
        [](const ResourceDiagnostic&) {
            throw std::runtime_error("diagnostic sink failure");
        }
        );
    auto loadWithThrowingDiagnostics = throwingDiagnosticsLoader.loadText(*qmlRequest);
    if (!loadWithThrowingDiagnostics)
    {
        return fail(QStringLiteral("throwing diagnostics sink changed the resource load result"));
    }

    auto json = loader.loadJson(*jsonRequest);
    if (!json || !json->get().isObject())
    {
        return fail(QStringLiteral("JSON loader failed: ")
            + (json ? QStringLiteral("document was not an object") : json.error().message));
    }
    auto movedJson = std::move(*json);
    if (!scopeMatches(
            movedJson.scope(),
            ResourceScopeKind::Startup,
            QStringLiteral("CampusCatalog")
            )
        || !scopeMatches(
            json->scope(),
            ResourceScopeKind::Startup,
            QStringLiteral("CampusCatalog")
            ))
    {
        return fail(QStringLiteral("JSON handle lost its scope across a move"));
    }

    auto wrongType = loader.loadImage(*jsonRequest);
    if (wrongType || wrongType.error().code != ResourceErrorCode::TypeMismatch)
    {
        return fail(QStringLiteral("image loader did not reject a JSON resource"));
    }

    auto image = loader.loadImage(*imageRequest);
    if (!image || image->get().isNull()
        || !scopeMatches(
            image->scope(),
            ResourceScopeKind::Page,
            QStringLiteral("CampusDetails")
            ))
    {
        return fail(QStringLiteral("image loader failed: ")
            + (image ? QStringLiteral("image was null") : image.error().message));
    }
    auto replacementImageRequest = resourceRequest(
        *imageId,
        ResourceScopeKind::Operation,
        QStringLiteral("CampusImagePreview")
        );
    if (!replacementImageRequest)
    {
        return false;
    }
    auto replacementImage = loader.loadImage(*replacementImageRequest);
    if (!replacementImage)
    {
        return fail(QStringLiteral("replacement image load failed: ")
            + replacementImage.error().message);
    }
    *image = std::move(*replacementImage);
    if (!scopeMatches(
            image->scope(),
            ResourceScopeKind::Operation,
            QStringLiteral("CampusImagePreview")
            )
        || !scopeMatches(
            replacementImage->scope(),
            ResourceScopeKind::Operation,
            QStringLiteral("CampusImagePreview")
            ))
    {
        return fail(QStringLiteral("image handle lost its scope during move assignment"));
    }

    auto stylesheet = loader.loadStylesheet(*stylesheetRequest);
    if (!stylesheet || stylesheet->get().isEmpty()
        || !scopeMatches(
            stylesheet->scope(),
            ResourceScopeKind::Feature,
            QStringLiteral("Theme")
            ))
    {
        return fail(QStringLiteral("stylesheet loader failed"));
    }
    auto wrongStylesheetType = loader.loadStylesheet(*jsonRequest);
    if (wrongStylesheetType
        || wrongStylesheetType.error().code != ResourceErrorCode::TypeMismatch)
    {
        return fail(QStringLiteral("stylesheet loader did not reject a JSON resource"));
    }

    auto qmlText = loader.loadText(*qmlRequest);
    if (!qmlText || !qmlText->get().contains(QStringLiteral("Calendar"))
        || !scopeMatches(
            qmlText->scope(),
            ResourceScopeKind::Page,
            QStringLiteral("EventCalendar")
            ))
    {
        return fail(QStringLiteral("qrc Calendar text load failed: ")
            + (qmlText ? QStringLiteral("unexpected QML contents") : qmlText.error().message));
    }
    const ResourceDescriptor* qmlDescriptor = loader.catalog().find(*qmlId);
    if (qmlDescriptor == nullptr
        || qmlDescriptor->location() != ResourceLocation::QtResource
        || !qmlDescriptor->runtimeUrl().startsWith(QStringLiteral("qrc:/")))
    {
        return fail(QStringLiteral("Calendar resource URL was conflated with a package file path"));
    }
    auto qrcStream = loader.openStream(*qrcStreamRequest);
    if (qrcStream || qrcStream.error().code != ResourceErrorCode::UnsupportedOperation)
    {
        return fail(QStringLiteral("file stream loader accepted a qrc resource"));
    }

    const int fontRegistrationId = [&]() {
        auto font = loader.loadFont(*fontRequest);
        if (!font || font->get().familyNames().isEmpty()
            || !scopeMatches(
                font->scope(),
                ResourceScopeKind::Feature,
                QStringLiteral("Typography")
                ))
        {
            fail(QStringLiteral("font loader failed: ")
                + (font ? QStringLiteral("no family names") : font.error().message));
            return -2;
        }
        const int registrationId = font->get().registrationId();
        auto movedFont = std::move(*font);
        if (!scopeMatches(
                movedFont.scope(),
                ResourceScopeKind::Feature,
                QStringLiteral("Typography")
                ))
        {
            fail(QStringLiteral("font result did not retain its scope after a move"));
            return -2;
        }
        return registrationId;
    }();
    if (fontRegistrationId < 0
        || !QFontDatabase::applicationFontFamilies(fontRegistrationId).isEmpty())
    {
        return fail(QStringLiteral("font registration was not removed when its owner expired"));
    }

    const QString untranslatedPrint = QCoreApplication::translate("ActionRegistry", "Print");
    if (untranslatedPrint != QStringLiteral("Print"))
    {
        return fail(QStringLiteral("translation fallback was not the source text before installation"));
    }
    {
        auto translation = loader.loadTranslation(*translationRequest, application);
        if (!translation || !translation->get().isInstalled()
            || translation->get().translator().isEmpty()
            || !scopeMatches(
                translation->scope(),
                ResourceScopeKind::Feature,
                QStringLiteral("KoreanTranslation")
                ))
        {
            return fail(QStringLiteral("translation loader failed: ")
                + (translation ? QStringLiteral("translation was not installed")
                               : translation.error().message));
        }
        auto movedTranslation = std::move(*translation);
        if (!scopeMatches(
                movedTranslation.scope(),
                ResourceScopeKind::Feature,
                QStringLiteral("KoreanTranslation")
                ))
        {
            return fail(QStringLiteral("translation result did not retain its scope after a move"));
        }
        if (QCoreApplication::translate("ActionRegistry", "Print") != QStringLiteral("인쇄"))
        {
            return fail(QStringLiteral("Korean translator did not translate ActionRegistry::Print"));
        }
    }
    if (QCoreApplication::translate("ActionRegistry", "Print") != QStringLiteral("Print"))
    {
        return fail(QStringLiteral("translation was still installed after its owner expired"));
    }

    const QString pdfPath = QDir(packageRoot).filePath(
        QStringLiteral("assets/documents/Book Reports/Book Report Grading Rubric.pdf")
        );
    qintptr streamHandle = -1;
#if defined(Q_OS_WIN)
    HANDLE nativeStreamHandle = INVALID_HANDLE_VALUE;
#endif
    {
        auto streamResult = loader.openStream(*pdfRequest);
        if (!streamResult || !streamResult->get().isOpen()
            || !scopeMatches(
                streamResult->scope(),
                ResourceScopeKind::Operation,
                QStringLiteral("RubricPreview")
                ))
        {
            return fail(QStringLiteral("PDF file-backed stream failed: ")
                + (streamResult ? QStringLiteral("stream was closed") : streamResult.error().message));
        }
        auto stream = std::move(*streamResult);
        if (!scopeMatches(
                stream.scope(),
                ResourceScopeKind::Operation,
                QStringLiteral("RubricPreview")
                ))
        {
            return fail(QStringLiteral("stream result did not retain its scope after a move"));
        }
        const QByteArray prefix = stream.get().device().read(4);
        if (prefix != QByteArray("%PDF", 4))
        {
            return fail(QStringLiteral("PDF stream returned unexpected leading bytes"));
        }
        auto* file = dynamic_cast<QFile*>(&stream.get().device());
        if (file == nullptr)
        {
            return fail(QStringLiteral("file stream did not expose its owning QFile device"));
        }
#if defined(Q_OS_WIN)
        const auto nativeHandleValue = ::_get_osfhandle(file->handle());
        if (nativeHandleValue == -1)
        {
            return fail(QStringLiteral("file stream descriptor had no native Windows handle"));
        }
        nativeStreamHandle = reinterpret_cast<HANDLE>(nativeHandleValue);
#else
        streamHandle = file->handle();
#endif
    }
#if defined(Q_OS_UNIX)
    if (streamHandle < 0)
    {
        return fail(QStringLiteral("file stream returned an invalid POSIX descriptor"));
    }
    errno = 0;
    if (::fcntl(static_cast<int>(streamHandle), F_GETFD) != -1 || errno != EBADF)
    {
        return fail(QStringLiteral("POSIX file descriptor remained open after stream destruction"));
    }
#elif defined(Q_OS_WIN)
    if (nativeStreamHandle == INVALID_HANDLE_VALUE)
    {
        return fail(QStringLiteral("file stream returned an invalid Windows handle"));
    }
    DWORD handleFlags = 0;
    const BOOL handleIsValid = ::GetHandleInformation(nativeStreamHandle, &handleFlags);
    const DWORD handleCheckError = ::GetLastError();
    if (handleIsValid != 0 || handleCheckError != ERROR_INVALID_HANDLE)
    {
        return fail(QStringLiteral("Windows file handle remained open after stream destruction"));
    }
#else
    const QString renamedPdfPath = pdfPath + QStringLiteral(".renamed");
    if (!QFile::rename(pdfPath, renamedPdfPath) || !QFile::rename(renamedPdfPath, pdfPath))
    {
        return fail(QStringLiteral("stream handle did not close when its RAII owner expired"));
    }
#endif

    auto missingId = resourceId("resource:assets/campuses/bundang/bundang_map.png");
    if (!missingId)
    {
        return fail(QStringLiteral("missing resource ID was rejected"));
    }
    auto missingImageRequest = resourceRequest(
        *missingId,
        ResourceScopeKind::Page,
        QStringLiteral("MissingCampusMap")
        );
    if (!missingImageRequest)
    {
        return false;
    }
    auto missingImage = loader.loadImage(*missingImageRequest);
    if (missingImage || missingImage.error().code != ResourceErrorCode::MissingFile)
    {
        return fail(QStringLiteral("missing packaged image did not return a missing-file error"));
    }

    const QString stagedJsonPath = QDir(packageRoot).filePath(
        QStringLiteral("assets/campuses/bundang.json")
        );
    const QByteArray originalJson = [&]() {
        QFile file(stagedJsonPath);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray{};
        return file.readAll();
    }();
    const auto corruptedJson = sameSizeValidJsonMutation(originalJson);
    if (originalJson.isEmpty() || !corruptedJson || corruptedJson->size() != originalJson.size()
        || !writeBytes(stagedJsonPath, *corruptedJson))
    {
        return fail(QStringLiteral("could not prepare corrupt JSON fixture"));
    }
    auto corruptJson = loader.loadJson(*jsonRequest);
    if (corruptJson || corruptJson.error().code != ResourceErrorCode::CorruptResource
        || !corruptJson.error().message.contains(QStringLiteral("SHA-256")))
    {
        return fail(QStringLiteral("same-size changed JSON did not fail catalog SHA-256 validation"));
    }
    if (!writeBytes(stagedJsonPath, originalJson))
    {
        return fail(QStringLiteral("could not restore JSON fixture"));
    }

    QTemporaryDir outsideRoot;
    if (!outsideRoot.isValid())
    {
        return fail(QStringLiteral("could not create symlink escape target directory"));
    }
    const QString outsideFile = QDir(outsideRoot.path()).filePath(QStringLiteral("outside.json"));
    if (!writeBytes(outsideFile, originalJson))
    {
        return fail(QStringLiteral("could not create symlink escape target"));
    }
    if (!QFile::remove(stagedJsonPath))
    {
        return fail(QStringLiteral("could not remove staged JSON before symlink escape fixture"));
    }
    const SymlinkFixtureResult symlinkFixture =
        createFileSymlink(outsideFile, stagedJsonPath);
    if (symlinkFixture == SymlinkFixtureResult::Failed)
    {
        return false;
    }
    if (symlinkFixture == SymlinkFixtureResult::Created)
    {
        const QFileInfo stagedSymlinkInfo(stagedJsonPath);
        if (!stagedSymlinkInfo.isSymLink())
        {
            return fail(QStringLiteral("symlink escape candidate is not a genuine filesystem symlink"));
        }
        const QString canonicalPackageRoot = QFileInfo(packageRoot).canonicalFilePath();
        const QString canonicalTarget = stagedSymlinkInfo.canonicalFilePath();
        if (canonicalPackageRoot.isEmpty() || canonicalTarget.isEmpty())
        {
            return fail(QStringLiteral("could not canonicalize symlink escape fixture paths"));
        }
        const QString relativeTarget = QDir::fromNativeSeparators(
            QDir(canonicalPackageRoot).relativeFilePath(canonicalTarget)
            );
        if (relativeTarget == QStringLiteral(".")
            || (relativeTarget != QStringLiteral("..")
                && !relativeTarget.startsWith(QStringLiteral("../"))
                && !QDir::isAbsolutePath(relativeTarget)))
        {
            return fail(QStringLiteral("symlink escape target does not resolve outside resource root"));
        }
        auto escapedJson = loader.loadJson(*jsonRequest);
        if (escapedJson || escapedJson.error().code != ResourceErrorCode::UnsafePath)
        {
            return fail(QStringLiteral("resource symlink escape was not rejected"));
        }
    }
    else
    {
        std::cout << "SKIP: symlink escape assertion requires filesystem symlink privilege; "
                     "the fixture could not be created.\n";
        if (!writeBytes(stagedJsonPath, originalJson))
        {
            return fail(QStringLiteral("could not restore JSON after skipping symlink fixture"));
        }
    }

    const QString maliciousCatalogPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("malicious-catalog.json")
        );
    QFile catalogFile(QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG));
    if (!catalogFile.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("could not read source catalog for schema test"));
    }
    QJsonParseError parseError;
    QJsonDocument catalogDocument = QJsonDocument::fromJson(catalogFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !catalogDocument.isObject())
    {
        return fail(QStringLiteral("test source catalog was unexpectedly invalid"));
    }
    QJsonObject catalogObject = catalogDocument.object();
    QJsonArray records = catalogObject.value(QStringLiteral("records")).toArray();
    QJsonObject unsafeRecord = records.first().toObject();
    unsafeRecord.insert(QStringLiteral("package_relative_path"), QStringLiteral("../escape.json"));
    records[0] = unsafeRecord;
    catalogObject.insert(QStringLiteral("records"), records);
    if (!writeBytes(
            maliciousCatalogPath,
            QJsonDocument(catalogObject).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write malicious catalog fixture"));
    }
    auto unsafeCatalog = ResourceCatalog::load(maliciousCatalogPath, packageRoot);
    if (unsafeCatalog || unsafeCatalog.error().code != ResourceErrorCode::InvalidCatalog)
    {
        return fail(QStringLiteral("catalog accepted a traversal path"));
    }

    QFile originalCatalogFile(QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG));
    if (!originalCatalogFile.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("could not reopen source catalog for collision tests"));
    }
    const QJsonDocument originalCatalogDocument = QJsonDocument::fromJson(
        originalCatalogFile.readAll(), &parseError
        );
    if (parseError.error != QJsonParseError::NoError || !originalCatalogDocument.isObject())
    {
        return fail(QStringLiteral("source catalog became invalid during collision tests"));
    }
    const QJsonObject originalCatalogObject = originalCatalogDocument.object();
    const QJsonArray originalRecords = originalCatalogObject.value(
        QStringLiteral("records")
        ).toArray();

    QJsonArray duplicateIdRecords = originalRecords;
    QJsonObject duplicateIdRecord = duplicateIdRecords[1].toObject();
    duplicateIdRecord.insert(
        QStringLiteral("logical_id"),
        duplicateIdRecords[0].toObject().value(QStringLiteral("logical_id"))
        );
    duplicateIdRecords[1] = duplicateIdRecord;
    QJsonObject duplicateIdCatalog = originalCatalogObject;
    duplicateIdCatalog.insert(QStringLiteral("records"), duplicateIdRecords);
    const QString duplicateIdCatalogPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("duplicate-id-catalog.json")
        );
    if (!writeBytes(
            duplicateIdCatalogPath,
            QJsonDocument(duplicateIdCatalog).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write duplicate-ID catalog fixture"));
    }
    auto duplicateIdResult = ResourceCatalog::load(duplicateIdCatalogPath, packageRoot);
    if (duplicateIdResult
        || duplicateIdResult.error().code != ResourceErrorCode::InvalidCatalog)
    {
        return fail(QStringLiteral("catalog accepted duplicate logical IDs"));
    }

    QJsonArray duplicatePathRecords = originalRecords;
    QJsonObject duplicatePathRecord = duplicatePathRecords[1].toObject();
    duplicatePathRecord.insert(QStringLiteral("logical_id"), QStringLiteral("test:duplicate-path"));
    duplicatePathRecord.insert(
        QStringLiteral("package_relative_path"),
        duplicatePathRecords[0].toObject().value(QStringLiteral("package_relative_path"))
        );
    duplicatePathRecords[1] = duplicatePathRecord;
    QJsonObject duplicatePathCatalog = originalCatalogObject;
    duplicatePathCatalog.insert(QStringLiteral("records"), duplicatePathRecords);
    const QString duplicatePathCatalogPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("duplicate-path-catalog.json")
        );
    if (!writeBytes(
            duplicatePathCatalogPath,
            QJsonDocument(duplicatePathCatalog).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write duplicate-path catalog fixture"));
    }
    auto duplicatePathResult = ResourceCatalog::load(duplicatePathCatalogPath, packageRoot);
    if (duplicatePathResult
        || duplicatePathResult.error().code != ResourceErrorCode::InvalidCatalog)
    {
        return fail(QStringLiteral("catalog accepted duplicate package paths"));
    }

    if (ResourceId::fromString(QString{}).has_value())
    {
        return fail(QStringLiteral("empty ResourceId was accepted"));
    }
    return true;
}

}

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    if (!testResourceDiagnosticSerialization()
        || !testResourceCacheCore() || !testBoundedImageCacheIdentity()
        || !testFileBackedFontRejectsCatalogSizeOverLimit()
        || !testVerifiedInterFileBackedFontRegistration()
        || !testLoader(application))
    {
        return 1;
    }
    return 0;
}
