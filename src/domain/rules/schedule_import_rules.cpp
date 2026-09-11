#include "schedule_import_rules.h"

#include "classmngr/engine/schedule_import_rules.h"

#include <QByteArray>
#include <QObject>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace
{
using EngineClassInfo = classmngr::engine::ClassInfo;
using EngineClassTime = classmngr::engine::ClassTime;
using EngineCandidate = classmngr::engine::ScheduleImportClassCandidate;
using EngineKind = classmngr::engine::ScheduleImportKind;
using EngineRules = classmngr::engine::ScheduleImportRules;
using EngineExpectation =
    classmngr::engine::ScheduleImportMeetingPatternExpectation;
using EnginePatternStatus =
    classmngr::engine::ScheduleImportMeetingPatternStatus;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

EngineKind toEngineKind(
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? EngineKind::Intensive
        : EngineKind::Normal;
}

EngineClassTime toEngineClassTime(
    const ClassTime& source
    )
{
    EngineClassTime result;
    result.day = toUtf8(source.day);
    result.startTime = toUtf8(source.startTime);
    result.endTime = toUtf8(source.endTime);
    return result;
}

std::vector<EngineClassTime> toEngineClassTimes(
    const QList<ClassTime>& source
    )
{
    std::vector<EngineClassTime> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const ClassTime& value : source)
    {
        result.push_back(toEngineClassTime(value));
    }
    return result;
}

QList<ClassTime> fromEngineClassTimes(
    const std::vector<EngineClassTime>& source
    )
{
    QList<ClassTime> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineClassTime& value : source)
    {
        ClassTime converted;
        converted.day = fromUtf8(value.day);
        converted.startTime = fromUtf8(value.startTime);
        converted.endTime = fromUtf8(value.endTime);
        result.append(std::move(converted));
    }
    return result;
}

EngineClassInfo toEngineClassInfo(
    const ClassInfo& source
    )
{
    EngineClassInfo result;
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.classTimes = toEngineClassTimes(source.classTimes);
    result.intensiveTimes = toEngineClassTimes(source.intensiveTimes);
    return result;
}

EngineCandidate toEngineCandidate(
    const ScheduleImportClassCandidate& source
    )
{
    EngineCandidate result;
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.times = toEngineClassTimes(source.times);
    return result;
}

QStringList fromEngineStrings(
    const std::vector<std::string>& source
    )
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const std::string& value : source)
    {
        result.append(fromUtf8(value));
    }
    return result;
}

QList<QStringList> fromEnginePatterns(
    const std::vector<std::vector<std::string>>& source
    )
{
    QList<QStringList> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const std::vector<std::string>& pattern : source)
    {
        result.append(fromEngineStrings(pattern));
    }
    return result;
}

QString localizedMeetingPatternExpectation(
    EngineExpectation expectation
    )
{
    switch (expectation)
    {
    case EngineExpectation::WeekdayPairs:
        return QObject::tr(
            "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, or Tuesday/Thursday."
            );
    case EngineExpectation::WeekdayTripleOrTuesdayThursday:
        return QObject::tr(
            "Expected Monday/Wednesday/Friday or Tuesday/Thursday."
            );
    case EngineExpectation::OneWeekday:
        return QObject::tr("Expected one weekday meeting.");
    case EngineExpectation::Unsupported:
        return QObject::tr(
            "The imported grade and level do not have a supported meeting-pattern rule."
            );
    }

    return {};
}
} // namespace

int scheduleImportDayGroup(
    const QList<ClassTime>& times
    )
{
    return EngineRules::dayGroup(toEngineClassTimes(times));
}

bool scheduleImportDaysAreCompatible(
    const QList<ClassTime>& importedTimes,
    const QList<ClassTime>& existingTimes
    )
{
    return EngineRules::daysAreCompatible(
        toEngineClassTimes(importedTimes),
        toEngineClassTimes(existingTimes)
        );
}

QStringList scheduleImportMeetingDays(
    const QList<ClassTime>& times
    )
{
    return fromEngineStrings(
        EngineRules::meetingDays(toEngineClassTimes(times))
        );
}

bool scheduleImportMeetingDaysMatch(
    const QList<ClassTime>& importedTimes,
    const QList<ClassTime>& existingTimes
    )
{
    return EngineRules::meetingDaysMatch(
        toEngineClassTimes(importedTimes),
        toEngineClassTimes(existingTimes)
        );
}

QList<ClassTime> scheduleImportTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    const EngineClassInfo converted = toEngineClassInfo(info);
    return fromEngineClassTimes(
        EngineRules::timesForKind(converted, toEngineKind(kind))
        );
}

QList<ClassTime> scheduleImportTargetTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    const EngineClassInfo converted = toEngineClassInfo(info);
    return fromEngineClassTimes(
        EngineRules::targetTimesForKind(converted, toEngineKind(kind))
        );
}

bool scheduleImportClassOptionIsEligible(
    const ScheduleImportClassCandidate& candidate,
    const ClassInfo& existing,
    ScheduleImportKind kind
    )
{
    return EngineRules::classOptionIsEligible(
        toEngineCandidate(candidate),
        toEngineClassInfo(existing),
        toEngineKind(kind)
        );
}

QList<QStringList> scheduleImportAllowedDayPatterns(
    const QString& classGrade,
    const QString& classLevel
    )
{
    return fromEnginePatterns(
        EngineRules::allowedDayPatterns(
            toUtf8(classGrade),
            toUtf8(classLevel)
            )
        );
}

QString scheduleImportMeetingPatternExpectation(
    const QString& classGrade,
    const QString& classLevel
    )
{
    return localizedMeetingPatternExpectation(
        EngineRules::meetingPatternExpectation(
            toUtf8(classGrade),
            toUtf8(classLevel)
            )
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
    const classmngr::engine::ScheduleImportMeetingPatternResult validation =
        EngineRules::validateMeetingPattern(toEngineCandidate(candidate));
    if (validation.status == EnginePatternStatus::Valid)
    {
        return {};
    }
    if (validation.status == EnginePatternStatus::InvalidWeekdayOrDuplicate)
    {
        return QObject::tr(
            "Each imported class must have exactly one meeting per scheduled weekday."
            );
    }

    QStringList displayDays;
    displayDays.reserve(
        static_cast<qsizetype>(validation.meetingDays.size())
        );
    for (const std::string& day : validation.meetingDays)
    {
        displayDays.append(scheduleImportWeekdayDisplayName(fromUtf8(day)));
    }

    return QObject::tr("%1 Detected: %2.")
        .arg(
            localizedMeetingPatternExpectation(validation.expectation),
            displayDays.isEmpty()
                ? QObject::tr("no meetings")
                : displayDays.join(QStringLiteral(", "))
            );
}
