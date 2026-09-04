#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class StudentNameIssue
{
    EnglishTooLong,
    EnglishContainsNonAscii,
    EnglishContainsInvalidCharacters,
    KoreanTooShort,
    KoreanUnusualLength,
    KoreanTooLong,
    KoreanContainsInvalidCharacters
};

class StudentNameService final
{
public:
    [[nodiscard]] static std::vector<StudentNameIssue> validateEnglish(
        std::string_view value,
        std::size_t maximumLength = 20
        );

    [[nodiscard]] static std::vector<StudentNameIssue> validateKorean(
        std::string_view value
        );

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

    [[nodiscard]] static std::string namePairKey(
        std::string_view englishName,
        std::string_view koreanName
        );
};

} // namespace classmngr::engine
