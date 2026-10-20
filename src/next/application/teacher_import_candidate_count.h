#pragma once

#include <cstddef>

namespace ClassMngr::Next::Application
{

class TeacherImportCandidateCount final
{
public:
    constexpr TeacherImportCandidateCount() noexcept = default;

    explicit constexpr TeacherImportCandidateCount(
        const std::size_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const TeacherImportCandidateCount&,
        const TeacherImportCandidateCount&
        ) noexcept = default;

private:
    std::size_t m_rawValue = 0;
};

} // namespace ClassMngr::Next::Application
