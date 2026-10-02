#include "next/application/roster_row_transfer_preparation.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <variant>
#include <vector>

using ClassMngr::Next::Application::RosterRowTransferPreparation;
using ClassMngr::Next::Application::RosterRowTransferPreparationError;
using ClassMngr::Next::Application::RosterRowTransferPreparationRejection;
using ClassMngr::Next::Application::RosterRowTransferPreparationResult;
using ClassMngr::Next::Application::prepareRosterRowTransfer;

namespace
{

bool asciiCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        char16_t lhs = left[index];
        char16_t rhs = right[index];
        if (lhs >= u'A' && lhs <= u'Z')
        {
            lhs = static_cast<char16_t>(lhs - u'A' + u'a');
        }
        if (rhs >= u'A' && rhs <= u'Z')
        {
            rhs = static_cast<char16_t>(rhs - u'A' + u'a');
        }
        if (lhs != rhs)
        {
            return false;
        }
    }

    return true;
}

RosterRowTransferPreparationResult prepare(
    const std::vector<std::u16string>& targetColumns,
    const std::vector<std::vector<std::u16string>>& targetRows,
    const std::vector<std::u16string>& sourceColumns,
    const std::vector<std::u16string>& sourceRow
    )
{
    return prepareRosterRowTransfer(
        targetColumns,
        targetRows,
        sourceColumns,
        sourceRow,
        asciiCaseInsensitiveEquals
        );
}

bool mapsAndNormalizesWithoutChangingTheTarget()
{
    const std::vector<std::u16string> targetColumns{
        u"English",
        u"Korean",
        u"Fall",
        u"Review Queue",
        u"Missing"
    };
    const std::vector<std::vector<std::u16string>> targetRows{
        {u"Other", u"\uAE40\uC9C0\uD6C8", u"Existing", u"Old note", u""},
        {u"", u"", u"", u"", u""},
        {u"", u"", u"", u"", u""}
    };
    const auto originalColumns = targetColumns;
    const auto originalRows = targetRows;

    const auto result = prepare(
        targetColumns,
        targetRows,
        {
            u" Review\t\u00a0 Queue ",
            u"Autumn",
            u"korean",
            u"english",
            u"Source only"
        },
        {
            u"  A note\t from   home  ",
            u"  Spring   result ",
            u" \uAE40 \uBBFC\uC9C0 (a) ",
            u"  jOHN\t smith  ",
            u"unused value"
        }
        );

    const auto* prepared = std::get_if<RosterRowTransferPreparation>(&result);
    return prepared
        && prepared->destinationRow == 1
        && prepared->mappedRow == std::vector<std::u16string>{
            u"John Smith",
            u"\uAE40\uBBFC\uC9C0(A)",
            u"Spring result",
            u"A note from home",
            u""
        }
        && targetColumns == originalColumns
        && targetRows == originalRows;
}

bool headerMatchingKeepsLegacyTargetCellClassification()
{
    const auto result = prepare(
        {u" English ", u"Fall", u"Review"},
        {{u"", u"", u""}},
        {u"english", u"Autumn", u" review "},
        {u" aMy ", u" \tRound 2 ", u" one\t note "}
        );
    const auto* prepared = std::get_if<RosterRowTransferPreparation>(&result);
    return prepared
        && prepared->mappedRow == std::vector<std::u16string>{
            u"aMy",
            u"Round 2",
            u"one note"
        };
}

RosterRowTransferPreparationRejection rejection(
    const RosterRowTransferPreparationResult& result
    )
{
    const auto* error = std::get_if<RosterRowTransferPreparationError>(&result);
    return error
        ? error->rejection
        : RosterRowTransferPreparationRejection::DuplicateStudentNamePair;
}

bool rejectionOrderAndTypedFailuresArePreserved()
{
    using Rejection = RosterRowTransferPreparationRejection;
    const std::vector<std::u16string> columns{u"English", u"Korean"};
    const std::vector<std::vector<std::u16string>> fullRows{
        {u"Amy", u"\uAE40\uBBFC\uC9C0"}
    };

    if (rejection(prepare(
            columns,
            fullRows,
            columns,
            {u" \t", u"\u00a0"}
            )) != Rejection::SourceRowHasNoData)
    {
        return false;
    }

    if (rejection(prepare(
            columns,
            fullRows,
            columns,
            {u"Amy", u"\uAE40\uBBFC\uC9C0"}
            )) != Rejection::TargetRosterIsFull)
    {
        return false;
    }

    if (rejection(prepare(
            columns,
            {fullRows.front(), {u"", u""}},
            columns,
            {u"Amy", u"\uAE40\uBBFC\uC9C0"}
            )) != Rejection::DuplicateStudentNamePair)
    {
        return false;
    }

    if (rejection(prepare({}, {}, {u"Extra"}, {u"not mapped"}))
        != Rejection::SourceRowHasNoData)
    {
        return false;
    }

    return rejection(prepare(columns, {}, columns, {u"Amy", u"\uAE40\uBBFC\uC9C0"}))
        == Rejection::TargetRosterIsFull;
}

bool incompletePairsRemainAllowedAndFirstEmptySlotWins()
{
    const auto result = prepare(
        {u"English", u"Korean", u"Review"},
        {
            {u"Amy", u"\uAE40\uBBFC\uC9C0", u"occupied"},
            {u" \t", u"\u00a0", u""},
            {u"", u"", u""}
        },
        {u"English", u"Korean", u"Review"},
        {u"Amy", u"", u"note"}
        );
    const auto* prepared = std::get_if<RosterRowTransferPreparation>(&result);
    return prepared
        && prepared->destinationRow == 1
        && prepared->mappedRow
            == std::vector<std::u16string>{u"Amy", u"", u"note"};
}

bool legacyNamePairDelimiterCollisionStillRejects()
{
    const auto result = prepare(
        {u"English", u"Korean"},
        {
            {u"A\u001fB", u"C"},
            {u"", u""}
        },
        {u"English", u"Korean"},
        {u"A", u"B\u001fC"}
        );
    return rejection(result)
        == RosterRowTransferPreparationRejection::DuplicateStudentNamePair;
}

} // namespace

int main()
{
    if (!mapsAndNormalizesWithoutChangingTheTarget())
    {
        std::fprintf(stderr, "Transfer preparation lost mapping or normalization behavior.\n");
        return EXIT_FAILURE;
    }
    if (!headerMatchingKeepsLegacyTargetCellClassification())
    {
        std::fprintf(stderr, "Transfer preparation changed normalized header or cell behavior.\n");
        return EXIT_FAILURE;
    }
    if (!rejectionOrderAndTypedFailuresArePreserved())
    {
        std::fprintf(stderr, "Transfer preparation rejection order or typed result changed.\n");
        return EXIT_FAILURE;
    }
    if (!incompletePairsRemainAllowedAndFirstEmptySlotWins())
    {
        std::fprintf(stderr, "Transfer preparation rejected an incomplete pair or chose the wrong row.\n");
        return EXIT_FAILURE;
    }
    if (!legacyNamePairDelimiterCollisionStillRejects())
    {
        std::fprintf(stderr, "Transfer preparation changed legacy name-pair key behavior.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
