#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine::ClassInfoConfig
{

using StringList = std::vector<std::string>;

[[nodiscard]] const StringList& grades() noexcept;
[[nodiscard]] const StringList& days() noexcept;
[[nodiscard]] const StringList& regularHours() noexcept;
[[nodiscard]] const StringList& intensiveHours() noexcept;
[[nodiscard]] const StringList& startMinutes() noexcept;
[[nodiscard]] const StringList& endMinutes() noexcept;

[[nodiscard]] StringList levelsForGrade(std::string_view grade);
[[nodiscard]] StringList readingBooks(
    std::string_view grade,
    std::string_view level
    );
[[nodiscard]] StringList essayBooks(
    std::string_view grade,
    std::string_view level
    );

} // namespace classmngr::engine::ClassInfoConfig
