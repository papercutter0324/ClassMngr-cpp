#pragma once

#include "classmngr/engine/gs_team_member.h"
#include "classmngr/engine/native_english_teacher.h"
#include "classmngr/engine/teacher.h"

#include <string>
#include <optional>
#include <vector>

namespace classmngr::engine
{

struct TeacherImportPlan
{
    std::string templateId;
    std::string sourceDate;
    std::vector<Teacher> koreanTeachers;
    std::vector<NativeEnglishTeacher> nativeEnglishTeachers;
    std::vector<GsTeamMember> gsTeamMembers;
};

enum class TeacherImportDateComparison
{
    NoPreviousDate,
    Newer,
    Equal,
    Older
};

struct TeacherImportDateDecision
{
    TeacherImportDateComparison comparison =
        TeacherImportDateComparison::NoPreviousDate;
    std::optional<std::string> previousDate;
};

struct TeacherImportCounts
{
    int created = 0;
    int updated = 0;
    int unchanged = 0;

    [[nodiscard]] int total() const noexcept
    {
        return created + updated + unchanged;
    }
};

struct TeacherImportSummary
{
    TeacherImportCounts koreanTeachers;
    TeacherImportCounts nativeEnglishTeachers;
    TeacherImportCounts gsTeamMembers;
};

} // namespace classmngr::engine
