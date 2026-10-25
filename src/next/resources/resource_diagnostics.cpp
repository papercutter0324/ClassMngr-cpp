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
            m_event.cacheAccountedBytes = snapshot.accountedBytes;
            m_event.cacheCapacityBytes = snapshot.capacityBytes;
            if (snapshot.entry)
            {
                m_event.cacheEntryAccountedBytes = snapshot.entry->accountedBytes;
            }
        }
        m_sink(m_event);
    }
    catch (...)
    {
        // Diagnostics must not change the outcome of a resource load.
    }
}

void ResourceDiagnosticRecorder::setSourceBytes(qint64 bytes) noexcept
{
    m_event.sourceBytes = bytes;
}

void ResourceDiagnosticRecorder::setDecodedBytes(qint64 bytes) noexcept
{
    m_event.decodedBytes = bytes;
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
