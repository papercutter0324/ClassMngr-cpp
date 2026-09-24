#include "schedule_import_rules.h"

#include "next/domain/course.h"

#include <QObject>

#include <algorithm>
#include <optional>

namespace
{
using ClassMngr::Next::Domain::Course;
using ClassMngr::Next::Domain::CourseGradeBand;
using ClassMngr::Next::Domain::CourseLevelCategory;
using ClassMngr::Next::Domain::Weekday;

std::optional<Course::WeeklyMeetingDayRule> weeklyMeetingDayRuleForNames(
    const QString& classGrade,
    const QString& classLevel
    )
{
    const QString grade = classGrade.trimmed().toUpper();
    const QString level = classLevel.trimmed();

    CourseGradeBand gradeBand = CourseGradeBand::Other;
    if (grade == QStringLiteral("E4"))
    {
        gradeBand = CourseGradeBand::E4;
    }
    else if (grade == QStringLiteral("E5"))
    {
        gradeBand = CourseGradeBand::E5;
    }
    else if (grade == QStringLiteral("E6"))
    {
        gradeBand = CourseGradeBand::E6;
    }
    else if (grade == QStringLiteral("M1"))
    {
        gradeBand = CourseGradeBand::M1;
    }
    else if (grade == QStringLiteral("M2"))
    {
        gradeBand = CourseGradeBand::M2;
    }
    else if (grade == QStringLiteral("M3"))
    {
        gradeBand = CourseGradeBand::M3;
    }

    CourseLevelCategory levelCategory = CourseLevelCategory::Standard;
    if (
        level.compare(
            QStringLiteral("Athena"),
            Qt::CaseInsensitive
            ) == 0
        )
    {
        levelCategory = CourseLevelCategory::Athena;
    }
    else if (
        level.compare(
            QStringLiteral("Song's"),
            Qt::CaseInsensitive
            ) == 0
        )
    {
        levelCategory = CourseLevelCategory::Songs;
    }

    return Course::weeklyMeetingDayRuleFor(gradeBand, levelCategory);
}

QString scheduleImportWeekdayName(Weekday weekday)
{
    switch (weekday)
    {
    case Weekday::Monday:
        return QStringLiteral("Monday");
    case Weekday::Tuesday:
        return QStringLiteral("Tuesday");
    case Weekday::Wednesday:
        return QStringLiteral("Wednesday");
    case Weekday::Thursday:
        return QStringLiteral("Thursday");
    case Weekday::Friday:
        return QStringLiteral("Friday");
    case Weekday::Saturday:
        return QStringLiteral("Saturday");
    case Weekday::Sunday:
        return QStringLiteral("Sunday");
    }
    return {};
}

std::optional<Weekday> scheduleImportWeekday(const QString& day)
{
    if (day == QStringLiteral("Monday"))
    {
        return Weekday::Monday;
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return Weekday::Tuesday;
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return Weekday::Wednesday;
    }
    if (day == QStringLiteral("Thursday"))
    {
        return Weekday::Thursday;
    }
    if (day == QStringLiteral("Friday"))
    {
        return Weekday::Friday;
    }
    return std::nullopt;
}
} // namespace

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
    const auto rule = weeklyMeetingDayRuleForNames(classGrade, classLevel);
    if (!rule.has_value())
    {
        return {};
    }

    QList<QStringList> result;
    for (
        const Course::WeeklyMeetingDayPattern& pattern :
        rule->allowedPatterns()
        )
    {
        QStringList days;
        for (const Weekday weekday : pattern)
        {
            days.append(scheduleImportWeekdayName(weekday));
        }
        result.append(days);
    }
    return result;
}

QString scheduleImportMeetingPatternExpectation(
    const QString& classGrade,
    const QString& classLevel
    )
{
    const auto rule = weeklyMeetingDayRuleForNames(classGrade, classLevel);
    if (!rule.has_value())
    {
        return QObject::tr(
            "The imported grade and level do not have a supported meeting-pattern rule."
            );
    }

    switch (rule->kind())
    {
    case Course::WeeklyMeetingDayRuleKind::PairedWeekdays:
        return QObject::tr(
            "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, or Tuesday/Thursday."
            );
    case Course::WeeklyMeetingDayRuleKind::ThreeDayOrTuesdayThursday:
        return QObject::tr(
            "Expected Monday/Wednesday/Friday or Tuesday/Thursday."
            );
    case Course::WeeklyMeetingDayRuleKind::SingleWeekday:
        return QObject::tr("Expected one weekday meeting.");
    }
    return {};
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
    ClassMngr::Next::Domain::Course::WeeklyMeetingDayPattern days;
    QStringList displayDays;
    for (const ClassTime& time : candidate.times)
    {
        const std::optional<Weekday> weekday =
            scheduleImportWeekday(time.day);
        if (
            !weekday.has_value()
            || std::find(days.begin(), days.end(), *weekday) != days.end()
            )
        {
            return QObject::tr(
                "Each imported class must have exactly one meeting per scheduled weekday."
                );
        }
        days.push_back(*weekday);
        displayDays.append(scheduleImportWeekdayDisplayName(time.day));
    }

    const auto rule =
        weeklyMeetingDayRuleForNames(
            candidate.classGrade,
            candidate.classLevel
            );
    if (!rule.has_value())
    {
        return {};
    }

    if (rule->allows(days))
    {
        return {};
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
