#pragma once

#include <cstddef>

namespace ClassMngr::Next::Application
{

enum class TeacherImportMatchCardinality
{
    NoMatch,
    UniqueMatch,
    MultipleMatches
};

constexpr TeacherImportMatchCardinality classifyTeacherImportMatchCardinality(
    const std::size_t matchCount
    ) noexcept
{
    if (matchCount == 0)
    {
        return TeacherImportMatchCardinality::NoMatch;
    }
    if (matchCount == 1)
    {
        return TeacherImportMatchCardinality::UniqueMatch;
    }
    return TeacherImportMatchCardinality::MultipleMatches;
}

} // namespace ClassMngr::Next::Application
