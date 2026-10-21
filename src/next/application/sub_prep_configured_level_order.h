#pragma once

#include "next/application/class_summary_projection.h"

#include <cstdint>

namespace ClassMngr::Next::Application
{

class SubPrepConfiguredLevelOrder final
{
public:
    constexpr SubPrepConfiguredLevelOrder() noexcept = default;

    explicit constexpr SubPrepConfiguredLevelOrder(
        const std::int32_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::int32_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const SubPrepConfiguredLevelOrder&,
        const SubPrepConfiguredLevelOrder&
        ) noexcept = default;

private:
    std::int32_t m_rawValue = kClassSummaryUnknownNavigationOrder;
};

} // namespace ClassMngr::Next::Application
