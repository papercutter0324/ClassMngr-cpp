#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{

class SpeakingEvaluationTextMatchStart final
{
public:
    constexpr SpeakingEvaluationTextMatchStart() noexcept = default;

    explicit constexpr SpeakingEvaluationTextMatchStart(
        const std::size_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const SpeakingEvaluationTextMatchStart&,
        const SpeakingEvaluationTextMatchStart&
        ) noexcept = default;

private:
    std::size_t m_value = 0;
};

class SpeakingEvaluationTextMatchLength final
{
public:
    constexpr SpeakingEvaluationTextMatchLength() noexcept = default;

    explicit constexpr SpeakingEvaluationTextMatchLength(
        const std::size_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const SpeakingEvaluationTextMatchLength&,
        const SpeakingEvaluationTextMatchLength&
        ) noexcept = default;

private:
    std::size_t m_value = 0;
};

// Both values count UTF-16 code units, matching the port's text contract.
struct SpeakingEvaluationTextMatchSpan final
{
    SpeakingEvaluationTextMatchStart start;
    SpeakingEvaluationTextMatchLength length;

    friend bool operator==(
        const SpeakingEvaluationTextMatchSpan&,
        const SpeakingEvaluationTextMatchSpan&
        ) = default;
};

// Supplies platform-specific literal, case-insensitive matching over UTF-16
// code units. Returned spans are ordered by start and non-overlapping for the
// requested literal.
class SpeakingEvaluationTextMatchPort
{
public:
    virtual ~SpeakingEvaluationTextMatchPort() = default;

    [[nodiscard]] virtual std::vector<SpeakingEvaluationTextMatchSpan>
    findLiteralCaseInsensitiveMatches(
        std::u16string_view text,
        std::u16string_view literal
        ) const = 0;

    [[nodiscard]] virtual bool equalsCaseInsensitive(
        std::u16string_view left,
        std::u16string_view right
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
