#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Carries the stored skipped-version spelling unchanged. Construction is
// permissive: parsing and comparison policy belong to their callers.
class SkippedUpdateVersionText final
{
public:
    constexpr SkippedUpdateVersionText() = default;

    explicit constexpr SkippedUpdateVersionText(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] constexpr const std::string& value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return m_value.empty();
    }

    friend constexpr bool operator==(
        const SkippedUpdateVersionText&,
        const SkippedUpdateVersionText&
        ) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application
