#pragma once

#include "resource_error.h"
#include "resource_id.h"

#include <QString>

#include <type_traits>
#include <utility>

namespace ClassMngr::Next::Resources
{

enum class ResourceScopeKind
{
    Startup,
    Page,
    Feature,
    Operation
};

class ResourceScope final
{
public:
    ResourceScope(const ResourceScope&) = default;
    ResourceScope(ResourceScope&&) noexcept = default;
    ResourceScope& operator=(const ResourceScope&) = default;
    ResourceScope& operator=(ResourceScope&&) noexcept = default;

    [[nodiscard]] static ResourceResult<ResourceScope> create(
        ResourceScopeKind kind,
        QString ownerLabel
        );

    [[nodiscard]] ResourceScopeKind kind() const noexcept { return m_kind; }
    [[nodiscard]] const QString& ownerLabel() const noexcept { return m_ownerLabel; }

    friend bool operator==(const ResourceScope&, const ResourceScope&) = default;

private:
    friend class ResourceRequest;

    ResourceScope(ResourceScopeKind kind, QString ownerLabel) noexcept
        : m_kind(kind)
        , m_ownerLabel(std::move(ownerLabel))
    {
    }

    [[nodiscard]] bool isValid() const noexcept;

    ResourceScopeKind m_kind;
    QString m_ownerLabel;
};

class ResourceRequest final
{
public:
    ResourceRequest(const ResourceRequest&) = default;
    ResourceRequest(ResourceRequest&&) noexcept = default;
    ResourceRequest& operator=(const ResourceRequest&) = default;
    ResourceRequest& operator=(ResourceRequest&&) noexcept = default;

    [[nodiscard]] static ResourceResult<ResourceRequest> create(
        ResourceId id,
        ResourceScope scope
        );
    [[nodiscard]] static ResourceResult<ResourceRequest> create(
        ResourceId id,
        ResourceScopeKind kind,
        QString ownerLabel
        );

    [[nodiscard]] const ResourceId& id() const noexcept { return m_id; }
    [[nodiscard]] const ResourceScope& scope() const noexcept { return m_scope; }

private:
    ResourceRequest(ResourceId id, ResourceScope scope) noexcept
        : m_id(std::move(id))
        , m_scope(std::move(scope))
    {
    }

    ResourceId m_id;
    ResourceScope m_scope;
};

class ResourceLoader;

template <typename Value>
class ScopedResource final
{
public:
    ScopedResource(const ScopedResource&) = delete;
    ScopedResource& operator=(const ScopedResource&) = delete;
    ScopedResource(ScopedResource&&) noexcept(
        std::is_nothrow_move_constructible_v<Value>
        && std::is_nothrow_copy_constructible_v<ResourceScope>
        );
    ScopedResource& operator=(ScopedResource&&) noexcept(
        std::is_nothrow_move_assignable_v<Value>
        && std::is_nothrow_copy_assignable_v<ResourceScope>
        );
    ~ScopedResource() = default;

    [[nodiscard]] Value& get() noexcept { return m_value; }
    [[nodiscard]] const Value& get() const noexcept { return m_value; }
    [[nodiscard]] const ResourceScope& scope() const noexcept { return m_scope; }

private:
    friend class ResourceLoader;

    ScopedResource(Value value, const ResourceScope& scope)
        : m_scope(scope)
        , m_value(std::move(value))
    {
    }

    // Destroy the owned value before releasing the metadata that describes it.
    ResourceScope m_scope;
    Value m_value;
};

template <typename Value>
ScopedResource<Value>::ScopedResource(ScopedResource&& other) noexcept(
    std::is_nothrow_move_constructible_v<Value>
    && std::is_nothrow_copy_constructible_v<ResourceScope>
    )
    : m_scope(other.m_scope)
    , m_value(std::move(other.m_value))
{
}

template <typename Value>
ScopedResource<Value>& ScopedResource<Value>::operator=(ScopedResource&& other) noexcept(
    std::is_nothrow_move_assignable_v<Value>
    && std::is_nothrow_copy_assignable_v<ResourceScope>
    )
{
    if (this != &other)
    {
        // Keep this scope with the old value until its move assignment releases it.
        m_value = std::move(other.m_value);
        m_scope = other.m_scope;
    }
    return *this;
}

}
