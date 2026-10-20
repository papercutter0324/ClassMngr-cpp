#pragma once

#include <cstddef>
#include <cstdint>

namespace ClassMngr::Next::Application
{

class TeacherImportRequestedCandidateIndex final
{
public:
    constexpr TeacherImportRequestedCandidateIndex() noexcept = default;

    explicit constexpr TeacherImportRequestedCandidateIndex(
        const std::int64_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::int64_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const TeacherImportRequestedCandidateIndex&,
        const TeacherImportRequestedCandidateIndex&
        ) noexcept = default;

private:
    std::int64_t m_rawValue = 0;
};

class TeacherImportResolvedCandidateIndex final
{
public:
    constexpr TeacherImportResolvedCandidateIndex() noexcept = default;

    explicit constexpr TeacherImportResolvedCandidateIndex(
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
        const TeacherImportResolvedCandidateIndex&,
        const TeacherImportResolvedCandidateIndex&
        ) noexcept = default;

private:
    std::size_t m_rawValue = 0;
};

} // namespace ClassMngr::Next::Application
