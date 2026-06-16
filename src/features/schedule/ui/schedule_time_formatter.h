#pragma once

#include <QString>

namespace ScheduleTimeFormatter
{
QString displayTime(
    const QString& timeLabel,
    bool use24h
    );

QString rangeLabel(
    const QString& startLabel,
    bool uses55Endings,
    bool use24h
    );
}
