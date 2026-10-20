#pragma once

namespace ClassMngr::Next::Application
{

class ScheduleImportCandidateIndex final
{
public:
    constexpr ScheduleImportCandidateIndex() noexcept = default;

    explicit constexpr ScheduleImportCandidateIndex(
        const int rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const ScheduleImportCandidateIndex&,
        const ScheduleImportCandidateIndex&
        ) noexcept = default;

private:
    int m_rawValue = -1;
};

} // namespace ClassMngr::Next::Application
