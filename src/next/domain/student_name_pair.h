#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Domain
{

class StudentNamePair final
{
public:
    [[nodiscard]] static std::optional<StudentNamePair> fromNames(
        const std::u16string_view englishName,
        const std::u16string_view koreanName
        )
    {
        if (englishName.empty() || koreanName.empty())
        {
            return std::nullopt;
        }

        return StudentNamePair(
            std::u16string(englishName),
            std::u16string(koreanName)
            );
    }

    [[nodiscard]] const std::u16string& englishName() const noexcept
    {
        return m_englishName;
    }

    [[nodiscard]] const std::u16string& koreanName() const noexcept
    {
        return m_koreanName;
    }

    friend bool operator==(
        const StudentNamePair&,
        const StudentNamePair&
        ) = default;

    friend auto operator<=>(
        const StudentNamePair&,
        const StudentNamePair&
        ) = default;

private:
    StudentNamePair(
        std::u16string englishName,
        std::u16string koreanName
        )
        : m_englishName(std::move(englishName)),
          m_koreanName(std::move(koreanName))
    {
    }

    std::u16string m_englishName;
    std::u16string m_koreanName;
};

} // namespace ClassMngr::Next::Domain
