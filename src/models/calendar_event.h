#pragma once

#include <QDate>
#include <QString>
#include <QTime>

struct CalendarEvent
{
    int id = -1;
    QString title;
    QDate startDate;
    QTime startTime;
    QDate endDate;
    QTime endTime;
};
