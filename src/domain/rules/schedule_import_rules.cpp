#include "schedule_import_rules.h"

#include <QObject>

#include <algorithm>
#include <utility>

int scheduleImportDayGroup(
    const QList<ClassTime>& times
    )
{
    int group = 0;
    for (const ClassTime& time : times)
    {
        const QString day =
            time.day.trimmed().toCaseFolded();
        const int dayGroup =
            day == QStringLiteral("monday")
                || day == QStringLiteral("wednesday")
                || day == QStringLiteral("friday")
                ? 1
                : day == QStringLiteral("tuesday")
                    || day == QStringLiteral("thursday")
                    ? 2
                    : 0;
        if (dayGroup == 0 || (group != 0 && group != dayGroup))
        {
            return 0;
        }
        group = dayGroup;
    }
    return group;
}

bool scheduleImportDaysAreCompatible(
    const QList<ClassTime>& importedTimes,
    const QList<ClassTime>& existingTimes
    )
{
    const int importedGroup =
        scheduleImportDayGroup(importedTimes);
    return importedGroup != 0
        && importedGroup == scheduleImportDayGroup(existingTimes);
}

QStringList scheduleImportMeetingDays(
    const QList<ClassTime>& times
    )
{
    QStringList days;
    for (const ClassTime& time : times)
    {
        const QString day =
            time.day.trimmed().toCaseFolded();
        if (!days.contains(day))
        {
            days.append(day);
        }
    }
    days.sort(Qt::CaseInsensitive);
    return days;
}

bool scheduleImportMeetingDaysMatch(
    const QList<ClassTime>& importedTimes,
    const QList<ClassTime>& existingTimes
    )
{
    return scheduleImportDaysAreCompatible(
        importedTimes,
        existingTimes
        )
        && scheduleImportMeetingDays(importedTimes)
            == scheduleImportMeetingDays(existingTimes);
}

QList<ClassTime> scheduleImportTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    const QList<ClassTime>& preferred =
        kind == ScheduleImportKind::Intensive
            ? info.intensiveTimes
            : info.classTimes;
    if (!preferred.isEmpty())
    {
        return preferred;
    }
    return kind == ScheduleImportKind::Intensive
        ? info.classTimes
        : info.intensiveTimes;
}

QList<ClassTime> scheduleImportTargetTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? info.intensiveTimes
        : info.classTimes;
}

bool scheduleImportClassOptionIsEligible(
    const ScheduleImportClassCandidate& candidate,
    const ClassInfo& existing,
    ScheduleImportKind kind
    )
{
    return candidate.classGrade.simplified().compare(
        existing.classGrade.simplified(),
        Qt::CaseInsensitive
        ) == 0
        && candidate.classLevel.simplified().compare(
            existing.classLevel.simplified(),
            Qt::CaseInsensitive
            ) == 0
        && (
            scheduleImportTimesForKind(existing, kind).isEmpty()
            || scheduleImportDaysAreCompatible(
                candidate.times,
                scheduleImportTimesForKind(existing, kind)
                )
            );
}

QList<QStringList> scheduleImportAllowedDayPatterns(
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
            wednesdayFriday,
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

QString scheduleImportMeetingPatternExpectation(
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
        return QObject::tr(
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
        return QObject::tr(
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
        return QObject::tr(
            "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, or Tuesday/Thursday."
            );
    }
    if (
        grade == QStringLiteral("E6")
        || grade == QStringLiteral("M1")
        || grade == QStringLiteral("M2")
        )
    {
        return QObject::tr("Expected one weekday meeting.");
    }
    return QObject::tr(
        "The imported grade and level do not have a supported meeting-pattern rule."
        );
}

QString scheduleImportWeekdayDisplayName(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QObject::tr("Monday");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QObject::tr("Tuesday");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QObject::tr("Wednesday");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QObject::tr("Thursday");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QObject::tr("Friday");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QObject::tr("Saturday");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QObject::tr("Sunday");
    }
    return day;
}

QString scheduleImportMeetingPatternError(
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
            return QObject::tr(
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

    QStringList displayDays;
    displayDays.reserve(days.size());
    for (const QString& day : std::as_const(days))
    {
        displayDays.append(
            scheduleImportWeekdayDisplayName(day)
            );
    }

    return QObject::tr("%1 Detected: %2.")
        .arg(
            scheduleImportMeetingPatternExpectation(
                candidate.classGrade,
                candidate.classLevel
                ),
            displayDays.isEmpty()
                ? QObject::tr("no meetings")
                : displayDays.join(QStringLiteral(", "))
            );
}
