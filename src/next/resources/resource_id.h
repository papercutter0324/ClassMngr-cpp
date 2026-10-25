#pragma once

#include <QString>

#include <optional>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Resources
{

class ResourceId final
{
public:
    ResourceId(const ResourceId&) = default;
    ResourceId(ResourceId&&) noexcept = default;
    ResourceId& operator=(const ResourceId&) = default;
    ResourceId& operator=(ResourceId&&) noexcept = default;

    [[nodiscard]] static std::optional<ResourceId> fromString(
        const QString& value
        );

    [[nodiscard]] static std::optional<ResourceId> fromString(
        std::string_view value
        );

    [[nodiscard]] const QString& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ResourceId&,
        const ResourceId&
        ) = default;

    friend bool operator<(
        const ResourceId& left,
        const ResourceId& right
        ) noexcept
    {
        return left.m_value < right.m_value;
    }

private:
    explicit ResourceId(
        QString value
        )
        : m_value(std::move(value))
    {
    }

    QString m_value;
};

}
