#pragma once

#include "next/domain/teacher_profile_fields.h"

#include <compare>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Domain
{

class TeacherId final
{
public:
    [[nodiscard]] static std::optional<TeacherId> fromInt(
        const int value
        )
    {
        if (value <= 0)
        {
            return std::nullopt;
        }

        return TeacherId(value);
    }

    [[nodiscard]] int value() const
    {
        return m_value;
    }

    friend bool operator==(
        const TeacherId&,
        const TeacherId&
        ) = default;

    friend auto operator<=>(
        const TeacherId&,
        const TeacherId&
        ) = default;

private:
    explicit TeacherId(
        const int value
        )
        : m_value(value)
    {
    }

    int m_value;
};

struct TeacherProfile final
{
    TeacherId id;
    TeacherProfileFields fields;

    friend bool operator==(
        const TeacherProfile&,
        const TeacherProfile&
        ) = default;
};

} // namespace ClassMngr::Next::Domain
