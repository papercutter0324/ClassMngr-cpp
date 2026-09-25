#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Domain
{

// Owns the first available teacher name in the application's display order.
// Platform adapters are responsible for normalizing their source strings
// before passing them here.
class TeacherDisplayName final
{
public:
    [[nodiscard]] static TeacherDisplayName select(
        const std::u16string_view preferredName,
        const std::u16string_view englishName,
        const std::u16string_view preferredRomanization,
        const std::u16string_view koreanName
        )
    {
        if (!preferredName.empty())
        {
            return TeacherDisplayName(std::u16string(preferredName));
        }
        if (!englishName.empty())
        {
            return TeacherDisplayName(std::u16string(englishName));
        }
        if (!preferredRomanization.empty())
        {
            return TeacherDisplayName(std::u16string(preferredRomanization));
        }
        return TeacherDisplayName(std::u16string(koreanName));
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
        const TeacherDisplayName&,
        const TeacherDisplayName&
        ) = default;

private:
    explicit TeacherDisplayName(std::u16string value)
        : m_value(std::move(value))
    {
    }

    std::u16string m_value;
};

} // namespace ClassMngr::Next::Domain
