#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_row_availability.h"
#include "next/application/speaking_evaluation_validation.h"
#include "next/domain/student_name_pair.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class RosterRowTransferPreparationRejection
{
    SourceRowHasNoData,
    TargetRosterIsFull,
    DuplicateStudentNamePair
};

struct RosterRowTransferPreparationError final
{
    RosterRowTransferPreparationRejection rejection;

    friend bool operator==(
        const RosterRowTransferPreparationError&,
        const RosterRowTransferPreparationError&
        ) = default;
};

struct RosterRowTransferPreparation final
{
    std::size_t destinationRow = 0;
    std::vector<std::u16string> mappedRow;

    friend bool operator==(
        const RosterRowTransferPreparation&,
        const RosterRowTransferPreparation&
        ) = default;
};

using RosterRowTransferPreparationResult = std::variant<
    RosterRowTransferPreparation,
    RosterRowTransferPreparationError
    >;

namespace RosterRowTransferPreparationDetail
{

template <typename CaseInsensitiveEquals>
[[nodiscard]] inline std::size_t findColumn(
    const std::vector<std::u16string>& columns,
    const std::u16string_view name,
    CaseInsensitiveEquals& caseInsensitiveEquals
    )
{
    const std::u16string normalizedName = normalizeRosterCustomColumnName(
        name,
        caseInsensitiveEquals
        );
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        const std::u16string normalizedColumn =
            normalizeRosterCustomColumnName(
                columns[index],
                caseInsensitiveEquals
                );
        if (std::invoke(
                caseInsensitiveEquals,
                std::u16string_view(normalizedColumn),
                std::u16string_view(normalizedName)
                ))
        {
            return index;
        }
    }

    return columns.size();
}

template <typename CaseInsensitiveEquals>
[[nodiscard]] inline std::vector<std::u16string> mapRow(
    const std::vector<std::u16string>& targetColumns,
    const std::vector<std::u16string>& sourceColumns,
    const std::vector<std::u16string>& sourceRow,
    CaseInsensitiveEquals& caseInsensitiveEquals
    )
{
    std::vector<std::u16string> mappedRow(targetColumns.size());
    for (std::size_t targetIndex = 0;
         targetIndex < targetColumns.size();
         ++targetIndex)
    {
        const std::u16string normalizedTarget = normalizeRosterCustomColumnName(
            targetColumns[targetIndex],
            caseInsensitiveEquals
            );
        std::size_t sourceIndex = sourceColumns.size();
        for (std::size_t candidateIndex = 0;
             candidateIndex < sourceColumns.size();
             ++candidateIndex)
        {
            const std::u16string normalizedSource =
                normalizeRosterCustomColumnName(
                    sourceColumns[candidateIndex],
                    caseInsensitiveEquals
                    );
            if (std::invoke(
                    caseInsensitiveEquals,
                    std::u16string_view(normalizedSource),
                    std::u16string_view(normalizedTarget)
                    ))
            {
                sourceIndex = candidateIndex;
                break;
            }
        }

        if (sourceIndex >= sourceRow.size())
        {
            continue;
        }

        const std::u16string_view targetName = targetColumns[targetIndex];
        if (std::invoke(caseInsensitiveEquals, targetName, u"English"))
        {
            mappedRow[targetIndex] =
                SpeakingEvaluationValidationDetail::normalizeEnglishName(
                    sourceRow[sourceIndex]
                    );
        }
        else if (std::invoke(caseInsensitiveEquals, targetName, u"Korean"))
        {
            mappedRow[targetIndex] =
                SpeakingEvaluationValidationDetail::normalizeKoreanName(
                    sourceRow[sourceIndex]
                    );
        }
        else
        {
            mappedRow[targetIndex] = simplifyQtWhitespace(sourceRow[sourceIndex]);
        }
    }

    return mappedRow;
}

[[nodiscard]] inline std::u16string legacyNamePairKey(
    const Domain::StudentNamePair& namePair
    )
{
    std::u16string key;
    key.reserve(
        namePair.englishName().size()
        + 1
        + namePair.koreanName().size()
        );
    key.append(namePair.englishName());
    key.push_back(0x001f);
    key.append(namePair.koreanName());
    return key;
}

[[nodiscard]] inline std::optional<Domain::StudentNamePair> completeNamePair(
    const std::vector<std::u16string>& row,
    const std::size_t englishColumn,
    const std::size_t koreanColumn
    )
{
    if (englishColumn >= row.size() || koreanColumn >= row.size())
    {
        return std::nullopt;
    }

    const std::u16string trimmedEnglish = trimQtWhitespace(row[englishColumn]);
    const std::u16string trimmedKorean = trimQtWhitespace(row[koreanColumn]);
    return Domain::StudentNamePair::fromNames(trimmedEnglish, trimmedKorean);
}

} // namespace RosterRowTransferPreparationDetail

// Maps and validates one source row without changing or cloning the target
// roster. The caller supplies Qt-compatible case-insensitive comparison.
template <typename CaseInsensitiveEquals>
[[nodiscard]] inline RosterRowTransferPreparationResult
prepareRosterRowTransfer(
    const std::vector<std::u16string>& targetColumns,
    const std::vector<std::vector<std::u16string>>& targetRows,
    const std::vector<std::u16string>& sourceColumns,
    const std::vector<std::u16string>& sourceRow,
    CaseInsensitiveEquals&& caseInsensitiveEquals
    )
{
    auto&& equals = caseInsensitiveEquals;
    std::vector<std::u16string> mappedRow =
        RosterRowTransferPreparationDetail::mapRow(
            targetColumns,
            sourceColumns,
            sourceRow,
            equals
            );

    using Rejection = RosterRowTransferPreparationRejection;
    if (!rosterRowHasData(mappedRow))
    {
        return RosterRowTransferPreparationError{
            .rejection = Rejection::SourceRowHasNoData
        };
    }

    const std::size_t destinationRow =
        firstEmptyRosterRow(targetRows);
    if (destinationRow == targetRows.size())
    {
        return RosterRowTransferPreparationError{
            .rejection = Rejection::TargetRosterIsFull
        };
    }

    const std::size_t englishColumn =
        RosterRowTransferPreparationDetail::findColumn(
            targetColumns,
            u"English",
            equals
            );
    const std::size_t koreanColumn =
        RosterRowTransferPreparationDetail::findColumn(
            targetColumns,
            u"Korean",
            equals
            );
    if (englishColumn < targetColumns.size()
        && koreanColumn < targetColumns.size())
    {
        const auto candidatePair =
            RosterRowTransferPreparationDetail::completeNamePair(
                mappedRow,
                englishColumn,
                koreanColumn
                );
        if (candidatePair)
        {
            const std::u16string candidateKey =
                RosterRowTransferPreparationDetail::legacyNamePairKey(
                    *candidatePair
                    );
            for (const std::vector<std::u16string>& existingRow : targetRows)
            {
                if (!rosterRowHasData(existingRow))
                {
                    continue;
                }

                const auto existingPair =
                    RosterRowTransferPreparationDetail::completeNamePair(
                        existingRow,
                        englishColumn,
                        koreanColumn
                        );
                if (existingPair
                    && RosterRowTransferPreparationDetail::legacyNamePairKey(
                           *existingPair
                           ) == candidateKey)
                {
                    return RosterRowTransferPreparationError{
                        .rejection = Rejection::DuplicateStudentNamePair
                    };
                }
            }
        }
    }

    return RosterRowTransferPreparation{
        .destinationRow = destinationRow,
        .mappedRow = std::move(mappedRow)
    };
}

} // namespace ClassMngr::Next::Application
