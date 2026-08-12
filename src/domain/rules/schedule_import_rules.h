#pragma once

#include "domain/models/schedule_import.h"

#include <QString>
#include <QStringList>

int scheduleImportDayGroup(const QList<ClassTime>& times);
bool scheduleImportDaysAreCompatible(
    const QList<ClassTime>& importedTimes,
    const QList<ClassTime>& existingTimes
    );
QStringList scheduleImportMeetingDays(const QList<ClassTime>& times);
bool scheduleImportMeetingDaysMatch(
    const QList<ClassTime>& importedTimes,
    const QList<ClassTime>& existingTimes
    );
QList<ClassTime> scheduleImportTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    );
QList<ClassTime> scheduleImportTargetTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    );
bool scheduleImportClassOptionIsEligible(
    const ScheduleImportClassCandidate& candidate,
    const ClassInfo& existing,
    ScheduleImportKind kind
    );
QList<QStringList> scheduleImportAllowedDayPatterns(
    const QString& classGrade,
    const QString& classLevel
    );
QString scheduleImportMeetingPatternExpectation(
    const QString& classGrade,
    const QString& classLevel
    );
QString scheduleImportWeekdayDisplayName(const QString& day);
QString scheduleImportMeetingPatternError(
    const ScheduleImportClassCandidate& candidate
    );
