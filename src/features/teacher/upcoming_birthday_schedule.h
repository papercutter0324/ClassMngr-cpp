#pragma once

#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/teacher.h"

#include <QDate>
#include <QList>
#include <QString>

enum class UpcomingBirthdayGroup
{
    KoreanTeacher,
    NativeEnglishTeacher,
    GsTeam
};

struct UpcomingBirthday
{
    QDate date;
    QString displayName;
    QString position;
    UpcomingBirthdayGroup group = UpcomingBirthdayGroup::KoreanTeacher;
};

struct UpcomingBirthdaySchedule
{
    QList<UpcomingBirthday> today;
    QList<UpcomingBirthday> thisWeek;
    QList<UpcomingBirthday> nextWeek;

    [[nodiscard]] bool isEmpty() const;

    [[nodiscard]] static UpcomingBirthdaySchedule build(
        const QList<Teacher>& teachers,
        const QList<NativeEnglishTeacher>& nativeEnglishTeachers,
        const QList<GsTeamMember>& gsTeamMembers,
        const QDate& referenceDate
        );
};
