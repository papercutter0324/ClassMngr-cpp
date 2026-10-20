#pragma once

namespace ClassMngr::Next::Application
{

class ScheduleImportAppliedCount final
{
public:
    constexpr ScheduleImportAppliedCount() noexcept = default;

    explicit constexpr ScheduleImportAppliedCount(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ScheduleImportAppliedCount&,
        const ScheduleImportAppliedCount&
        ) noexcept = default;

private:
    int m_value = 0;
};

} // namespace ClassMngr::Next::Application
