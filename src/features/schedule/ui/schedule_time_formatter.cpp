#include "schedule_time_formatter.h"

#include <QTime>

namespace ScheduleTimeFormatter
{
QString displayTime(
    const QString& timeLabel,
    bool use24h
    )
{
    const QTime time =
        QTime::fromString(
            timeLabel,
            QStringLiteral("HH:mm")
            );

    if (!time.isValid())
    {
        return timeLabel;
    }

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
    const QTime start =
        QTime::fromString(
            startLabel,
            QStringLiteral("HH:mm")
            );

    if (!start.isValid())
    {
        return startLabel;
    }

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
