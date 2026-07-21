#pragma once

#include "domain/models/class_info.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTime>

class DataService;

struct ScheduleEntry
{
    int classId{-1};
    QString teacherKr;
    QString teacherEn;
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
        DataService* dataService
        );

    ScheduleBuildResult build(
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
    DataService* m_dataService = nullptr;
};
