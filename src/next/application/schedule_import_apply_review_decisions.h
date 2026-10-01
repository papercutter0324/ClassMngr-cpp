#pragma once

#include "next/application/schedule_import_review_decisions.h"

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Application
{

// Convert a UTF-16 application value to UTF-8 without depending on Qt or the
// legacy application layer. Valid surrogate pairs are combined into one code
// point before encoding.
[[nodiscard]] inline std::string scheduleImportApplyUtf8(
    const std::u16string& text
    )
{
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        char32_t value = text[i];
        if (value >= 0xD800 && value <= 0xDBFF && i + 1 < text.size()
            && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
        {
            value = 0x10000 + ((value - 0xD800) << 10)
                + (text[++i] - 0xDC00);
        }
        if (value < 0x80)
        {
            result.push_back(static_cast<char>(value));
        }
        else if (value < 0x800)
        {
            result.push_back(static_cast<char>(0xC0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
        else if (value < 0x10000)
        {
            result.push_back(static_cast<char>(0xE0 | (value >> 12)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
        else
        {
            result.push_back(static_cast<char>(0xF0 | (value >> 18)));
            result.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
    }
    return result;
}

// This structural template deliberately avoids including the apply use case
// header. It accepts ScheduleImportApplyRequest and keeps the projection
// available to both the use case and UI adapter without a header cycle.
template <typename ApplyRequest>
[[nodiscard]] inline ScheduleImportReviewDecisionRequest
projectScheduleImportApplyReviewDecisions(const ApplyRequest& request)
{
    ScheduleImportReviewDecisionRequest result;
    result.candidates.reserve(request.candidates.size());
    for (const auto& candidate : request.candidates)
    {
        ScheduleImportReviewDecisionCandidate item;
        item.teacherKey = scheduleImportApplyUtf8(candidate.teacherKey);
        item.importedRooms.reserve(candidate.rooms.size());
        for (const auto& room : candidate.rooms)
        {
            if (!room.empty())
            {
                item.importedRooms.push_back(scheduleImportApplyUtf8(room));
            }
        }
        result.candidates.push_back(std::move(item));
    }

    result.teachers.reserve(request.teachers.size());
    for (const auto& teacher : request.teachers)
    {
        result.teachers.push_back({
            scheduleImportApplyUtf8(teacher.teacherKey),
            teacher.action,
            scheduleImportApplyUtf8(teacher.selectedRoom)
        });
    }

    result.classes.reserve(request.classes.size());
    for (const auto& classroom : request.classes)
    {
        result.classes.push_back({
            classroom.candidateIndex,
            classroom.action,
            classroom.targetClassId
        });
    }
    return result;
}

} // namespace ClassMngr::Next::Application
