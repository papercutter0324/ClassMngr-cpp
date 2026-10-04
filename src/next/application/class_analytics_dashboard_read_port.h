#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ClassAnalyticsEvaluation : std::size_t
{
    Winter,
    SpeechContest,
    Summer,
    Fall
};

inline constexpr std::array<std::u16string_view, 4>
    kClassAnalyticsEvaluationNames{
        u"Winter", u"Speech Contest", u"Summer", u"Fall"
    };

[[nodiscard]] constexpr std::u16string_view classAnalyticsEvaluationName(
    ClassAnalyticsEvaluation evaluation
    ) noexcept
{
    const std::size_t index = static_cast<std::size_t>(evaluation);
    return index < kClassAnalyticsEvaluationNames.size()
        ? kClassAnalyticsEvaluationNames[index]
        : std::u16string_view{};
}

struct ClassAnalyticsRosterNames final
{
    // Matches Roster::rows.size(), including blank and sparse rows, without
    // copying unrelated roster columns or their cell values.
    std::size_t rowCount = 0;
    bool hasEnglishColumn = false;
    bool hasKoreanColumn = false;
    std::vector<std::u16string> englishNames;
    std::vector<std::u16string> koreanNames;
};

struct ClassAnalyticsEvaluationRow final
{
    std::u16string englishName;
    std::u16string koreanName;
    std::array<std::u16string, 6> scores;
};

using ClassAnalyticsEvaluationRows =
    std::vector<ClassAnalyticsEvaluationRow>;

using ClassAnalyticsEvaluationBatch =
    std::array<ClassAnalyticsEvaluationRows,
               kClassAnalyticsEvaluationNames.size()>;

class ClassAnalyticsDashboardReadPort
{
public:
    virtual ~ClassAnalyticsDashboardReadPort() = default;

    [[nodiscard]] virtual Domain::Result<ClassAnalyticsRosterNames>
    readRosterNames(const Domain::ClassId& classId) const = 0;

    // Missing evaluations and evaluations without stored rows are successful
    // empty entries at their canonical position in the batch.
    [[nodiscard]] virtual Domain::Result<ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(
        const Domain::ClassId& classId
        ) const = 0;
};

// This seam keeps QString/StudentNameUtils/QLocale out of Application while
// preserving the legacy name identity, trimming, and ranking tie behavior.
class ClassAnalyticsNameSemanticsPort
{
public:
    virtual ~ClassAnalyticsNameSemanticsPort() = default;

    [[nodiscard]] virtual std::u16string trimmed(
        std::u16string_view value
        ) const = 0;

    [[nodiscard]] virtual std::u16string normalizedEnglishIdentity(
        std::u16string_view value
        ) const = 0;

    [[nodiscard]] virtual std::u16string baseKoreanName(
        std::u16string_view value
        ) const = 0;

    [[nodiscard]] virtual bool equalsCaseInsensitive(
        std::u16string_view left,
        std::u16string_view right
        ) const = 0;

    [[nodiscard]] virtual int compareEnglishNames(
        std::u16string_view left,
        std::u16string_view right
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
