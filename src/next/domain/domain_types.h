#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Domain
{

template <typename Tag>
class TypedId final
{
public:
    [[nodiscard]] static std::optional<TypedId> fromString(
        std::string_view value
        )
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        return TypedId(std::string(value));
    }

    [[nodiscard]] const std::string& value() const
    {
        return m_value;
    }

    friend bool operator==(
        const TypedId&,
        const TypedId&
        ) = default;

    friend auto operator<=> (
        const TypedId&,
        const TypedId&
        ) = default;

private:
    explicit TypedId(
        std::string value
        )
        : m_value(std::move(value))
    {
    }

    std::string m_value;
};

struct WorkspaceIdTag;
struct TeacherIdTag;
struct ClassIdTag;
struct CampusIdTag;
struct CalendarEventIdTag;

using WorkspaceId = TypedId<WorkspaceIdTag>;
using TeacherId = TypedId<TeacherIdTag>;
using ClassId = TypedId<ClassIdTag>;
using CampusId = TypedId<CampusIdTag>;
using CalendarEventId = TypedId<CalendarEventIdTag>;

}
