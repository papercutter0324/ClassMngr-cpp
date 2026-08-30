#pragma once

#include "classmngr/engine/academic_calendar.h"
#include "classmngr/engine/gs_team_member.h"
#include "classmngr/engine/native_english_teacher.h"
#include "classmngr/engine/teacher.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

enum class UpcomingBirthdayGroup
{
    KoreanTeacher,
    NativeEnglishTeacher,
    GsTeam
};

struct UpcomingBirthday
{
    CalendarDate date{};
    std::string displayName;
    std::string position;
    UpcomingBirthdayGroup group = UpcomingBirthdayGroup::KoreanTeacher;
};

struct UpcomingBirthdaySchedule
{
    std::vector<UpcomingBirthday> today;
    std::vector<UpcomingBirthday> thisWeek;
    std::vector<UpcomingBirthday> nextWeek;

    [[nodiscard]] bool isEmpty() const;

    [[nodiscard]] static UpcomingBirthdaySchedule build(
        const std::vector<Teacher>& teachers,
        const std::vector<NativeEnglishTeacher>& nativeEnglishTeachers,
        const std::vector<GsTeamMember>& gsTeamMembers,
        const CalendarDate& referenceDate
        );
};

} // namespace classmngr::engine
