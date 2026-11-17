#include "resource_diagnostics.h"

#include <limits>

namespace ClassMngr::Next::Resources
{

ResourceDiagnosticRecorder::ResourceDiagnosticRecorder(
    const ResourceDiagnosticsSink& sink,
    const ResourceRequest& request,
    const ResourceCache* cache
    )
    : m_sink(sink)
    , m_cache(cache)
    , m_started(std::chrono::steady_clock::now())
    , m_event{request.id(), request.scope()}
{
    m_event.cacheConfigured = cache != nullptr;
}

ResourceDiagnosticRecorder::~ResourceDiagnosticRecorder() noexcept
{
    if (!m_sink)
    {
        return;
    }

    try
    {
        m_event.elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - m_started
            );
        if (m_cache != nullptr)
        {
            const auto snapshot = m_cache->snapshot(m_event.resourceId);
            m_event.cacheEntryCount = snapshot.entryCount;
            m_event.cacheAccountedBytes = snapshot.accountedBytes;
            m_event.cacheCapacityBytes = snapshot.capacityBytes;
            if (snapshot.entry)
            {
                m_event.cacheEntryAccountedBytes = snapshot.entry->accountedBytes;
            }
            m_event.cacheEntry = snapshot.entry;
        }
        m_sink(m_event);
    }
    catch (...)
    {
        // Diagnostics must not change the outcome of a resource load.
    }
}

void ResourceDiagnosticRecorder::setCatalogResource(
    const ResourceDescriptor& descriptor
    ) noexcept
{
    m_event.sourceBytes = descriptor.sourceBytes();
    try
    {
        m_event.catalogOwner = descriptor.owner();
        m_event.catalogCategory = descriptor.category();
        m_event.resourceKind = descriptor.resourceKind();
        m_event.expectedType = descriptor.expectedTypeName();
        m_event.location = descriptor.location() == ResourceLocation::PackageFile
            ? QStringLiteral("package_file")
            : QStringLiteral("qt_resource");
    }
    catch (...)
    {
        m_event.catalogOwner.reset();
        m_event.catalogCategory.reset();
        m_event.resourceKind.reset();
        m_event.expectedType.reset();
        m_event.location.reset();
    }
}

void ResourceDiagnosticRecorder::setDecodedBytes(qint64 bytes) noexcept
{
    m_event.decodedBytes = bytes;
}

void ResourceDiagnosticRecorder::setSourceImageDimensions(
    const QSize dimensions
    ) noexcept
{
    if (dimensions.isValid())
    {
        m_event.sourceImageDimensions = dimensions;
    }
}

void ResourceDiagnosticRecorder::setDecodedImageDimensions(
    const QSize dimensions
    ) noexcept
{
    if (dimensions.isValid())
    {
        m_event.decodedImageDimensions = dimensions;
    }
}

void ResourceDiagnosticRecorder::setFontFamilyNames(
    const QStringList& familyNames
    ) noexcept
{
    try
    {
        m_event.fontFamilyNames = familyNames;
    }
    catch (...)
    {
        m_event.fontFamilyNames.reset();
    }
}

void ResourceDiagnosticRecorder::markCacheHit() noexcept
{
    m_event.cacheHit = true;
}

void ResourceDiagnosticRecorder::setSuccess() noexcept
{
    m_event.succeeded = true;
    m_event.errorCode.reset();
}

void ResourceDiagnosticRecorder::setError(ResourceErrorCode code) noexcept
{
    m_event.succeeded = false;
    m_event.errorCode = code;
}

std::optional<qint64> resourceDecodedPayloadBytes(const QString& value) noexcept
{
    const qint64 characters = static_cast<qint64>(value.size());
    if (characters > std::numeric_limits<qint64>::max() / qint64(sizeof(QChar)))
    {
        return std::nullopt;
    }
    return characters * qint64(sizeof(QChar));
}

std::optional<qint64> resourceDecodedPayloadBytes(const QJsonDocument& value) noexcept
{
    try
    {
        return static_cast<qint64>(value.toJson(QJsonDocument::Compact).size());
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<qint64> resourceDecodedPayloadBytes(const QImage& value) noexcept
{
    const qint64 bytes = value.sizeInBytes();
    if (bytes < 0)
    {
        return std::nullopt;
    }
    return bytes;
}

}
