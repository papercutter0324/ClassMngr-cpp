#pragma once

#include <string>
#include <string_view>

namespace classmngr::engine
{

class StudentNameService final
{
public:
    [[nodiscard]] static std::string normalizeEnglish(
        std::string_view value
        );

    [[nodiscard]] static std::string normalizeKorean(
        std::string_view value
        );

    [[nodiscard]] static std::string baseKorean(
        std::string_view value
        );

    [[nodiscard]] static std::string koreanSuffix(
        std::string_view value
        );
};

} // namespace classmngr::engine
