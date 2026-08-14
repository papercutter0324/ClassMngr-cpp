#pragma once

#include "core/result.h"
#include "domain/models/class_info.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTime>

class ClassService;

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
    explicit ScheduleBuilder(
        ClassService* classService
        );

    [[nodiscard]] Result<ScheduleBuildResult> build(
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

private:
    ClassService* m_classService = nullptr;
};
