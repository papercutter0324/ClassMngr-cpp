#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/application/roster_snapshot.h"
#include "next/application/roster_save_preparation.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterSaveRequest final
{
    Domain::ClassId classId;
    RosterSnapshot roster;
    bool allowQuestionableKoreanNameLengths = false;
    std::vector<std::u16string> removedCustomColumns;

    friend bool operator==(
        const RosterSaveRequest&,
        const RosterSaveRequest&
        ) = default;
};

// Logical UTF-16 values after roster preparation. This payload must not be
// decoded again as raw UTF-16: a literal leading BOM is part of the text.
struct PreparedRosterSaveRequest final
{
    Domain::ClassId classId;
    RosterSnapshot roster;
    bool allowQuestionableKoreanNameLengths = false;
    std::vector<std::u16string> removedCustomColumns;

    friend bool operator==(
        const PreparedRosterSaveRequest&,
        const PreparedRosterSaveRequest&
        ) = default;
};

enum class RosterSaveDiagnosticStage
{
    BeforeSnapshot,
    AfterSnapshot,
    AfterPreparation,
    BeforeRowSourceSetup,
    AfterRowSourceSetup,
    BeforeRowWiseRepositoryWrite,
    AfterRowWiseRepositoryWrite
};

enum class RosterSaveDiagnosticOutcome
{
    Observed,
    Success,
    Failure
};

using RosterSaveDiagnosticObserver = std::function<void(
    RosterSaveDiagnosticStage,
    RosterSaveDiagnosticOutcome)>;

inline void reportRosterSaveDiagnostic(
    const RosterSaveDiagnosticObserver& observer,
    const RosterSaveDiagnosticStage stage,
    const RosterSaveDiagnosticOutcome outcome
    ) noexcept
{
    if (!observer)
    {
        return;
    }
    try
    {
        observer(stage, outcome);
    }
    catch (...)
    {
        // Diagnostic callbacks must not change roster save behavior.
    }
}

class RosterSavePort
{
public:
    virtual ~RosterSavePort() = default;

    // Receives the prepared visible window from RosterSaveUseCase.
    // removedCustomColumns records explicit user-confirmed custom removals.
    [[nodiscard]] virtual Domain::Result<void> saveRoster(
        const PreparedRosterSaveRequest& request
        ) const = 0;

    // Existing ports can ignore diagnostics. Platform ports that perform
    // legacy conversion and repository I/O can override this boundary.
    [[nodiscard]] virtual Domain::Result<void> saveRosterWithDiagnostics(
        const PreparedRosterSaveRequest& request,
        const RosterSaveDiagnosticObserver& observer
        ) const
    {
        static_cast<void>(observer);
        return saveRoster(request);
    }
};

class RosterSaveUseCase final
{
public:
    [[nodiscard]] static Domain::Result<void> execute(
        RosterSaveRequest request,
        const RosterSavePort& port,
        const RosterSaveCaseInsensitiveEquals& caseInsensitiveEquals,
        const RosterSaveDiagnosticObserver& diagnosticObserver = {}
        )
    {
        if (!isCanonicalPositiveDecimalId(request.classId.value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        auto prepared = prepareRosterSave(
            std::move(request.roster), request.allowQuestionableKoreanNameLengths,
            caseInsensitiveEquals);
        if (diagnosticObserver)
        {
            reportRosterSaveDiagnostic(
                diagnosticObserver,
                RosterSaveDiagnosticStage::AfterPreparation,
                prepared.hasErrors()
                    ? RosterSaveDiagnosticOutcome::Failure
                    : RosterSaveDiagnosticOutcome::Success);
        }
        if (prepared.hasErrors())
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Validation,
                .message = validationError(prepared.issues),
                .recoverable = false
            });
        }
        const PreparedRosterSaveRequest preparedRequest{
            .classId = request.classId,
            .roster = std::move(prepared.roster),
            .allowQuestionableKoreanNameLengths = request.allowQuestionableKoreanNameLengths,
            .removedCustomColumns = std::move(request.removedCustomColumns)
        };
        return diagnosticObserver
            ? port.saveRosterWithDiagnostics(preparedRequest, diagnosticObserver)
            : port.saveRoster(preparedRequest);
    }

private:
    // Matches Qt 6.12 QString::toUtf8(), omitting unpaired surrogates.
    [[nodiscard]] static std::string utf8(std::u16string_view value)
    {
        std::string result;
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            std::uint32_t code = value[index];
            if (code >= 0xd800 && code <= 0xdbff && index + 1 < value.size()
                && value[index + 1] >= 0xdc00 && value[index + 1] <= 0xdfff)
            {
                code = 0x10000 + ((code - 0xd800) << 10) + (value[++index] - 0xdc00);
            }
            else if (code >= 0xd800 && code <= 0xdfff)
                continue;
            if (code < 0x80)
                result.push_back(static_cast<char>(code));
            else if (code < 0x800)
            {
                result.push_back(static_cast<char>(0xc0 | (code >> 6)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
            else if (code < 0x10000)
            {
                result.push_back(static_cast<char>(0xe0 | (code >> 12)));
                result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
            else
            {
                result.push_back(static_cast<char>(0xf0 | (code >> 18)));
                result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
                result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
        }
        return result;
    }

    [[nodiscard]] static std::string validationError(const std::vector<RosterSaveIssue>& issues)
    {
        std::string result = "Roster validation failed: ";
        bool first = true;
        for (const auto& issue : issues)
        {
            if (issue.severity != RosterSaveIssueSeverity::Error)
                continue;
            if (!first)
                result += "; ";
            first = false;
            if (issue.row.rawValue() >= 0
                && issue.field.find(u'[') == std::u16string::npos)
            {
                result += "row "
                    + std::to_string(issue.row.rawValue() + 1)
                    + ", ";
            }
            if (!issue.field.empty())
                result += utf8(issue.field) + ": ";
            result += std::string(rosterSaveIssueCodeKey(issue.code));
        }
        return result;
    }

};

} // namespace ClassMngr::Next::Application
