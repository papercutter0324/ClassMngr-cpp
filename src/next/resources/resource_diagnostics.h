#pragma once

#include "resource_cache.h"
#include "resource_descriptor.h"
#include "resource_error.h"
#include "resource_scope.h"

#include <QSize>
#include <QStringList>

#include <chrono>
#include <functional>
#include <optional>

namespace ClassMngr::Next::Resources
{

struct ResourceDiagnostic final
{
    ResourceId resourceId;
    ResourceScope scope;
    std::optional<QString> catalogOwner;
    std::optional<QString> catalogCategory;
    std::optional<QString> resourceKind;
    std::optional<QString> expectedType;
    std::optional<QString> location;
    // Monotonic time covering descriptor lookup, verification, decode, and cache lookup.
    std::chrono::nanoseconds elapsed{};
    // Catalog source size; absent when the ID is not in the catalog.
    std::optional<qint64> sourceBytes;
    // Payload accounting: UTF-16 text, compact-UTF-8 JSON, and decoded image
    // pixels. Unavailable for fonts, translations, and streams.
    std::optional<qint64> decodedBytes;
    std::optional<QSize> sourceImageDimensions;
    std::optional<QSize> decodedImageDimensions;
    std::optional<QStringList> fontFamilyNames;
    bool succeeded = false;
    bool cacheConfigured = false;
    bool cacheHit = false;
    std::optional<qsizetype> cacheEntryCount;
    std::optional<qint64> cacheEntryAccountedBytes;
    std::optional<qint64> cacheAccountedBytes;
    std::optional<qint64> cacheCapacityBytes;
    std::optional<ResourceCache::EntryInfo> cacheEntry;
    std::optional<ResourceErrorCode> errorCode;
};

// A loader owns a copy of this callback. It is invoked synchronously on the
// loading thread. If a loader is called concurrently, its sink may also be
// invoked concurrently and must be thread-safe. Callback exceptions are
// swallowed so diagnostics cannot replace the ResourceResult produced by a load.
using ResourceDiagnosticsSink = std::function<void(const ResourceDiagnostic&)>;

class ResourceDiagnosticRecorder final
{
public:
    ResourceDiagnosticRecorder(
        const ResourceDiagnosticsSink& sink,
        const ResourceRequest& request,
        const ResourceCache* cache
        );
    ~ResourceDiagnosticRecorder() noexcept;

    ResourceDiagnosticRecorder(const ResourceDiagnosticRecorder&) = delete;
    ResourceDiagnosticRecorder& operator=(const ResourceDiagnosticRecorder&) = delete;

    void setCatalogResource(const ResourceDescriptor& descriptor) noexcept;
    void setDecodedBytes(qint64 bytes) noexcept;
    void setSourceImageDimensions(QSize dimensions) noexcept;
    void setDecodedImageDimensions(QSize dimensions) noexcept;
    void setFontFamilyNames(const QStringList& familyNames) noexcept;
    void markCacheHit() noexcept;
    void setSuccess() noexcept;
    void setError(ResourceErrorCode code) noexcept;

private:
    const ResourceDiagnosticsSink& m_sink;
    const ResourceCache* m_cache;
    std::chrono::steady_clock::time_point m_started;
    ResourceDiagnostic m_event;
};

[[nodiscard]] std::optional<qint64> resourceDecodedPayloadBytes(
    const QString& value
    ) noexcept;
[[nodiscard]] std::optional<qint64> resourceDecodedPayloadBytes(
    const QJsonDocument& value
    ) noexcept;
[[nodiscard]] std::optional<qint64> resourceDecodedPayloadBytes(
    const QImage& value
    ) noexcept;

}
