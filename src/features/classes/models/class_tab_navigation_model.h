#pragma once

#include "domain/models/class_info.h"

#include <QList>
#include <QSet>
#include <QString>

namespace ClassTabNavigation
{
inline constexpr int FlatClassThreshold = 6;

enum class Mode
{
    Flat,
    GradeGrouped
};

enum class GroupingPolicy
{
    Adaptive,
    AlwaysGradeGrouped
};

enum class ScheduleSource
{
    Regular,
    Intensive
};

enum class VisibilityScope
{
    AllClasses,
    ActiveSchedule
};

struct DayFilter
{
    QSet<QString> selectedDays;
    ScheduleSource scheduleSource{ScheduleSource::Regular};
    VisibilityScope visibilityScope{VisibilityScope::AllClasses};
};

struct ClassEntry
{
    int classId{-1};
    QString classroomName;
    QString grade;
    QString level;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
    QString teacherEn;
    QString teacherKr;
};

struct ClassTab
{
    int classId{-1};
    QString label;
};

struct GradeGroup
{
    QString grade;
    QString label;
    QList<ClassTab> classes;
};

struct Model
{
    Mode mode{Mode::Flat};
    QList<ClassTab> flatClasses;
    QList<GradeGroup> gradeGroups;
};

Model build(
    const QList<ClassEntry>& entries,
    GroupingPolicy groupingPolicy = GroupingPolicy::Adaptive,
    const DayFilter& dayFilter = {}
    );
}
