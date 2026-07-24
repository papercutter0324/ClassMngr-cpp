#pragma once

#include "domain/models/schedule_import.h"

#include <QString>
#include <QStringList>

#include <algorithm>

inline QList<QStringList> scheduleImportAllowedDayPatterns(
    const QString& classGrade,
    const QString& classLevel
    )
{
    const QString grade =
        classGrade.trimmed().toUpper();
    const QString level =
        classLevel.trimmed();
    const bool songs =
        level.compare(
            QStringLiteral("Song's"),
            Qt::CaseInsensitive
            ) == 0;

    const QStringList mondayWednesday{
        QStringLiteral("Monday"),
        QStringLiteral("Wednesday")
    };
    const QStringList mondayFriday{
        QStringLiteral("Monday"),
        QStringLiteral("Friday")
    };
    const QStringList wednesdayFriday{
        QStringLiteral("Wednesday"),
        QStringLiteral("Friday")
    };
    const QStringList tuesdayThursday{
        QStringLiteral("Tuesday"),
        QStringLiteral("Thursday")
    };
    const QStringList mondayWednesdayFriday{
        QStringLiteral("Monday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Friday")
    };

    if (
        grade == QStringLiteral("E4")
        || (
            grade == QStringLiteral("E5")
            && level.compare(
                QStringLiteral("Athena"),
                Qt::CaseInsensitive
                ) != 0
            )
        )
    {
        return {
            mondayWednesday,
            mondayFriday,
            wednesdayFriday,
            tuesdayThursday
        };
    }
    if (
        grade == QStringLiteral("E5")
        && level.compare(
            QStringLiteral("Athena"),
            Qt::CaseInsensitive
            ) == 0
        )
    {
        return {
            mondayWednesdayFriday,
            tuesdayThursday
        };
    }
    if (
        grade == QStringLiteral("E6")
        && songs
        )
    {
        return {
            mondayWednesdayFriday,
            tuesdayThursday
        };
    }
    if (
        (
            grade == QStringLiteral("M1")
            || grade == QStringLiteral("M2")
            || grade == QStringLiteral("M3")
            )
        && songs
        )
    {
        return {
            mondayWednesday,
            mondayFriday,
            tuesdayThursday
        };
    }
    if (
        grade == QStringLiteral("E6")
        || grade == QStringLiteral("M1")
        || grade == QStringLiteral("M2")
        )
    {
        return {
            {QStringLiteral("Monday")},
            {QStringLiteral("Tuesday")},
            {QStringLiteral("Wednesday")},
            {QStringLiteral("Thursday")},
            {QStringLiteral("Friday")}
        };
    }

    return {};
}

inline QString scheduleImportMeetingPatternExpectation(
    const QString& classGrade,
    const QString& classLevel
    )
{
    const QString grade =
        classGrade.trimmed().toUpper();
    const QString level =
        classLevel.trimmed();
    const bool songs =
        level.compare(
            QStringLiteral("Song's"),
            Qt::CaseInsensitive
            ) == 0;

    if (
        grade == QStringLiteral("E4")
        || (
            grade == QStringLiteral("E5")
            && level.compare(
                QStringLiteral("Athena"),
                Qt::CaseInsensitive
                ) != 0
            )
        )
    {
        return QStringLiteral(
            "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, or Tuesday/Thursday."
            );
    }
    if (
        (
            grade == QStringLiteral("E5")
            && level.compare(
                QStringLiteral("Athena"),
                Qt::CaseInsensitive
                ) == 0
            )
        || (
            grade == QStringLiteral("E6")
            && songs
            )
        )
    {
        return QStringLiteral(
            "Expected Monday/Wednesday/Friday or Tuesday/Thursday."
            );
    }
    if (
        (
            grade == QStringLiteral("M1")
            || grade == QStringLiteral("M2")
            || grade == QStringLiteral("M3")
            )
        && songs
        )
    {
        return QStringLiteral(
            "Expected Monday/Wednesday, Monday/Friday, or Tuesday/Thursday."
            );
    }
    if (
        grade == QStringLiteral("E6")
        || grade == QStringLiteral("M1")
        || grade == QStringLiteral("M2")
        )
    {
        return QStringLiteral("Expected one weekday meeting.");
    }
    return QStringLiteral(
        "The imported grade and level do not have a supported meeting-pattern rule."
        );
}

inline QString scheduleImportMeetingPatternError(
    const ScheduleImportClassCandidate& candidate
    )
{
    static const QStringList weekdayOrder{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    const auto patternKey =
        [](QStringList days)
        {
            std::sort(
                days.begin(),
                days.end(),
                [](
                    const QString& left,
                    const QString& right
                    )
                {
                    return weekdayOrder.indexOf(left)
                        < weekdayOrder.indexOf(right);
                }
                );
            return days.join(QLatin1Char('|'));
        };

    QStringList days;
    for (const ClassTime& time : candidate.times)
    {
        if (
            !weekdayOrder.contains(time.day)
            || days.contains(time.day)
            )
        {
            return QStringLiteral(
                "Each imported class must have exactly one meeting per scheduled weekday."
                );
        }
        days.append(time.day);
    }

    const QList<QStringList> allowedPatterns =
        scheduleImportAllowedDayPatterns(
            candidate.classGrade,
            candidate.classLevel
            );
    if (allowedPatterns.isEmpty())
    {
        return {};
    }

    QStringList allowedKeys;
    for (const QStringList& pattern : allowedPatterns)
    {
        allowedKeys.append(patternKey(pattern));
    }

    if (allowedKeys.contains(patternKey(days)))
    {
        return {};
    }

    return QStringLiteral("%1 Detected: %2.")
        .arg(
            scheduleImportMeetingPatternExpectation(
                candidate.classGrade,
                candidate.classLevel
                ),
            days.isEmpty()
                ? QStringLiteral("no meetings")
                : days.join(QStringLiteral(", "))
            );
}
