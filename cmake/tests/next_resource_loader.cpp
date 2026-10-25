#include "next/resources/resource_loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <optional>
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

bool fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return false;
}

bool createFileSymlink(const QString& targetPath, const QString& linkPath)
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
        return true;
    }
    return fail(QStringLiteral("could not create genuine file symlink fixture; "
                               "CreateSymbolicLinkW error ")
                + QString::number(GetLastError()));
#elif defined(Q_OS_UNIX)
    const QByteArray target = QFile::encodeName(targetPath);
    const QByteArray link = QFile::encodeName(linkPath);
    if (::symlink(target.constData(), link.constData()) == 0)
    {
        return true;
    }
    return fail(QStringLiteral("could not create genuine file symlink fixture; symlink error ")
                + QString::fromLocal8Bit(std::strerror(errno)));
#else
    Q_UNUSED(targetPath);
    Q_UNUSED(linkPath);
    return fail(QStringLiteral("genuine file symlink fixture is unsupported on this platform"));
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
        || absentCatalogDiagnostics.front().sourceBytes.has_value())
    {
        return fail(QStringLiteral(
            "missing catalog resource did not return NotFound with one failed diagnostic"
            ));
    }

    if (loadedCatalog->records().size() != 194)
    {
        return fail(QStringLiteral("generated catalog did not contain 194 runtime records"));
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
    if (packageFileCount != 192 || generatedTranslationCount != 5 || qtResourceCount != 2)
    {
        return fail(QStringLiteral(
            "catalog must contain 192 package-file records including 5 generated translations, "
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
        || cacheDiagnostics.back().elapsed.count() < 0
        || cacheDiagnostics.back().sourceBytes != cachedJsonDescriptor->sourceBytes()
        || cacheDiagnostics.back().decodedBytes != jsonWeight
        || jsonCacheSnapshot.entry->accountedBytes != jsonWeight
        || cacheDiagnostics.back().cacheEntryAccountedBytes
            != jsonCacheSnapshot.entry->accountedBytes
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
        || cacheDiagnostics.back().decodedBytes != jsonWeight
        || cacheDiagnostics.back().cacheEntryAccountedBytes != jsonWeight)
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
        || cacheDiagnostics.back().decodedBytes != cachedImageHit->get().sizeInBytes())
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
    {
        auto uncachedFont = cachedLoader.loadFont(*fontRequest);
        if (!uncachedFont || uncachedFont->get().familyNames().isEmpty())
        {
            return fail(QStringLiteral("cache bypass font load failed: ")
                + (uncachedFont ? QStringLiteral("no family names")
                                : uncachedFont.error().message));
        }
    }
    if (static_cast<qsizetype>(cacheDiagnostics.size()) != beforeFontEvents + 1
        || !diagnosticMatches(
            cacheDiagnostics.back(),
            *fontId,
            ResourceScopeKind::Feature,
            QStringLiteral("Typography")
            )
        || !cacheDiagnostics.back().succeeded
        || cacheDiagnostics.back().decodedBytes.has_value())
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
    if (!createFileSymlink(outsideFile, stagedJsonPath))
    {
        return false;
    }
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
    if (!testResourceCacheCore() || !testLoader(application))
    {
        return 1;
    }
    return 0;
}
