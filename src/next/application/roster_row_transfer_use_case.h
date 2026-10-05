#pragma once

#include "next/application/roster_column_projection.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_row_removal.h"
#include "next/application/roster_row_transfer_preparation.h"
#include "next/application/roster_save_preparation.h"

#include <charconv>
#include <functional>
#include <string_view>
#include <variant>

namespace ClassMngr::Next::Application
{

struct RosterRowTransferRequest final
{
    Domain::ClassId sourceClassId;
    Domain::ClassId targetClassId;
    RosterSnapshot sourceRoster;
    int sourceRow = -1;
    std::vector<std::u16string> baseColumnNames;
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

struct RosterRowTransferSuccess final
{
    std::size_t destinationRow = 0;
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
        CaseInsensitiveEquals&& caseInsensitiveEquals
        )
    {
        if (!canonicalId(request.sourceClassId)
            || !canonicalId(request.targetClassId)
            || request.sourceClassId == request.targetClassId)
        {
            return Domain::OperationError{
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Transfer requires distinct canonical positive class IDs.",
                .recoverable = true
            };
        }

        auto removed = removeRosterRow(request.sourceRoster, request.sourceRow);
        if (const auto* error = std::get_if<RosterRowRemovalError>(&removed))
            return *error;

        const auto loaded = RosterReadUseCase::execute(
            {.classId = request.targetClassId}, readPort);
        if (!loaded)
            return loaded.error();

        auto&& equals = caseInsensitiveEquals;
        RosterSnapshot target = projectTargetRoster(
            decodedTargetSnapshot(loaded.value()), request.baseColumnNames, equals);
        auto prepared = prepareRosterRowTransfer(
            target.columns, target.rows, request.sourceRoster.columns,
            request.sourceRoster.rows[static_cast<std::size_t>(request.sourceRow)],
            equals);
        if (const auto* error =
                std::get_if<RosterRowTransferPreparationError>(&prepared))
            return *error;

        auto& insertion = std::get<RosterRowTransferPreparation>(prepared);
        const std::size_t destinationRow = insertion.destinationRow;
        target.rows[destinationRow] = std::move(insertion.mappedRow);
        const auto saved = savePort.saveTransfer({
            .sourceClassId = request.sourceClassId,
            .targetClassId = request.targetClassId,
            .sourceRoster = std::move(std::get<RosterSnapshot>(removed)),
            .targetRoster = std::move(target)
        });
        if (!saved)
            return saved.error();

        return RosterRowTransferSuccess{destinationRow};
    }

private:
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

    [[nodiscard]] static bool canonicalId(const Domain::ClassId& id)
    {
        const auto& value = id.value();
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        return error == std::errc{} && end == value.data() + value.size()
            && parsed > 0 && std::to_string(parsed) == value;
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
