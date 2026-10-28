#pragma once

#include "next/application/my_classes_page_read.h"
#include "next/application/positive_decimal_id_validation.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <unordered_set>

namespace ClassMngr::Next::Application
{

class MyClassesPageReadQuery final
{
public:
    explicit MyClassesPageReadQuery(const MyClassesPageReadPort& port) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] MyClassesPageSummaryResult readSummaries() const
    {
        auto result = m_port.readSummaries();
        if (!result)
        {
            return MyClassesPageSummaryResult::failure(result.error());
        }
        if (result.value().size() > kMyClassesPageMaximumClasses)
        {
            return MyClassesPageSummaryResult::failure(validationError(
                "The My Classes summary list exceeds its bounded limit."
                ));
        }

        std::unordered_set<std::string> classIds;
        classIds.reserve(result.value().size());
        for (const MyClassesPageClassSummary& summary : result.value())
        {
            if (!isCanonicalPositiveDecimalId(summary.classId.value())
                || !classIds.insert(summary.classId.value()).second
                || !bounded(summary.className)
                || !bounded(summary.grade)
                || !bounded(summary.level)
                || !bounded(summary.teacherEnglishName)
                || !bounded(summary.teacherKoreanName)
                || !boundedSchedule(summary.regularSchedule)
                || !boundedSchedule(summary.intensiveSchedule)
                || (summary.teacherId
                    && !isCanonicalPositiveDecimalId64(
                        summary.teacherId->value()
                        )))
            {
                return MyClassesPageSummaryResult::failure(validationError(
                    "The My Classes summary list contains an invalid or unbounded record."
                    ));
            }
        }
        return result;
    }

    [[nodiscard]] MyClassesPageSelectedDetailsResult readSelectedDetails(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            return MyClassesPageSelectedDetailsResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "The selected My Classes identifier is invalid.",
                .recoverable = false
            });
        }

        auto result = m_port.readSelectedDetails(classId);
        if (!result)
        {
            return result;
        }
        const MyClassesPageSelectedDetails& details = result.value();
        if (details.classId != classId
            || !isCanonicalPositiveDecimalId(details.classId.value())
            || !bounded(details.className)
            || !bounded(details.grade)
            || !bounded(details.level)
            || !bounded(details.classNotes)
            || !bounded(details.timeFillerActivities)
            || !boundedSchedule(details.regularSchedule)
            || !boundedSchedule(details.intensiveSchedule)
            || (details.studentCount && *details.studentCount < 0)
            || (details.teacherId
                && !isCanonicalPositiveDecimalId64(details.teacherId->value()))
            || (details.teacher
                && !boundedTeacher(*details.teacher)))
        {
            return MyClassesPageSelectedDetailsResult::failure(validationError(
                "The selected My Classes details are invalid or unbounded."
                ));
        }
        return result;
    }

private:
    [[nodiscard]] static bool bounded(const std::u16string& value) noexcept
    {
        return value.size() <= kMyClassesPageMaximumTextLength;
    }

    [[nodiscard]] static bool boundedTeacher(
        const MyClassesPageTeacherDetails& teacher
        ) noexcept
    {
        return bounded(teacher.koreanName)
            && bounded(teacher.englishName)
            && bounded(teacher.preferredRomanization)
            && bounded(teacher.preferredName)
            && bounded(teacher.roomNumber)
            && bounded(teacher.internetType)
            && bounded(teacher.wifiName)
            && bounded(teacher.wifiPassword)
            && bounded(teacher.projectionType)
            && bounded(teacher.zoomId)
            && bounded(teacher.zoomPassword)
            && bounded(teacher.notes);
    }

    [[nodiscard]] static bool boundedSchedule(
        const std::vector<MyClassesPageScheduleEntry>& schedule
        ) noexcept
    {
        return schedule.size() <= kMyClassesPageMaximumScheduleRows
            && std::all_of(schedule.cbegin(), schedule.cend(), [](const auto& entry) {
                return boundedScheduleToken(entry.day)
                    && boundedScheduleToken(entry.startTime)
                    && boundedScheduleToken(entry.endTime);
            });
    }

    [[nodiscard]] static bool boundedScheduleToken(
        const std::u16string& value
        ) noexcept
    {
        return value.size() <= kMyClassesPageMaximumScheduleTokenLength;
    }

    [[nodiscard]] static Domain::OperationError validationError(
        const char* message
        )
    {
        return {
            .code = Domain::ErrorCode::Validation,
            .message = message,
            .recoverable = false
        };
    }

    const MyClassesPageReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
