#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationTextMatchSpan final
{
    std::size_t start = 0;
    std::size_t length = 0;

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
