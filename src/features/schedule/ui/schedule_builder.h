#pragma once

#include "next/application/schedule_builder_source_snapshot.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTime>

enum class ScheduleEntryKind
{
    RegularClass,
    TestingClass
};

struct ScheduleEntry
{
    int classId{-1};
    ScheduleEntryKind kind =
        ScheduleEntryKind::RegularClass;
    QString className;
    QString teacherKr;
    QString teacherEn;
    QString teacherPreferredName;
    QString roomNumber;
    QString classGrade;
    QString classLevel;
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
};

struct ScheduleRow
{
    QString label;
};

struct ScheduleBuildResult
{
    QStringList days;
    QList<ScheduleRow> rows;
    QMap<QString, QMap<QString, QList<ScheduleEntry>>> schedule;
    int scheduleOffset{0};
    bool uses55Endings{false};
};

class ScheduleBuilder
{
public:
    [[nodiscard]] ScheduleBuildResult build(
        const ClassMngr::Next::Application::
            ScheduleBuilderSourceSnapshot& source,
        bool useIntensive,
        const QStringList& visibleDays
        ) const;

private:
    struct ParsedClass
    {
        QString day;
        QTime startTime;
        ScheduleEntry entry;
    };

    QTime parseTime(
        const QString& value
        ) const;

    QList<ScheduleRow> buildRows(
        int startHour,
        int finalHour,
        int offset
        ) const;
};
