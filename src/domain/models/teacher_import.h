#pragma once

#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/teacher.h"

#include <QDate>
#include <QList>
#include <QString>

struct KoreanTeacherImportCandidate
{
    Teacher teacher;
    bool selectedByDefault = false;
};

struct KoreanTeacherImportGroup
{
    QString level;
    QList<KoreanTeacherImportCandidate> candidates;
};

struct TeacherImportPreview
{
    QString templateId;
    QString templateName;
    QDate sourceDate;
    QList<KoreanTeacherImportGroup> koreanGroups;
    QList<NativeEnglishTeacher> nativeEnglishTeachers;
    QList<GsTeamMember> gsTeamMembers;
};

enum class TeacherImportSelectionMode
{
    All,
    Selected,
    None
};

struct TeacherImportGroupSelection
{
    QString level;
    TeacherImportSelectionMode mode = TeacherImportSelectionMode::All;
    QList<int> selectedCandidateIndexes;
};

struct TeacherImportPlan
{
    QString templateId;
    QDate sourceDate;
    QList<Teacher> koreanTeachers;
    QList<NativeEnglishTeacher> nativeEnglishTeachers;
    QList<GsTeamMember> gsTeamMembers;
};

struct TeacherImportCounts
{
    int created = 0;
    int updated = 0;
    int unchanged = 0;

    [[nodiscard]] int total() const
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
