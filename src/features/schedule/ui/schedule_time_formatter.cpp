#include "schedule_time_formatter.h"

#include "domain/rules/schedule_value_parser.h"

#include <QTime>

namespace ScheduleTimeFormatter
{
QString displayTime(
    const QString& timeLabel,
    bool use24h
    )
{
    const auto parsedTime =
        ScheduleValueParser::parseTime(timeLabel);

    if (!parsedTime || parsedTime->text != timeLabel)
    {
        return timeLabel;
    }

    const QTime time = parsedTime->value;

    if (use24h)
    {
        return time.toString(
            QStringLiteral("HH:mm")
            );
    }

    return time.toString(
        QStringLiteral("h:mm AP")
        );
}

QString rangeLabel(
    const QString& startLabel,
    bool uses55Endings,
    bool use24h
    )
{
    const auto parsedStart =
        ScheduleValueParser::parseTime(startLabel);

    if (!parsedStart || parsedStart->text != startLabel)
    {
        return startLabel;
    }

    const QTime start = parsedStart->value;

    const QTime end =
        start.addSecs(
            (uses55Endings ? 55 : 50) * 60
            );

    const QString startDisplay =
        displayTime(
            start.toString(QStringLiteral("HH:mm")),
            use24h
            );

    const QString endDisplay =
        displayTime(
            end.toString(QStringLiteral("HH:mm")),
            use24h
            );

    if (use24h)
    {
        return QStringLiteral("%1 - %2")
            .arg(startDisplay)
            .arg(endDisplay);
    }

    const QString startAmpm =
        startDisplay.endsWith(QStringLiteral("AM"))
            ? QStringLiteral("AM")
            : QStringLiteral("PM");

    const QString endAmpm =
        endDisplay.endsWith(QStringLiteral("AM"))
            ? QStringLiteral("AM")
            : QStringLiteral("PM");

    QString startClean =
        startDisplay;

    startClean.remove(QStringLiteral(" AM"));
    startClean.remove(QStringLiteral(" PM"));

    QString endClean =
        endDisplay;

    endClean.remove(QStringLiteral(" AM"));
    endClean.remove(QStringLiteral(" PM"));

    if (startAmpm == endAmpm)
    {
        return QStringLiteral("%1 -\n%2 %3")
            .arg(startClean)
            .arg(endClean)
            .arg(endAmpm);
    }

    return QStringLiteral("%1\n- %2")
        .arg(startDisplay)
        .arg(endDisplay);
}
}
