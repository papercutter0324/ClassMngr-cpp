#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/application/roster_snapshot.h"
#include "next/application/roster_save_preparation.h"

#include <charconv>
#include <string>
#include <system_error>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterSaveRequest final
{
    Domain::ClassId classId;
    RosterSnapshot roster;
    bool allowQuestionableKoreanNameLengths = false;

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

    friend bool operator==(
        const PreparedRosterSaveRequest&,
        const PreparedRosterSaveRequest&
        ) = default;
};

class RosterSavePort
{
public:
    virtual ~RosterSavePort() = default;

    // Receives the prepared, valid snapshot from RosterSaveUseCase.
    [[nodiscard]] virtual Domain::Result<void> saveRoster(
        const PreparedRosterSaveRequest& request
        ) const = 0;
};

class RosterSaveUseCase final
{
public:
    [[nodiscard]] static Domain::Result<void> execute(
        const RosterSaveRequest& request,
        const RosterSavePort& port,
        const RosterSaveCaseInsensitiveEquals& caseInsensitiveEquals
        )
    {
        if (!isCanonicalPositiveClassId(request.classId.value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        const auto prepared = prepareRosterSave(
            request.roster, request.allowQuestionableKoreanNameLengths,
            caseInsensitiveEquals);
        if (prepared.hasErrors())
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = validationError(prepared.issues),
                .recoverable = false
            });
        }
        return port.saveRoster({
            .classId = request.classId,
            .roster = prepared.roster,
            .allowQuestionableKoreanNameLengths = request.allowQuestionableKoreanNameLengths
        });
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
            if (issue.row >= 0 && issue.field.find(u'[') == std::u16string::npos)
                result += "row " + std::to_string(issue.row + 1) + ", ";
            if (!issue.field.empty())
                result += utf8(issue.field) + ": ";
            result += issue.code;
        }
        return result;
    }

    [[nodiscard]] static bool isCanonicalPositiveClassId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
