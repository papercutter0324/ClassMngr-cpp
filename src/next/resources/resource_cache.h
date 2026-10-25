#pragma once

#include "resource_scope.h"

#include <QImage>
#include <QJsonDocument>
#include <QString>

#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <variant>

namespace ClassMngr::Next::Resources
{

enum class ResourceCacheValueKind
{
    Text,
    Json,
    Image
};

class ResourceCache final
{
public:
    struct EntryInfo final
    {
        ResourceCacheValueKind kind;
        qint64 accountedBytes;
        ResourceScope scope;
    };

    struct Snapshot final
    {
        qint64 capacityBytes;
        qint64 accountedBytes;
        std::optional<EntryInfo> entry;
    };

    explicit ResourceCache(qint64 capacityBytes);

    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    ResourceCache(ResourceCache&&) = delete;
    ResourceCache& operator=(ResourceCache&&) = delete;

    [[nodiscard]] qint64 capacityBytes() const noexcept { return m_capacityBytes; }
    // Payload accounting excludes map/list, allocator, and Qt object overhead.
    // Text uses UTF-16 payload bytes, images use decoded pixel bytes, and JSON
    // uses compact UTF-8 serialization bytes as a deterministic weight.
    [[nodiscard]] qint64 accountedBytes() const;
    [[nodiscard]] qsizetype entryCount() const;

    [[nodiscard]] std::optional<QString> findText(const ResourceId& id);
    [[nodiscard]] std::optional<QJsonDocument> findJson(const ResourceId& id);
    [[nodiscard]] std::optional<QImage> findImage(const ResourceId& id);

    [[nodiscard]] Snapshot snapshot(const ResourceId& id) const;

    void insertText(
        const ResourceId& id,
        const QString& value,
        const ResourceScope& scope
        );
    void insertJson(
        const ResourceId& id,
        const QJsonDocument& value,
        const ResourceScope& scope
        );
    void insertImage(
        const ResourceId& id,
        const QImage& value,
        const ResourceScope& scope
        );

    [[nodiscard]] std::optional<EntryInfo> entryInfo(const ResourceId& id) const;

private:
    using Value = std::variant<QString, QJsonDocument, QImage>;

    struct Entry final
    {
        ResourceCacheValueKind kind;
        qint64 accountedBytes;
        ResourceScope scope;
        Value value;
        std::list<QString>::iterator recency;
    };

    [[nodiscard]] Entry* findAndTouch(
        const ResourceId& id,
        ResourceCacheValueKind kind
        );
    void insert(
        const ResourceId& id,
        ResourceCacheValueKind kind,
        qint64 accountedBytes,
        Value value,
        const ResourceScope& scope
        );
    void remove(const std::map<QString, Entry>::iterator& entry) noexcept;

    qint64 m_capacityBytes;
    mutable std::mutex m_mutex;
    qint64 m_accountedBytes = 0;
    std::list<QString> m_recency;
    std::map<QString, Entry> m_entries;
};

}
