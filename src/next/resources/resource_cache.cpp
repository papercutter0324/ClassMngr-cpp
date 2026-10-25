#include "resource_cache.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace ClassMngr::Next::Resources
{
namespace
{

std::optional<qint64> textResidentBytes(const QString& value) noexcept
{
    const qint64 characters = static_cast<qint64>(value.size());
    if (characters > std::numeric_limits<qint64>::max() / qint64(sizeof(QChar)))
    {
        return std::nullopt;
    }
    return characters * qint64(sizeof(QChar));
}

qint64 jsonAccountedBytes(const QJsonDocument& value)
{
    // Compact UTF-8 size is a deterministic payload accounting weight. It is
    // not a measurement of QJsonDocument's internal allocation.
    return static_cast<qint64>(value.toJson(QJsonDocument::Compact).size());
}

}

ResourceCache::ResourceCache(qint64 capacityBytes)
    : m_capacityBytes(capacityBytes)
{
    if (capacityBytes <= 0)
    {
        throw std::invalid_argument("resource cache capacity must be positive");
    }
}

qint64 ResourceCache::accountedBytes() const
{
    const std::lock_guard lock(m_mutex);
    return m_accountedBytes;
}

qsizetype ResourceCache::entryCount() const
{
    const std::lock_guard lock(m_mutex);
    return static_cast<qsizetype>(m_entries.size());
}

ResourceCache::Entry* ResourceCache::findAndTouch(
    const ResourceId& id,
    ResourceCacheValueKind kind
    )
{
    auto found = m_entries.find(id.value());
    if (found == m_entries.end() || found->second.kind != kind)
    {
        return nullptr;
    }

    m_recency.splice(m_recency.begin(), m_recency, found->second.recency);
    return &found->second;
}

std::optional<QString> ResourceCache::findText(const ResourceId& id)
{
    const std::lock_guard lock(m_mutex);
    Entry* found = findAndTouch(id, ResourceCacheValueKind::Text);
    if (found == nullptr)
    {
        return std::nullopt;
    }
    return std::get<QString>(found->value);
}

std::optional<QJsonDocument> ResourceCache::findJson(const ResourceId& id)
{
    const std::lock_guard lock(m_mutex);
    Entry* found = findAndTouch(id, ResourceCacheValueKind::Json);
    if (found == nullptr)
    {
        return std::nullopt;
    }
    return std::get<QJsonDocument>(found->value);
}

std::optional<QImage> ResourceCache::findImage(const ResourceId& id)
{
    const std::lock_guard lock(m_mutex);
    Entry* found = findAndTouch(id, ResourceCacheValueKind::Image);
    if (found == nullptr)
    {
        return std::nullopt;
    }
    return std::get<QImage>(found->value);
}

void ResourceCache::insertText(
    const ResourceId& id,
    const QString& value,
    const ResourceScope& scope
    )
{
    const auto residentSize = textResidentBytes(value);
    if (!residentSize)
    {
        return;
    }
    insert(
        id,
        ResourceCacheValueKind::Text,
        *residentSize,
        Value(value),
        scope
        );
}

void ResourceCache::insertJson(
    const ResourceId& id,
    const QJsonDocument& value,
    const ResourceScope& scope
    )
{
    insert(
        id,
        ResourceCacheValueKind::Json,
        jsonAccountedBytes(value),
        Value(value),
        scope
        );
}

void ResourceCache::insertImage(
    const ResourceId& id,
    const QImage& value,
    const ResourceScope& scope
    )
{
    insert(
        id,
        ResourceCacheValueKind::Image,
        value.sizeInBytes(),
        Value(value),
        scope
        );
}

void ResourceCache::insert(
    const ResourceId& id,
    ResourceCacheValueKind kind,
    qint64 accountedSize,
    Value value,
    const ResourceScope& scope
    )
{
    const std::lock_guard lock(m_mutex);
    if (accountedSize < 0 || accountedSize > m_capacityBytes
        || id.value().startsWith(QStringLiteral("document.body:")))
    {
        return;
    }

    const QString key = id.value();
    const auto existing = m_entries.find(key);
    if (existing != m_entries.end())
    {
        remove(existing);
    }

    while (m_accountedBytes > m_capacityBytes - accountedSize)
    {
        const auto leastRecent = std::prev(m_recency.end());
        remove(m_entries.find(*leastRecent));
    }

    m_recency.push_front(key);
    Entry entry{
        kind,
        accountedSize,
        scope,
        std::move(value),
        m_recency.begin()
    };
    m_entries.emplace(key, std::move(entry));
    m_accountedBytes += accountedSize;
}

void ResourceCache::remove(const std::map<QString, Entry>::iterator& entry) noexcept
{
    m_accountedBytes -= entry->second.accountedBytes;
    m_recency.erase(entry->second.recency);
    m_entries.erase(entry);
}

std::optional<ResourceCache::EntryInfo> ResourceCache::entryInfo(const ResourceId& id) const
{
    const std::lock_guard lock(m_mutex);
    const auto found = m_entries.find(id.value());
    if (found == m_entries.end())
    {
        return std::nullopt;
    }
    return EntryInfo{
        found->second.kind,
        found->second.accountedBytes,
        found->second.scope
    };
}

}
