#include "schedule_time_formatter.h"

#include <classmngr/engine/schedule_report.h>

#include <string>

namespace
{
std::string toEngineString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString toQtString(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}
}

namespace ScheduleTimeFormatter
{
QString displayTime(
    const QString& timeLabel,
    bool use24h
    )
{
    return toQtString(
        classmngr::engine::ScheduleReportService::displayTime(
            toEngineString(timeLabel),
            use24h
            )
        );
}

QString rangeLabel(
    const QString& startLabel,
    bool uses55Endings,
    bool use24h
    )
{
    return toQtString(
        classmngr::engine::ScheduleReportService::rangeLabel(
            toEngineString(startLabel),
            uses55Endings,
            use24h
            )
        );
}
}
