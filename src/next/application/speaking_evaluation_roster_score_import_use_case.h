#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/application/speaking_evaluation_query.h"
#include "next/domain/speaking_evaluation_grade.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationRosterScore final
{
    std::u16string englishName;
    std::u16string koreanName;
    std::u16string finalGrade;

    friend bool operator==(
        const SpeakingEvaluationRosterScore&,
        const SpeakingEvaluationRosterScore&
        ) = default;
};

using SpeakingEvaluationRosterScoreImportResult =
    Domain::Result<std::vector<SpeakingEvaluationRosterScore>>;

class SpeakingEvaluationRosterScoreImportUseCase final
{
public:
    [[nodiscard]] static SpeakingEvaluationRosterScoreImportResult execute(
        const SpeakingEvaluationReadQuery& query,
        const SpeakingEvaluationReadPort& port
        )
    {
        const SpeakingEvaluationReadResult source =
            SpeakingEvaluationQuery::execute(query, port);
        if (!source)
        {
            return SpeakingEvaluationRosterScoreImportResult::failure(
                source.error()
                );
        }

        return SpeakingEvaluationRosterScoreImportResult::success(
            scoresFromSnapshot(source.value())
            );
    }

    [[nodiscard]] static std::vector<SpeakingEvaluationRosterScore>
    scoresFromSnapshot(const SpeakingEvaluationReadSnapshot& source)
    {
        std::vector<SpeakingEvaluationRosterScore> scores;
        scores.reserve(source.rows.size());
        for (const std::vector<std::u16string>& row : source.rows)
        {
            if (row.size() < RequiredStoredColumnCount)
            {
                continue;
            }

            std::u16string englishName = trimQtWhitespace(
                row[EnglishNameColumn]
                );
            std::u16string koreanName = trimQtWhitespace(
                row[KoreanNameColumn]
                );
            if (englishName.empty() || koreanName.empty())
            {
                continue;
            }

            Domain::SpeakingEvaluationComponentScores componentScores{};
            for (std::size_t index = 0; index < ScoreColumns.size(); ++index)
            {
                componentScores[index] = gradeFromCell(
                    row[ScoreColumns[index]]
                    );
            }

            std::u16string finalGrade = u"N/A";
            const auto overallGrade =
                Domain::calculateOverallSpeakingEvaluationGrade(
                    componentScores
                    );
            if (overallGrade)
            {
                const std::string_view label =
                    Domain::speakingEvaluationGradeLabel(*overallGrade);
                finalGrade.assign(label.begin(), label.end());
            }

            scores.push_back({
                .englishName = std::move(englishName),
                .koreanName = std::move(koreanName),
                .finalGrade = std::move(finalGrade)
            });
        }

        return scores;
    }

private:
    // Raw query rows retain the legacy 11-column SpeakingEval layout. Rows
    // shorter than that complete shape have always been ignored by import.
    static constexpr std::size_t RequiredStoredColumnCount = 11;
    static constexpr std::size_t EnglishNameColumn = 1;
    static constexpr std::size_t KoreanNameColumn = 2;
    static constexpr std::array<std::size_t, 6> ScoreColumns{
        3, 4, 5, 6, 7, 8
    };

    [[nodiscard]] static std::optional<Domain::SpeakingEvaluationGrade>
    gradeFromCell(std::u16string_view cell)
    {
        const std::u16string trimmed = trimQtWhitespace(cell);
        if (trimmed.size() > 2)
        {
            return std::nullopt;
        }

        char latin1[2]{};
        for (std::size_t index = 0; index < trimmed.size(); ++index)
        {
            const char16_t value = trimmed[index];
            latin1[index] = value <= 0x00ff
                ? static_cast<char>(value)
                : '?';
        }

        return Domain::speakingEvaluationGradeFromLabel(
            std::string_view(latin1, trimmed.size())
            );
    }
};

} // namespace ClassMngr::Next::Application
