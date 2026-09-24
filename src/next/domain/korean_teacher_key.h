#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Domain
{

class KoreanTeacherKey final
{
public:
    [[nodiscard]] static constexpr bool isHangulCodeUnit(
        const char16_t codeUnit
        ) noexcept
    {
        return (codeUnit >= 0x1100 && codeUnit <= 0x11ff)
            || (codeUnit >= 0x3130 && codeUnit <= 0x318f)
            || (codeUnit >= 0xa960 && codeUnit <= 0xa97f)
            || (codeUnit >= 0xac00 && codeUnit <= 0xd7af)
            || (codeUnit >= 0xd7b0 && codeUnit <= 0xd7ff);
    }

    [[nodiscard]] static KoreanTeacherKey fromName(
        const std::u16string_view value
        )
    {
        std::u16string key;
        key.reserve(value.size());
        for (const char16_t codeUnit : value)
        {
            if (isHangulCodeUnit(codeUnit))
            {
                key.push_back(codeUnit);
            }
        }
        return KoreanTeacherKey(std::move(key));
    }

    [[nodiscard]] const std::u16string& value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    friend bool operator==(
        const KoreanTeacherKey&,
        const KoreanTeacherKey&
        ) = default;

private:
    explicit KoreanTeacherKey(std::u16string value)
        : m_value(std::move(value))
    {
    }

    std::u16string m_value;
};

} // namespace ClassMngr::Next::Domain
