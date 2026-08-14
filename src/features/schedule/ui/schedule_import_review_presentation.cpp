#include "schedule_import_review_presentation.h"

#include "app/services/feature_services.h"
#include "core/utils/colorutils.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/classes/config/class_info_config.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"
#include "features/schedule/ui/schedule_time_formatter.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTime>

#include <algorithm>
#include <limits>

namespace ScheduleImportReviewPresentation
{
constexpr int RegularPreviewFirstHour = 16;
constexpr int RegularPreviewLastHour = 21;

int timeMinutes(
    const QString& value
    )
{
    QTime time;
    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm")
    };
    for (const QString& format : formats)
    {
        time =
            QTime::fromString(
                value.trimmed(),
                format
                );
        if (time.isValid())
        {
            break;
        }
    }
    return time.isValid()
        ? time.hour() * 60 + time.minute()
        : -1;
}

int weekdayIndex(
    const QString& day
    );

QString weekdayLabel(
    const QString& day
    );

int weekdayIndex(
    const QString& day
    )
{
    const int index =
        ClassInfoConfig::Days.indexOf(day.trimmed());
    return index >= 0
        ? index
        : std::numeric_limits<int>::max();
}

QString weekdayLabel(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QObject::tr("Mon.");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QObject::tr("Tues.");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QObject::tr("Wed.");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QObject::tr("Thurs.");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QObject::tr("Fri.");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QObject::tr("Sat.");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QObject::tr("Sun.");
    }
    return day.trimmed();
}

QString compactTimeDisplay(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    if (!time.isValid())
    {
        return value;
    }

    const QString minutePart =
        time.minute() == 0
            ? QString()
            : QStringLiteral(":%1")
                  .arg(time.minute(), 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1%2%3")
        .arg(
            time.hour() == 0 || time.hour() == 12
                ? 12
                : time.hour() % 12
            )
        .arg(minutePart)
        .arg(time.hour() < 12 ? QStringLiteral("am") : QStringLiteral("pm"));
}

QString reconciliationTimeDisplay(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    if (!time.isValid())
    {
        return value.toHtmlEscaped();
    }

    return QStringLiteral("%1:%2%3")
        .arg(
            time.hour() == 0 || time.hour() == 12
                ? 12
                : time.hour() % 12
            )
        .arg(
            time.minute(),
            2,
            10,
            QLatin1Char('0')
            )
        .arg(
            time.hour() < 12
                ? QStringLiteral("am")
                : QStringLiteral("pm")
            );
}

QString reconciliationMeetingText(
    const ClassTime& time
    )
{
    return QStringLiteral("%1 %2 - %3")
        .arg(
            weekdayLabel(time.day).toHtmlEscaped(),
            reconciliationTimeDisplay(time.startTime),
            reconciliationTimeDisplay(time.endTime)
            );
}

QList<ClassTime> orderedMeetings(
    QList<ClassTime> times
    )
{
    std::stable_sort(
        times.begin(),
        times.end(),
        [](const ClassTime& left, const ClassTime& right)
        {
            const int leftDay = weekdayIndex(left.day);
            const int rightDay = weekdayIndex(right.day);
            if (leftDay != rightDay)
            {
                return leftDay < rightDay;
            }
            return timeMinutes(left.startTime)
                < timeMinutes(right.startTime);
        }
        );
    return times;
}

QString meetingDifferenceText(
    const QList<ClassTime>& existingTimes,
    const QList<ClassTime>& importedTimes
    )
{
    const QList<ClassTime> orderedExisting =
        orderedMeetings(existingTimes);
    const QList<ClassTime> orderedImported =
        orderedMeetings(importedTimes);
    QStringList lines;
    int existingIndex = 0;
    int importedIndex = 0;
    while (
        existingIndex < orderedExisting.size()
        || importedIndex < orderedImported.size()
        )
    {
        const bool hasExisting =
            existingIndex < orderedExisting.size();
        const bool hasImported =
            importedIndex < orderedImported.size();
        const int existingDay =
            hasExisting
                ? weekdayIndex(orderedExisting[existingIndex].day)
                : std::numeric_limits<int>::max();
        const int importedDay =
            hasImported
                ? weekdayIndex(orderedImported[importedIndex].day)
                : std::numeric_limits<int>::max();
        const bool sameDay =
            hasExisting
            && hasImported
            && orderedExisting[existingIndex].day.compare(
                orderedImported[importedIndex].day,
                Qt::CaseInsensitive
                ) == 0;
        const bool existingDayComesFirst =
            hasExisting
            && hasImported
            && !sameDay
            && (
                existingDay < importedDay
                || (
                    existingDay == importedDay
                    && orderedExisting[existingIndex].day.compare(
                        orderedImported[importedIndex].day,
                        Qt::CaseInsensitive
                        ) < 0
                    )
                );
        const bool useExisting =
            hasExisting
            && (
                !hasImported
                || existingDayComesFirst
                );
        const bool useImported =
            hasImported
            && (
                !hasExisting
                || (
                    !sameDay
                    && !existingDayComesFirst
                    )
                );
        const QString existing =
            hasExisting && !useImported
                ? reconciliationMeetingText(orderedExisting[existingIndex])
                : QStringLiteral("—");
        const QString imported =
            hasImported && !useExisting
                ? reconciliationMeetingText(orderedImported[importedIndex])
                : QStringLiteral("—");
        lines.append(
            QStringLiteral("%1 → %2")
                .arg(existing, imported)
            );

        if (hasExisting && !useImported)
        {
            ++existingIndex;
        }
        if (hasImported && !useExisting)
        {
            ++importedIndex;
        }
    }

    return lines.join(QStringLiteral("<br>"));
}

QString compactMeetingText(
    const QList<ClassTime>& times
    )
{
    QList<ClassTime> orderedTimes = times;
    std::sort(
        orderedTimes.begin(),
        orderedTimes.end(),
        [](const ClassTime& left, const ClassTime& right)
        {
            const int leftDay = weekdayIndex(left.day);
            const int rightDay = weekdayIndex(right.day);
            if (leftDay != rightDay)
            {
                return leftDay < rightDay;
            }
            return timeMinutes(left.startTime)
                < timeMinutes(right.startTime);
        }
        );

    QStringList text;
    for (const ClassTime& time : orderedTimes)
    {
        text.append(
            QStringLiteral("%1 %2")
                .arg(
                    weekdayLabel(time.day),
                    compactTimeDisplay(time.startTime)
                    )
            );
    }
    return text.join(QStringLiteral(" / "));
}

QStringList meetingKeys(
    const QList<ClassTime>& times
    )
{
    QStringList keys;
    for (const ClassTime& time : times)
    {
        keys.append(
            QStringLiteral("%1\x1f%2\x1f%3")
                .arg(
                    time.day,
                    time.startTime,
                    time.endTime
                    )
            );
    }
    keys.sort(Qt::CaseInsensitive);
    return keys;
}

bool timesOverlap(
    const ClassTime& left,
    const ClassTime& right
    )
{
    if (left.day != right.day)
    {
        return false;
    }
    const int leftStart = timeMinutes(left.startTime);
    const int leftEnd = timeMinutes(left.endTime);
    const int rightStart = timeMinutes(right.startTime);
    const int rightEnd = timeMinutes(right.endTime);
    return leftStart >= 0
        && rightStart >= 0
        && leftEnd > leftStart
        && rightEnd > rightStart
        && leftStart < rightEnd
        && rightStart < leftEnd;
}

QString importedClassConflictLabel(
    const ScheduleImportClassCandidate& candidate
    )
{
    const QString course =
        QStringLiteral("%1 %2")
            .arg(
                candidate.classGrade,
                candidate.classLevel
                )
            .simplified();
    const QString meetings =
        compactMeetingText(candidate.times);
    return QStringLiteral("%1 — %2 (%3)")
        .arg(
            course,
            candidate.teacherKr.trimmed(),
            meetings.isEmpty()
                ? QObject::tr("time unavailable")
                : meetings
            );
}

QStringList projectedScheduleConflicts(
    const ScheduleImportUserBlock& user
    )
{
    struct Occurrence
    {
        QString label;
        ClassTime time;
    };
    QList<Occurrence> occurrences;
    QStringList conflicts;

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        const QString label =
            QStringLiteral("%1 %2")
                .arg(
                    candidate.classGrade,
                    candidate.classLevel
                    );
        for (const ClassTime& time : candidate.times)
        {
            for (const Occurrence& existing : occurrences)
            {
                if (timesOverlap(time, existing.time))
                {
                    conflicts.append(
                        QObject::tr(
                            "%1 overlaps %2 on %3 (%4 - %5 and %6 - %7)."
                            )
                            .arg(
                                label,
                                existing.label,
                                scheduleImportWeekdayDisplayName(time.day),
                                reconciliationTimeDisplay(
                                    existing.time.startTime
                                    ),
                                reconciliationTimeDisplay(
                                    existing.time.endTime
                                    ),
                                reconciliationTimeDisplay(
                                    time.startTime
                                    ),
                                reconciliationTimeDisplay(
                                    time.endTime
                                    )
                                )
                        );
                }
            }
            occurrences.append({label, time});
        }
    }

    conflicts.removeDuplicates();
    return conflicts;
}

QString classLabel(
    ClassService* classService,
    TeacherService* teacherService,
    int classId,
    ScheduleImportKind kind
    )
{
    const Classroom classroom =
        classService->classroom(classId).value_or(Classroom{});
    const ClassInfo info =
        classService->classInfo(classId);
    const QString course =
        QStringLiteral("%1 %2")
            .arg(
                info.classGrade,
                info.classLevel
                )
            .simplified();

    const QString label =
        !course.isEmpty()
            ? course
            : !classroom.name.trimmed().isEmpty()
                ? classroom.name.trimmed()
                : QObject::tr("Class %1").arg(classId);
    const Teacher teacher =
        teacherService->teacher(info.teacherId).value_or(Teacher{});
    const bool importingIntensive =
        kind == ScheduleImportKind::Intensive;
    const QList<ClassTime>& preferredTimes =
        importingIntensive
            ? info.intensiveTimes
            : info.classTimes;
    const QList<ClassTime>& fallbackTimes =
        importingIntensive
            ? info.classTimes
            : info.intensiveTimes;
    const bool usesPreferredTimes =
        !preferredTimes.isEmpty();
    const QList<ClassTime>& times =
        usesPreferredTimes
            ? preferredTimes
            : fallbackTimes;
    const QString schedule =
        compactMeetingText(times);
    QStringList detailParts;
    if (!teacher.teacherKr.trimmed().isEmpty())
    {
        detailParts.append(teacher.teacherKr.trimmed());
    }
    if (!schedule.isEmpty())
    {
        detailParts.append(schedule);
    }
    const QString details =
        detailParts.join(QLatin1Char(' '));
    const QString scheduleTag =
        schedule.isEmpty()
            ? QString()
            : usesPreferredTimes
                ? importingIntensive
                    ? QStringLiteral(" ") + QObject::tr("[Int]")
                    : QStringLiteral(" ") + QObject::tr("[Reg]")
                : importingIntensive
                    ? QStringLiteral(" ") + QObject::tr("[Reg]")
                    : QStringLiteral(" ") + QObject::tr("[Int]");
    return details.isEmpty()
        ? label
        : QStringLiteral("%1 (%2)%3")
              .arg(label, details, scheduleTag);
}

QString classDifferences(
    ClassService* classService,
    TeacherService* teacherService,
    const ScheduleImportClassCandidate& candidate,
    int targetClassId,
    ScheduleImportKind kind,
    const QString& classColor,
    const QColor& changesColor,
    const QColor& changesHeadingColor
    )
{
    if (!classService || !teacherService || targetClassId <= 0)
    {
        return QObject::tr(
                   "A new class will be created with color %1."
                   )
            .arg(classColor)
            .toHtmlEscaped();
    }

    const auto differenceItem =
        [](const QString& label,
           const QString& existing,
           const QString& imported)
        {
            return QStringLiteral(
                "<li><b>%1:</b> %2 → %3</li>"
                )
                .arg(
                    label.toHtmlEscaped(),
                    existing.toHtmlEscaped(),
                    imported.toHtmlEscaped()
                    );
        };
    const auto meetingDifferenceItem =
        [](const QString& label,
           const QString& differences)
        {
            return QStringLiteral(
                "<li><b>%1:</b><br>%2</li>"
                )
                .arg(
                    label.toHtmlEscaped(),
                    differences
                    );
        };

    const ClassInfo existing =
        classService->classInfo(targetClassId);
    const Teacher existingTeacher =
        teacherService->teacher(existing.teacherId).value_or(Teacher{});
    const QList<ClassTime> existingTimes =
        kind == ScheduleImportKind::Intensive
            ? existing.intensiveTimes
            : existing.classTimes;
    QStringList differences;

    if (existing.classGrade != candidate.classGrade)
    {
        differences.append(
            differenceItem(
                QObject::tr("Grade"),
                existing.classGrade,
                candidate.classGrade
                )
            );
    }
    if (existing.classLevel != candidate.classLevel)
    {
        differences.append(
            differenceItem(
                QObject::tr("Level"),
                existing.classLevel,
                candidate.classLevel
                )
            );
    }
    if (
        TeacherImportNameUtils::hangulOnly(
            existingTeacher.teacherKr
            ) != candidate.teacherKey
        )
    {
        differences.append(
            differenceItem(
                QObject::tr("Teacher"),
                existingTeacher.teacherKr,
                candidate.teacherKr
                )
            );
    }
    if (meetingKeys(existingTimes) != meetingKeys(candidate.times))
    {
        differences.append(
            meetingDifferenceItem(
                QObject::tr("Days"),
                meetingDifferenceText(
                    existingTimes,
                    candidate.times
                    )
                )
            );
    }
    if (
        existing.classColor.compare(
            classColor,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        differences.append(
            differenceItem(
                QObject::tr("Color"),
                existing.classColor,
                classColor
                )
            );
    }

    if (differences.isEmpty())
    {
        return QObject::tr(
                   "No grade, level, teacher, day, or color differences."
                   )
            .toHtmlEscaped();
    }

    return QStringLiteral(
        "<span style=\"color:%1\"><b style=\"color:%2\">%3</b>"
        "<ul style=\"margin-top:2px; margin-bottom:0px;\">%4</ul>"
        "</span>"
        )
        .arg(
            changesColor.name(QColor::HexRgb),
            changesHeadingColor.name(QColor::HexRgb),
            QObject::tr("Changes:").toHtmlEscaped(),
            differences.join(QString())
            );
}

QString teacherLabel(
    const Teacher& teacher
    )
{
    return QObject::tr("%1 — Room %2")
        .arg(
            teacher.teacherKr.trimmed(),
            teacher.roomNumber.trimmed().isEmpty()
                ? QObject::tr("not set")
                : teacher.roomNumber.trimmed()
            );
}

int configuredValueOrder(
    const QStringList& configuredValues,
    const QString& value
    )
{
    for (int index = 0; index < configuredValues.size(); ++index)
    {
        if (
            configuredValues[index].compare(
                value.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            return index;
        }
    }
    return std::numeric_limits<int>::max();
}

bool importedClassLess(
    const ScheduleImportClassCandidate& left,
    const ScheduleImportClassCandidate& right
    )
{
    const int leftGrade =
        configuredValueOrder(
            ClassInfoConfig::Grades,
            left.classGrade
            );
    const int rightGrade =
        configuredValueOrder(
            ClassInfoConfig::Grades,
            right.classGrade
            );
    if (leftGrade != rightGrade)
    {
        return leftGrade < rightGrade;
    }
    if (
        leftGrade == std::numeric_limits<int>::max()
        && left.classGrade.compare(
            right.classGrade,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return left.classGrade.compare(
            right.classGrade,
            Qt::CaseInsensitive
            ) < 0;
    }

    const QString configuredGrade =
        leftGrade == std::numeric_limits<int>::max()
            ? left.classGrade.trimmed()
            : ClassInfoConfig::Grades[leftGrade];
    const QStringList levels =
        ClassInfoConfig::levelsForGrade(
            configuredGrade
            );
    const int leftLevel =
        configuredValueOrder(levels, left.classLevel);
    const int rightLevel =
        configuredValueOrder(levels, right.classLevel);
    if (leftLevel != rightLevel)
    {
        return leftLevel < rightLevel;
    }
    if (
        leftLevel == std::numeric_limits<int>::max()
        && left.classLevel.compare(
            right.classLevel,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return left.classLevel.compare(
            right.classLevel,
            Qt::CaseInsensitive
            ) < 0;
    }
    return false;
}

ScheduleViewModel previewModel(
    const ScheduleImportUserBlock& user,
    bool useIntensive,
    const ScheduleDisplayState& displayState
    )
{
    const QStringList days =
        visibleScheduleDays(
            displayState.showWeekends
            );

    QSet<int> starts;
    QHash<QString, QList<ScheduleEntry>> entries;
    QHash<QString, QString> slotStates;
    bool uses55Endings = false;

    if (useIntensive)
    {
        for (const IntensiveSlotState& state :
             user.intensiveSlotStates)
        {
            const int start =
                timeMinutes(state.startTime);
            if (start < 0)
            {
                continue;
            }
            starts.insert(start);
            slotStates.insert(
                state.day
                    + QLatin1Char('\x1f')
                    + QString::number(start),
                state.state
                );
        }
    }

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        ScheduleEntry entry;
        entry.teacherKr = candidate.teacherKr;
        entry.roomNumber =
            candidate.rooms.isEmpty()
                ? QString()
                : candidate.rooms.first();
        entry.classGrade = candidate.classGrade;
        entry.classLevel = candidate.classLevel;
        entry.classColor =
            candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first();
        entry.fontColor =
            ColorUtils::getContrastingFontColor(
                QColor(entry.classColor)
                );

        for (const ClassTime& time : candidate.times)
        {
            const int start =
                timeMinutes(time.startTime);
            if (start < 0)
            {
                continue;
            }
            starts.insert(start);
            const int end =
                timeMinutes(time.endTime);
            if (end >= 0 && end % 60 == 55)
            {
                uses55Endings = true;
            }
            entries[
                time.day
                + QLatin1Char('\x1f')
                + QString::number(start)
                ].append(entry);
        }
    }

    if (!useIntensive)
    {
        for (int hour = RegularPreviewFirstHour;
             hour <= RegularPreviewLastHour;
             ++hour)
        {
            starts.insert(hour * 60);
        }
    }

    QList<int> sortedStarts =
        starts.values();
    std::sort(
        sortedStarts.begin(),
        sortedStarts.end()
        );

    ScheduleViewModel model;
    model.days = days;
    model.uses55Endings =
        !useIntensive
        && uses55Endings;

    for (int start : sortedStarts)
    {
        ScheduleRowView row;
        const QTime startTime(
            start / 60,
            start % 60
            );
        row.timeLabel =
            startTime.toString(
                QStringLiteral("HH:mm")
                );
        row.timeRangeLabel =
            ScheduleTimeFormatter::rangeLabel(
                row.timeLabel,
                model.uses55Endings,
                displayState.use24HourTime
                );

        for (const QString& day : days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = row.timeLabel;
            cell.entries =
                entries.value(
                    day
                    + QLatin1Char('\x1f')
                    + QString::number(start)
                    );
            cell.defaultSlotState =
                useIntensive
                    ? scheduleEmptySlotState()
                    : scheduleEssaySlotState();
            cell.slotState =
                useIntensive
                    ? slotStates.value(
                        day
                            + QLatin1Char('\x1f')
                            + QString::number(start),
                        scheduleEmptySlotState()
                        )
                    : cell.entries.isEmpty()
                        ? scheduleEssaySlotState()
                        : scheduleEmptySlotState();
            row.maxEntryCount =
                std::max(
                    row.maxEntryCount,
                    static_cast<int>(
                        cell.entries.size()
                        )
                    );
            if (!cell.entries.isEmpty())
            {
                ++model.summary.scheduledBlocks;
            }
            else if (
                cell.slotState
                    == scheduleEssaySlotState()
                )
            {
                ++model.summary.essayBlocks;
                ++model.summary.scheduledBlocks;
            }
            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    if (!useIntensive)
    {
        return model;
    }

    int firstVisibleRow = -1;
    int lastVisibleRow = -1;
    for (int rowIndex = 0;
         rowIndex < model.rows.size();
         ++rowIndex)
    {
        const ScheduleRowView& row = model.rows[rowIndex];
        const bool hasVisibleContent =
            std::any_of(
                row.cells.cbegin(),
                row.cells.cend(),
                [](const ScheduleCellView& cell)
                {
                    return !cell.entries.isEmpty()
                        || cell.slotState != scheduleEmptySlotState();
                }
                );
        if (!hasVisibleContent)
        {
            continue;
        }

        if (firstVisibleRow < 0)
        {
            firstVisibleRow = rowIndex;
        }
        lastVisibleRow = rowIndex;
    }

    model.rows = firstVisibleRow >= 0
        ? model.rows.mid(
            firstVisibleRow,
            lastVisibleRow - firstVisibleRow + 1
            )
        : QList<ScheduleRowView>();

    return model;
}
}
