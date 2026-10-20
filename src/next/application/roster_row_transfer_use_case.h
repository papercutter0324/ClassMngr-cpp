#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/roster_column_projection.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_row_removal.h"
#include "next/application/roster_row_transfer_preparation.h"
#include "next/application/roster_save_preparation.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

class RosterRowTransferSourceIndex final
{
public:
    constexpr RosterRowTransferSourceIndex() noexcept = default;

    explicit constexpr RosterRowTransferSourceIndex(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterRowTransferSourceIndex&,
        const RosterRowTransferSourceIndex&
        ) = default;

private:
    int m_value = -1;
};

struct RosterRowTransferRequest final
{
    Domain::ClassId sourceClassId;
    Domain::ClassId targetClassId;
    RosterSnapshot sourceRoster;
    RosterRowTransferSourceIndex sourceRow;
    std::vector<std::u16string> baseColumnNames;
    bool sourceQuestionableKoreanNameLengthsConfirmed = false;
};

struct RosterRowTransferSaveRequest final
{
    Domain::ClassId sourceClassId;
    Domain::ClassId targetClassId;
    RosterSnapshot sourceRoster;
    RosterSnapshot targetRoster;
};

class RosterRowTransferSavePort
{
public:
    virtual ~RosterRowTransferSavePort() = default;

    // Both snapshots must be saved in one transaction, or neither is saved.
    [[nodiscard]] virtual Domain::Result<void> saveTransfer(
        const RosterRowTransferSaveRequest& request
        ) const = 0;
};

class RosterRowTransferConfirmationPort
{
public:
    virtual ~RosterRowTransferConfirmationPort() = default;

    [[nodiscard]] virtual bool confirmTargetQuestionableKoreanNameLengths(
        const RosterSnapshot& preparedTarget,
        const std::vector<RosterSaveIssue>& issues
        ) const = 0;
};

struct RosterRowTransferSuccess final
{
    RosterRowTransferDestinationIndex destinationRow;
};

using RosterRowTransferResult = std::variant<
    RosterRowTransferSuccess,
    RosterRowRemovalError,
    RosterRowTransferPreparationError,
    Domain::OperationError
    >;

class RosterRowTransferUseCase final
{
public:
    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static RosterRowTransferResult execute(
        const RosterRowTransferRequest& request,
        const RosterReadPort& readPort,
        const RosterRowTransferSavePort& savePort,
        CaseInsensitiveEquals&& caseInsensitiveEquals,
        const RosterRowTransferConfirmationPort* confirmationPort = nullptr
        )
    {
        if (!isCanonicalPositiveDecimalId(request.sourceClassId.value())
            || !isCanonicalPositiveDecimalId(request.targetClassId.value())
            || request.sourceClassId == request.targetClassId)
        {
            return Domain::OperationError{
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Transfer requires distinct canonical positive class IDs.",
                .recoverable = true
            };
        }

        const int sourceRow = request.sourceRow.rawValue();
        auto removed = removeRosterRow(
            request.sourceRoster,
            RosterRowRemovalIndex{sourceRow}
            );
        if (const auto* error = std::get_if<RosterRowRemovalError>(&removed))
            return *error;

        const auto selectedRowIndex = static_cast<std::size_t>(sourceRow);
        if (request.sourceRoster.rows[selectedRowIndex].size()
            > request.sourceRoster.columns.size())
            return validationError("Source");

        auto&& equals = caseInsensitiveEquals;
        const RosterSaveCaseInsensitiveEquals validationEquals =
            [&equals](const std::u16string_view left,
                      const std::u16string_view right)
            {
                return std::invoke(equals, left, right);
            };
        const auto& source = std::get<RosterSnapshot>(removed);
        auto preparedSource = prepareRosterSaveText(
            source, request.sourceQuestionableKoreanNameLengthsConfirmed,
            validationEquals);
        if (preparedSource.hasErrors())
            return validationError("Source");

        const auto loaded = RosterReadUseCase::execute(
            {.classId = request.targetClassId}, readPort);
        if (!loaded)
            return loaded.error();

        RosterSnapshot target = projectTargetRoster(
            decodedTargetSnapshot(loaded.value()), request.baseColumnNames, equals);
        auto prepared = prepareRosterRowTransfer(
            target.columns, target.rows, request.sourceRoster.columns,
            request.sourceRoster.rows[selectedRowIndex],
            equals);
        if (const auto* error =
                std::get_if<RosterRowTransferPreparationError>(&prepared))
            return *error;

        auto& insertion = std::get<RosterRowTransferPreparation>(prepared);
        const RosterRowTransferDestinationIndex destinationRow =
            insertion.destinationRow;
        target.rows[destinationRow.rawValue()] = std::move(insertion.mappedRow);
        auto preparedTarget = prepareRosterSaveText(
            target, true, validationEquals);
        if (preparedTarget.hasErrors())
            return validationError("Target");

        std::vector<RosterSaveIssue> questionableKoreanNameIssues;
        for (const auto& issue : preparedTarget.issues)
        {
            if (issue.code == RosterSaveIssueCode::StudentNameKoreanTooShort
                || issue.code == RosterSaveIssueCode::StudentNameKoreanTooLong)
                questionableKoreanNameIssues.push_back(issue);
        }
        if (!questionableKoreanNameIssues.empty()
            && (!confirmationPort
                || !confirmationPort->confirmTargetQuestionableKoreanNameLengths(
                    preparedTarget.roster, questionableKoreanNameIssues)))
        {
            return Domain::OperationError{
                .code = Domain::ErrorCode::Canceled,
                .message = "Target Korean name lengths were not confirmed.",
                .recoverable = true
            };
        }

        const auto saved = savePort.saveTransfer({
            .sourceClassId = request.sourceClassId,
            .targetClassId = request.targetClassId,
            .sourceRoster = std::move(preparedSource.roster),
            .targetRoster = std::move(preparedTarget.roster)
        });
        if (!saved)
            return saved.error();

        return RosterRowTransferSuccess{destinationRow};
    }

private:
    [[nodiscard]] static Domain::OperationError validationError(
        const std::string_view rosterName)
    {
        return {
            .code = Domain::ErrorCode::Validation,
            .message = std::string(rosterName) + " roster failed validation before transfer.",
            .recoverable = false
        };
    }

    [[nodiscard]] static RosterSnapshot decodedTargetSnapshot(RosterSnapshot snapshot)
    {
        // Preserve the former read-edge QString::fromStdU16String decoding.
        // Source UI text is already logical text and never goes through this path.
        for (auto& column : snapshot.columns)
            column = RosterSavePreparationDetail::snapshotText(column);
        for (auto& row : snapshot.rows)
            for (auto& cell : row)
                cell = RosterSavePreparationDetail::snapshotText(cell);
        return snapshot;
    }

    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static RosterSnapshot projectTargetRoster(
        const RosterSnapshot& stored,
        const std::vector<std::u16string>& baseColumnNames,
        CaseInsensitiveEquals& equals
        )
    {
        const auto projection = RosterColumnProjection::create(
            stored.columns, baseColumnNames, equals);
        RosterSnapshot target;
        for (const auto& column : projection.columns())
        {
            target.columns.push_back(column.name);

            // Widths historically use raw header comparison, unlike cell
            // projection. A normalized custom header may therefore have width 0.
            int width = 0;
            for (std::size_t index = 0; index < stored.columns.size(); ++index)
            {
                if (std::invoke(equals, std::u16string_view(stored.columns[index]),
                                std::u16string_view(column.name))
                    || (std::invoke(equals, std::u16string_view(column.name), u"Fall")
                        && std::invoke(equals,
                            std::u16string_view(stored.columns[index]), u"Autumn")))
                {
                    if (index < stored.columnWidths.size())
                        width = stored.columnWidths[index];
                    break;
                }
            }
            target.columnWidths.push_back(width);
        }

        target.rows.reserve(RosterModeledRowCount);
        for (std::size_t row = 0; row < RosterModeledRowCount; ++row)
        {
            std::vector<std::u16string> values;
            for (const auto& column : projection.columns())
            {
                const std::u16string_view value = row < stored.rows.size()
                    && column.sourceColumnIndex
                    && *column.sourceColumnIndex < stored.rows[row].size()
                    ? std::u16string_view(stored.rows[row][*column.sourceColumnIndex])
                    : std::u16string_view{};
                values.push_back(std::invoke(equals,
                        std::u16string_view(column.name), u"English")
                    ? SpeakingEvaluationValidationDetail::normalizeEnglishName(value)
                    : std::invoke(equals,
                        std::u16string_view(column.name), u"Korean")
                        ? RosterSavePreparationDetail::normalizeKoreanName(value)
                        : simplifyQtWhitespace(value));
            }
            target.rows.push_back(std::move(values));
        }
        return target;
    }
};

} // namespace ClassMngr::Next::Application
