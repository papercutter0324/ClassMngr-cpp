#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

class RosterSnapshotColumnWidth final
{
public:
    constexpr RosterSnapshotColumnWidth() noexcept = default;

    explicit constexpr RosterSnapshotColumnWidth(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterSnapshotColumnWidth&,
        const RosterSnapshotColumnWidth&
        ) noexcept = default;

private:
    int m_value = 0;
};

struct RosterSnapshot final
{
    std::vector<std::u16string> columns;
    std::vector<RosterSnapshotColumnWidth> columnWidths;
    std::vector<std::vector<std::u16string>> rows;

    friend bool operator==(
        const RosterSnapshot&,
        const RosterSnapshot&
        ) = default;
};

} // namespace ClassMngr::Next::Application
