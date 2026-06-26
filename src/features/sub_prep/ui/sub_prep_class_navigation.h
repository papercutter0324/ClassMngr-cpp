#pragma once

#include "domain/models/class_info.h"

#include <QList>
#include <QString>

namespace SubPrepClassNavigation
{
inline constexpr int FlatClassThreshold = 6;

enum class Mode
{
    Flat,
    GradeGrouped
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
    const QList<ClassEntry>& entries
    );
}
