#pragma once

#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include <QList>
#include <QSet>
#include <QStringList>

namespace SubPrepClassInformation
{
struct SourceClass
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
    int studentCount = 0;
};

struct ClassDetails
{
    int classId = -1;
    ClassInfo info;
    int studentCount = 0;
    QString classLabel;
    QString timeText;
};

struct TeacherGroup
{
    Teacher teacher;
    QString displayName;
    QString classListText;
    QList<ClassDetails> classes;
};

struct BuildOptions
{
    QSet<int> visibleClassIds;
    QStringList visibleDays;
    bool useIntensive = false;
};

[[nodiscard]] QString formatMeetingTimes(
    const QList<ClassTime>& times,
    const QStringList& visibleDays
    );

[[nodiscard]] QList<TeacherGroup> build(
    const QList<SourceClass>& sourceClasses,
    const BuildOptions& options
    );
}
