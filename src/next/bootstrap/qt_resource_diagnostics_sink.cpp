#include "next/bootstrap/qt_resource_diagnostics_sink.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <mutex>

namespace ClassMngr::Next::Bootstrap
{
namespace
{

const char* scopeKindName(const Resources::ResourceScopeKind kind) noexcept
{
    switch (kind)
    {
    case Resources::ResourceScopeKind::Startup:
        return "startup";
    case Resources::ResourceScopeKind::Page:
        return "page";
    case Resources::ResourceScopeKind::Feature:
        return "feature";
    case Resources::ResourceScopeKind::Operation:
        return "operation";
    case Resources::ResourceScopeKind::ViewerSession:
        return "viewer_session";
    }
    return "unknown";
}

const char* cacheValueKindName(
    const Resources::ResourceCacheValueKind kind
    ) noexcept
{
    switch (kind)
    {
    case Resources::ResourceCacheValueKind::Text:
        return "text";
    case Resources::ResourceCacheValueKind::Json:
        return "json";
    case Resources::ResourceCacheValueKind::Image:
        return "image";
    }
    return "unknown";
}

const char* resourceErrorCodeName(
    const Resources::ResourceErrorCode code
    ) noexcept
{
    switch (code)
    {
    case Resources::ResourceErrorCode::InvalidArgument:
        return "invalid_argument";
    case Resources::ResourceErrorCode::InvalidCatalog:
        return "invalid_catalog";
    case Resources::ResourceErrorCode::NotFound:
        return "not_found";
    case Resources::ResourceErrorCode::TypeMismatch:
        return "type_mismatch";
    case Resources::ResourceErrorCode::UnsafePath:
        return "unsafe_path";
    case Resources::ResourceErrorCode::MissingFile:
        return "missing_file";
    case Resources::ResourceErrorCode::CorruptResource:
        return "corrupt_resource";
    case Resources::ResourceErrorCode::IoError:
        return "io_error";
    case Resources::ResourceErrorCode::UnsupportedOperation:
        return "unsupported_operation";
    case Resources::ResourceErrorCode::DecodeError:
        return "decode_error";
    case Resources::ResourceErrorCode::RegistrationFailed:
        return "registration_failed";
    }
    return "unknown";
}

QJsonValue optionalString(const std::optional<QString>& value)
{
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

QJsonValue optionalBytes(const std::optional<qint64>& value)
{
    return value
        ? QJsonValue(static_cast<double>(*value))
        : QJsonValue(QJsonValue::Null);
}

QJsonValue optionalDimensions(const std::optional<QSize>& dimensions)
{
    return dimensions
        ? QJsonValue(QJsonObject{
            {QStringLiteral("width_pixels"), dimensions->width()},
            {QStringLiteral("height_pixels"), dimensions->height()}
        })
        : QJsonValue(QJsonValue::Null);
}

QJsonObject scopeObject(const Resources::ResourceScope& scope)
{
    return {
        {QStringLiteral("kind"), QString::fromLatin1(scopeKindName(scope.kind()))},
        {QStringLiteral("owner"), scope.ownerLabel()}
    };
}

QJsonValue cacheEntryObject(
    const std::optional<Resources::ResourceCache::EntryInfo>& entry
    )
{
    if (!entry)
    {
        return QJsonValue(QJsonValue::Null);
    }

    const bool isImage = entry->kind == Resources::ResourceCacheValueKind::Image;
    return QJsonValue(QJsonObject{
        {QStringLiteral("kind"), QString::fromLatin1(cacheValueKindName(entry->kind))},
        {QStringLiteral("accounted_payload_bytes"),
         static_cast<double>(entry->accountedBytes)},
        {QStringLiteral("owner_scope"), scopeObject(entry->scope)},
        {QStringLiteral("image_maximum_dimension_pixels"),
         isImage ? QJsonValue(entry->imageMaximumDimension)
                 : QJsonValue(QJsonValue::Null)}
    });
}

std::mutex& outputMutex()
{
    static std::mutex mutex;
    return mutex;
}

}  // namespace

QJsonObject qtResourceDiagnosticObject(
    const Resources::ResourceDiagnostic& diagnostic
    )
{
    QJsonArray fontFamilyNames;
    if (diagnostic.fontFamilyNames)
    {
        for (const QString& family : *diagnostic.fontFamilyNames)
        {
            fontFamilyNames.append(family);
        }
    }

    const QJsonValue cacheConfigured = diagnostic.cacheConfigured;
    const QJsonValue cacheHit = diagnostic.cacheConfigured
        ? QJsonValue(diagnostic.cacheHit)
        : QJsonValue(QJsonValue::Null);
    const QJsonValue cacheEntryCount = diagnostic.cacheEntryCount
        ? QJsonValue(static_cast<double>(*diagnostic.cacheEntryCount))
        : QJsonValue(QJsonValue::Null);
    const QJsonValue cacheAccountedBytes = optionalBytes(
        diagnostic.cacheAccountedBytes);
    const QJsonValue cacheCapacityBytes = optionalBytes(
        diagnostic.cacheCapacityBytes);

    return {
        {QStringLiteral("event"), QStringLiteral("resource-load")},
        {QStringLiteral("resource_id"), diagnostic.resourceId.value()},
        {QStringLiteral("request_scope"), scopeObject(diagnostic.scope)},
        {QStringLiteral("catalog_resource"), QJsonObject{
            {QStringLiteral("owner"), optionalString(diagnostic.catalogOwner)},
            {QStringLiteral("category"), optionalString(diagnostic.catalogCategory)},
            {QStringLiteral("kind"), optionalString(diagnostic.resourceKind)},
            {QStringLiteral("type"), optionalString(diagnostic.expectedType)},
            {QStringLiteral("location"), optionalString(diagnostic.location)}
        }},
        {QStringLiteral("outcome"), diagnostic.succeeded
            ? QStringLiteral("succeeded") : QStringLiteral("failed")},
        {QStringLiteral("error_code"), diagnostic.errorCode
            ? QJsonValue(QString::fromLatin1(
                resourceErrorCodeName(*diagnostic.errorCode)))
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("elapsed_nanoseconds"),
         static_cast<double>(diagnostic.elapsed.count())},
        {QStringLiteral("payload_accounting"), QJsonObject{
            {QStringLiteral("source_catalog_bytes"),
             optionalBytes(diagnostic.sourceBytes)},
            {QStringLiteral("decoded_payload_bytes"),
             optionalBytes(diagnostic.decodedBytes)},
            {QStringLiteral("meaning"), QStringLiteral(
                "Payload weights only; decoded bytes are not resident allocation sizes")}
        }},
        {QStringLiteral("image"), QJsonObject{
            {QStringLiteral("source_dimensions"),
             optionalDimensions(diagnostic.sourceImageDimensions)},
            {QStringLiteral("decoded_dimensions"),
             optionalDimensions(diagnostic.decodedImageDimensions)},
            {QStringLiteral("decoded_payload_bytes"),
             optionalBytes(diagnostic.decodedBytes)}
        }},
        {QStringLiteral("font"), QJsonObject{
            {QStringLiteral("family_names"), diagnostic.fontFamilyNames
                ? QJsonValue(fontFamilyNames)
                : QJsonValue(QJsonValue::Null)}
        }},
        {QStringLiteral("cache"), QJsonObject{
            {QStringLiteral("configured"), cacheConfigured},
            {QStringLiteral("hit"), cacheHit},
            {QStringLiteral("entry_count"), cacheEntryCount},
            {QStringLiteral("accounted_payload_bytes"), cacheAccountedBytes},
            {QStringLiteral("capacity_payload_bytes"), cacheCapacityBytes},
            {QStringLiteral("entry"), cacheEntryObject(diagnostic.cacheEntry)}
        }}
    };
}

QByteArray qtResourceDiagnosticRecord(
    const Resources::ResourceDiagnostic& diagnostic
    )
{
    return QJsonDocument(qtResourceDiagnosticObject(diagnostic))
        .toJson(QJsonDocument::Compact);
}

Resources::ResourceDiagnosticsSink makeQtResourceDiagnosticsSink()
{
    return [](const Resources::ResourceDiagnostic& diagnostic) {
        try
        {
            const QByteArray record = qtResourceDiagnosticRecord(diagnostic);
            const std::lock_guard lock(outputMutex());
            qInfo().noquote().nospace()
                << "ClassMngrNext resource checkpoint=" << record;
        }
        catch (...)
        {
            qWarning("ClassMngrNext resource diagnostics failed.");
        }
    };
}

}  // namespace ClassMngr::Next::Bootstrap
