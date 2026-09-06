#include "schedule_workbook_parser.h"

#include "domain/rules/schedule_import_rules.h"
#include "features/calendar/calendar_workbook_reader.h"
#include "features/classes/config/class_info_config.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QTime>

#include <algorithm>
#include <functional>
#include <limits>

namespace
{
struct RawTimeRange
{
    int startHour = -1;
    int startMinute = -1;
    int endHour = -1;
    int endMinute = -1;

    [[nodiscard]] bool valid() const
    {
        return startHour >= 1
            && startHour <= 12
            && startMinute >= 0
            && startMinute <= 59
            && endHour >= 1
            && endHour <= 12
            && endMinute >= 0
            && endMinute <= 59;
    }
};

struct TimetableRow
{
    int row = 0;
    RawTimeRange raw;
    int startMinutes = -1;
    int endMinutes = -1;
};

struct ResolvedTimeRange
{
    int startMinutes = -1;
    int endMinutes = -1;

    [[nodiscard]] bool valid() const
    {
        return startMinutes >= 0
            && endMinutes > startMinutes;
    }
};

constexpr qint64 PositionStride = 20'000;
constexpr int IntensiveFirstHour = 9;
constexpr int IntensiveFinalHour = 21;

qint64 positionKey(
    int row,
    int column
    )
{
    return static_cast<qint64>(row) * PositionStride + column;
}

QString cellReference(
    int row,
    int column
    )
{
    QString letters;
    int value = column;

    while (value > 0)
    {
        --value;
        letters.prepend(
            QChar(
                QLatin1Char('A').unicode()
                + value % 26
                )
            );
        value /= 26;
    }

    return letters + QString::number(row);
}

const CalendarImport::Cell* cellAt(
    const QHash<qint64, const CalendarImport::Cell*>& cells,
    int row,
    int column
    )
{
    return cells.value(
        positionKey(row, column),
        nullptr
        );
}

QString normalizedToken(
    const QString& value
    )
{
    return value.normalized(QString::NormalizationForm_KC)
        .simplified()
        .toCaseFolded();
}

QString weekdayFor(
    const QString& value
    )
{
    const QString normalized =
        normalizedToken(value).toUpper();

    if (
        normalized.contains(QStringLiteral("MON"))
        || normalized.contains(QChar(0xC6D4))
        )
    {
        return QStringLiteral("Monday");
    }
    if (
        normalized.contains(QStringLiteral("TUE"))
        || normalized.contains(QChar(0xD654))
        )
    {
        return QStringLiteral("Tuesday");
    }
    if (
        normalized.contains(QStringLiteral("WED"))
        || normalized.contains(QChar(0xC218))
        )
    {
        return QStringLiteral("Wednesday");
    }
    if (
        normalized.contains(QStringLiteral("THU"))
        || normalized.contains(QChar(0xBAA9))
        )
    {
        return QStringLiteral("Thursday");
    }
    if (
        normalized.contains(QStringLiteral("FRI"))
        || normalized.contains(QChar(0xAE08))
        )
    {
        return QStringLiteral("Friday");
    }

    return {};
}

RawTimeRange rawTimeRange(
    const QString& value
    )
{
    static const QRegularExpression expression(
        QStringLiteral(
            R"((\d{1,2})\s*:\s*(\d{2})\s*[~\-–—]\s*(\d{1,2})\s*:\s*(\d{2}))"
            )
        );

    const QRegularExpressionMatch match =
        expression.match(value);

    if (!match.hasMatch())
    {
        return {};
    }

    RawTimeRange result;
    result.startHour = match.captured(1).toInt();
    result.startMinute = match.captured(2).toInt();
    result.endHour = match.captured(3).toInt();
    result.endMinute = match.captured(4).toInt();
    return result.valid()
        ? result
        : RawTimeRange{};
}

bool resolveTimetableTimes(
    QList<TimetableRow>* rows,
    ScheduleImportKind kind
    )
{
    if (!rows || rows->isEmpty())
    {
        return false;
    }

    int noonTransition = -1;
    bool invalidDecrease = false;
    for (int index = 1; index < rows->size(); ++index)
    {
        if (
            rows->at(index).raw.startHour
            >= rows->at(index - 1).raw.startHour
            )
        {
            continue;
        }

        if (
            noonTransition < 0
            && rows->at(index - 1).raw.startHour == 12
            && rows->at(index).raw.startHour < 12
            )
        {
            noonTransition = index;
        }
        else
        {
            invalidDecrease = true;
        }
    }

    const bool ambiguousIntensive =
        kind == ScheduleImportKind::Intensive
        && (
            invalidDecrease
            || (
                noonTransition < 0
                && rows->first().raw.startHour >= 9
                )
            );
    if (ambiguousIntensive)
    {
        return true;
    }

    bool afterNoon =
        kind == ScheduleImportKind::Normal
        || noonTransition < 0;

    for (int index = 0; index < rows->size(); ++index)
    {
        TimetableRow& row = (*rows)[index];
        int hour = row.raw.startHour;

        if (kind == ScheduleImportKind::Normal)
        {
            if (hour != 12)
            {
                hour += 12;
            }
        }
        else
        {
            if (index == noonTransition)
            {
                afterNoon = true;
            }

            if (afterNoon && hour != 12)
            {
                hour += 12;
            }
        }

        row.startMinutes =
            hour * 60 + row.raw.startMinute;

        int endHour = row.raw.endHour;
        QList<int> candidates{
            endHour * 60 + row.raw.endMinute,
            (endHour + 12) * 60 + row.raw.endMinute,
            (endHour + 24) * 60 + row.raw.endMinute
        };
        row.endMinutes = -1;

        for (int candidate : candidates)
        {
            if (
                candidate > row.startMinutes
                && (
                    row.endMinutes < 0
                    || candidate < row.endMinutes
                    )
                )
            {
                row.endMinutes = candidate;
            }
        }
    }

    return false;
}

QString formattedTime(
    int minutes
    )
{
    if (minutes < 0)
    {
        return {};
    }

    const int normalized =
        minutes % (24 * 60);

    return QTime(
        normalized / 60,
        normalized % 60
        ).toString(QStringLiteral("h:mm AP"));
}

QString intensiveSlotTime(
    int minutes
    )
{
    return QTime(
        minutes / 60,
        minutes % 60
        ).toString(QStringLiteral("HH:mm"));
}

void setIntensiveSlotState(
    QList<IntensiveSlotState>* states,
    const QString& day,
    int startMinutes,
    const QString& state
    )
{
    if (!states || startMinutes < 0)
    {
        return;
    }

    const QString startTime =
        intensiveSlotTime(startMinutes);
    for (IntensiveSlotState& existing : *states)
    {
        if (
            existing.day == day
            && existing.startTime == startTime
            )
        {
            existing.state = state;
            return;
        }
    }

    states->append({day, startTime, state});
}

void initializeIntensiveSlotStates(
    ScheduleImportUserBlock* result,
    const QStringList& days
    )
{
    if (!result)
    {
        return;
    }

    for (const QString& day : days)
    {
        if (day.isEmpty())
        {
            continue;
        }

        for (int hour = IntensiveFirstHour;
             hour <= IntensiveFinalHour;
             ++hour)
        {
            setIntensiveSlotState(
                &result->intensiveSlotStates,
                day,
                hour * 60,
                QStringLiteral("empty")
                );
        }
    }
}

QString canonicalGrade(
    const QString& value
    )
{
    for (const QString& grade : ClassInfoConfig::Grades)
    {
        if (grade.compare(value, Qt::CaseInsensitive) == 0)
        {
            return grade;
        }
    }

    return {};
}

QString canonicalLevel(
    const QString& grade,
    const QString& value
    )
{
    for (const QString& level : ClassInfoConfig::levelsForGrade(grade))
    {
        if (level.compare(value, Qt::CaseInsensitive) == 0)
        {
            return level;
        }
    }

    return {};
}

bool ignoredTimetableValue(
    const QString& value
    )
{
    static const QSet<QString> ignored{
        QStringLiteral("ESSAY"),
        QStringLiteral("LUNCH"),
        QStringLiteral("READY")
    };

    return ignored.contains(
        value.simplified().toUpper()
        );
}

struct ParsedClassCell
{
    bool parsed = false;
    QString teacherKey;
    QString teacherKr;
    QString room;
    QString grade;
    QString level;
    RawTimeRange explicitTime;
};

ParsedClassCell parseClassCell(
    const QString& value
    )
{
    static const QRegularExpression courseExpression(
        QStringLiteral(
            R"(\b(E[4-6]|M[1-3])\s*[-–—]\s*([A-Za-z]+(?:['’][A-Za-z]+)?)\b)"
            ),
        QRegularExpression::CaseInsensitiveOption
        );
    static const QRegularExpression roomExpression(
        QStringLiteral(
            R"((?:\(|\s)([A-Za-z]?\d{3,4})\)?)"
            )
        );

    const QRegularExpressionMatch course =
        courseExpression.match(value);

    if (!course.hasMatch())
    {
        return {};
    }

    const QString grade =
        canonicalGrade(course.captured(1));
    const QString level =
        canonicalLevel(
            grade,
            course.captured(2).replace(
                QChar(0x2019),
                QLatin1Char('\'')
                )
            );

    if (grade.isEmpty() || level.isEmpty())
    {
        return {};
    }

    const QString prefix =
        value.left(course.capturedStart());
    const QString teacherKr =
        TeacherImportNameUtils::hangulOnly(prefix);
    const QRegularExpressionMatch room =
        roomExpression.match(prefix);

    if (teacherKr.isEmpty() || !room.hasMatch())
    {
        return {};
    }

    ParsedClassCell result;
    result.parsed = true;
    result.teacherKey = teacherKr;
    result.teacherKr = teacherKr;
    result.room = room.captured(1).trimmed();
    result.grade = grade;
    result.level = level;
    result.explicitTime = rawTimeRange(value);
    return result;
}

ResolvedTimeRange resolvedExplicitRange(
    const RawTimeRange& range,
    int fallbackStart
    )
{
    if (!range.valid())
    {
        return {};
    }

    const QList<int> startCandidates{
        range.startHour * 60 + range.startMinute,
        (range.startHour + 12) * 60 + range.startMinute,
        (range.startHour + 24) * 60 + range.startMinute
    };
    int explicitStart = startCandidates.first();

    for (int candidate : startCandidates)
    {
        if (
            qAbs(candidate - fallbackStart)
            < qAbs(explicitStart - fallbackStart)
            )
        {
            explicitStart = candidate;
        }
    }

    const QList<int> endCandidates{
        range.endHour * 60 + range.endMinute,
        (range.endHour + 12) * 60 + range.endMinute,
        (range.endHour + 24) * 60 + range.endMinute
    };

    for (int candidate : endCandidates)
    {
        if (candidate > explicitStart)
        {
            return {explicitStart, candidate};
        }
    }

    return {};
}

const CalendarImport::CellRange* mergedRangeAt(
    const CalendarImport::Worksheet& worksheet,
    int row,
    int column
    )
{
    for (const CalendarImport::CellRange& range : worksheet.mergedRanges)
    {
        if (range.contains(row, column))
        {
            return &range;
        }
    }

    return nullptr;
}

bool sameTime(
    const ClassTime& left,
    const ClassTime& right
    )
{
    return left.day == right.day
        && left.startTime == right.startTime
        && left.endTime == right.endTime;
}

QString classCellColor(
    const CalendarImport::Cell& cell,
    const QVector<CalendarImport::Style>& styles
    )
{
    if (
        cell.style < 0
        || cell.style >= styles.size()
        )
    {
        return {};
    }

    const CalendarImport::Style& style =
        styles[cell.style];
    QString color =
        CalendarImport::normalizedColor(
            style.fillColor
            );
    if (color.size() == 8)
    {
        color = color.right(6);
    }
    return style.filled && color.size() == 6
        ? QLatin1Char('#') + color
        : QString();
}

struct ParsedScheduleOccurrence
{
    QString teacherKey;
    QString teacherKr;
    QString room;
    QString color;
    QString classGrade;
    QString classLevel;
    ClassTime time;
    QString sourceCell;
};

int occurrenceGroupScore(
    const QList<ParsedScheduleOccurrence>& occurrences,
    const QList<int>& indexes
    )
{
    if (indexes.isEmpty())
    {
        return 0;
    }

    const ParsedScheduleOccurrence& first =
        occurrences[indexes.first()];
    bool sameColor = !first.color.isEmpty();
    bool sameTime = true;
    bool sameRoom = true;

    for (int index : indexes)
    {
        const ParsedScheduleOccurrence& occurrence =
            occurrences[index];
        sameColor =
            sameColor
            && occurrence.color == first.color;
        sameTime =
            sameTime
            && occurrence.time.startTime
                == first.time.startTime
            && occurrence.time.endTime
                == first.time.endTime;
        sameRoom =
            sameRoom
            && occurrence.room == first.room;
    }

    return (sameColor ? 1000 : 0)
        + (sameTime ? 500 : 0)
        + (sameRoom ? 100 : 0);
}

struct OccurrencePartition
{
    bool valid = false;
    int score = std::numeric_limits<int>::min();
    QList<QList<int>> groups;
};

OccurrencePartition bestOccurrencePartition(
    const QList<ParsedScheduleOccurrence>& occurrences,
    const QList<QStringList>& allowedPatterns,
    const QList<int>& remaining
    )
{
    if (remaining.isEmpty())
    {
        return {true, 0, {}};
    }

    const int firstIndex =
        remaining.first();
    const QString firstDay =
        occurrences[firstIndex].time.day;
    QList<int> fallbackRemaining =
        remaining;
    fallbackRemaining.removeFirst();
    const OccurrencePartition fallbackRest =
        bestOccurrencePartition(
            occurrences,
            allowedPatterns,
            fallbackRemaining
            );
    OccurrencePartition best;
    if (fallbackRest.valid)
    {
        best.valid = true;
        best.score = fallbackRest.score - 10'000;
        best.groups = {{firstIndex}};
        best.groups.append(fallbackRest.groups);
    }

    for (const QStringList& pattern : allowedPatterns)
    {
        if (!pattern.contains(firstDay))
        {
            continue;
        }

        QStringList daysToChoose =
            pattern;
        daysToChoose.removeOne(firstDay);
        QList<int> selected{firstIndex};

        std::function<void(int)> choose;
        choose =
            [&](int dayIndex)
            {
                if (dayIndex >= daysToChoose.size())
                {
                    QList<int> nextRemaining =
                        remaining;
                    for (int index : selected)
                    {
                        nextRemaining.removeAll(index);
                    }

                    const OccurrencePartition rest =
                        bestOccurrencePartition(
                            occurrences,
                            allowedPatterns,
                            nextRemaining
                            );
                    if (!rest.valid)
                    {
                        return;
                    }

                    const int score =
                        occurrenceGroupScore(
                            occurrences,
                            selected
                            )
                        + rest.score;
                    if (!best.valid || score > best.score)
                    {
                        best.valid = true;
                        best.score = score;
                        best.groups = {selected};
                        best.groups.append(rest.groups);
                    }
                    return;
                }

                const QString day =
                    daysToChoose[dayIndex];
                for (int index : remaining)
                {
                    if (
                        selected.contains(index)
                        || occurrences[index].time.day != day
                        )
                    {
                        continue;
                    }
                    selected.append(index);
                    choose(dayIndex + 1);
                    selected.removeLast();
                }
            };
        choose(0);
    }

    return best;
}

ScheduleImportClassCandidate candidateForOccurrences(
    const QList<ParsedScheduleOccurrence>& occurrences,
    const QList<int>& indexes
    )
{
    ScheduleImportClassCandidate candidate;
    if (indexes.isEmpty())
    {
        return candidate;
    }

    const ParsedScheduleOccurrence& first =
        occurrences[indexes.first()];
    candidate.teacherKey = first.teacherKey;
    candidate.teacherKr = first.teacherKr;
    candidate.classGrade = first.classGrade;
    candidate.classLevel = first.classLevel;

    for (int index : indexes)
    {
        const ParsedScheduleOccurrence& occurrence =
            occurrences[index];
        if (!candidate.rooms.contains(occurrence.room))
        {
            candidate.rooms.append(occurrence.room);
        }
        if (
            !occurrence.color.isEmpty()
            && !candidate.importedColors.contains(
                occurrence.color
                )
            )
        {
            candidate.importedColors.append(
                occurrence.color
                );
        }
        if (
            std::none_of(
                candidate.times.cbegin(),
                candidate.times.cend(),
                [&occurrence](const ClassTime& existing)
                {
                    return sameTime(
                        existing,
                        occurrence.time
                        );
                }
                )
            )
        {
            candidate.times.append(occurrence.time);
        }
        if (
            !candidate.sourceCells.contains(
                occurrence.sourceCell
                )
            )
        {
            candidate.sourceCells.append(
                occurrence.sourceCell
                );
        }
    }

    candidate.meetingPatternError =
        scheduleImportMeetingPatternError(candidate);
    return candidate;
}

QList<ScheduleImportClassCandidate> aggregateOccurrences(
    const QList<ParsedScheduleOccurrence>& occurrences
    )
{
    QHash<QString, QList<ParsedScheduleOccurrence>> grouped;
    QStringList groupOrder;

    for (const ParsedScheduleOccurrence& occurrence : occurrences)
    {
        const QString key =
            QStringLiteral("%1\x1f%2\x1f%3")
                .arg(
                    occurrence.teacherKey,
                    occurrence.classGrade,
                    occurrence.classLevel
                    );
        if (!grouped.contains(key))
        {
            groupOrder.append(key);
        }
        grouped[key].append(occurrence);
    }

    QList<ScheduleImportClassCandidate> candidates;
    for (const QString& key : groupOrder)
    {
        const QList<ParsedScheduleOccurrence>& group =
            grouped[key];
        const QList<QStringList> patterns =
            scheduleImportAllowedDayPatterns(
                group.first().classGrade,
                group.first().classLevel
                );
        QList<int> remaining;
        for (int index = 0; index < group.size(); ++index)
        {
            remaining.append(index);
        }
        if (patterns.isEmpty())
        {
            candidates.append(
                candidateForOccurrences(
                    group,
                    remaining
                    )
                );
            continue;
        }
        const OccurrencePartition partition =
            bestOccurrencePartition(
                group,
                patterns,
                remaining
                );

        if (partition.valid)
        {
            for (const QList<int>& indexes : partition.groups)
            {
                candidates.append(
                    candidateForOccurrences(
                        group,
                        indexes
                        )
                    );
            }
        }
        else
        {
            candidates.append(
                candidateForOccurrences(
                    group,
                    remaining
                    )
                );
        }
    }

    return candidates;
}

ScheduleImportUserBlock parseBlock(
    const CalendarImport::Worksheet& worksheet,
    const QVector<CalendarImport::Style>& styles,
    const QHash<qint64, const CalendarImport::Cell*>& cells,
    const CalendarImport::Cell& header,
    ScheduleImportKind kind,
    const ScheduleImportCancellation& isCancelled
    )
{
    ScheduleImportUserBlock result;
    result.name = header.value.simplified();
    result.headerCell =
        cellReference(header.row, header.column);

    QStringList days;
    for (int offset = 1; offset <= 5; ++offset)
    {
        const CalendarImport::Cell* dayCell =
            cellAt(
                cells,
                header.row,
                header.column + offset
                );
        days.append(
            dayCell
                ? weekdayFor(dayCell->value)
                : QString()
            );
    }

    QList<TimetableRow> timetableRows;
    for (int row = header.row + 1;
         row <= header.row + 20;
         ++row)
    {
        if (isCancelled && isCancelled())
        {
            return result;
        }

        const CalendarImport::Cell* timeCell =
            cellAt(cells, row, header.column);

        if (!timeCell)
        {
            break;
        }

        const RawTimeRange raw =
            rawTimeRange(timeCell->value);

        if (!raw.valid())
        {
            break;
        }

        timetableRows.append({row, raw});
    }

    const bool ambiguousTimes =
        resolveTimetableTimes(
        &timetableRows,
        kind
        );

    if (ambiguousTimes)
    {
        for (const TimetableRow& row : timetableRows)
        {
            if (isCancelled && isCancelled())
            {
                return result;
            }

            for (int dayOffset = 0;
                 dayOffset < days.size();
                 ++dayOffset)
            {
                const int column =
                    header.column + dayOffset + 1;
                const CalendarImport::Cell* cell =
                    cellAt(cells, row.row, column);
                if (
                    cell
                    && parseClassCell(cell->value).parsed
                    )
                {
                    result.diagnostics.append(
                        {
                            worksheet.name,
                            result.name,
                            cellReference(row.row, column),
                            cell->value,
                            QObject::tr(
                                "The intensive schedule's AM-to-PM transition is ambiguous."
                                )
                        }
                        );
                }
            }
        }
        return result;
    }

    QHash<int, TimetableRow> timeByRow;
    for (const TimetableRow& row : timetableRows)
    {
        if (isCancelled && isCancelled())
        {
            return result;
        }

        timeByRow.insert(row.row, row);
    }

    if (kind == ScheduleImportKind::Intensive)
    {
        initializeIntensiveSlotStates(
            &result,
            days
            );

        for (const TimetableRow& row : timetableRows)
        {
            for (int dayOffset = 0;
                 dayOffset < days.size();
                 ++dayOffset)
            {
                const QString& day = days[dayOffset];
                if (day.isEmpty())
                {
                    continue;
                }

                const int column =
                    header.column + dayOffset + 1;
                const CalendarImport::Cell* cell =
                    cellAt(cells, row.row, column);
                if (!cell)
                {
                    if (
                        const CalendarImport::CellRange* range =
                            mergedRangeAt(
                                worksheet,
                                row.row,
                                column
                                )
                        )
                    {
                        cell =
                            cellAt(
                                cells,
                                range->firstRow,
                                range->firstColumn
                                );
                    }
                }

                const QString value =
                    cell ? cell->value.trimmed() : QString();
                const QString state =
                    value.isEmpty()
                        ? QStringLiteral("empty")
                        : value.compare(
                            QStringLiteral("Lunch"),
                            Qt::CaseInsensitive
                            ) == 0
                            ? QStringLiteral("lunch")
                            : QStringLiteral("essay");
                setIntensiveSlotState(
                    &result.intensiveSlotStates,
                    day,
                    row.startMinutes,
                    state
                    );
            }
        }
    }

    QList<ParsedScheduleOccurrence> occurrences;

    for (const TimetableRow& row : timetableRows)
    {
        for (int dayOffset = 0;
             dayOffset < days.size();
             ++dayOffset)
        {
            if (isCancelled && isCancelled())
            {
                return result;
            }

            if (days[dayOffset].isEmpty())
            {
                continue;
            }

            const int column =
                header.column + dayOffset + 1;
            const CalendarImport::Cell* cell =
                cellAt(cells, row.row, column);

            if (!cell || cell->value.trimmed().isEmpty())
            {
                continue;
            }

            if (ignoredTimetableValue(cell->value))
            {
                continue;
            }

            const ParsedClassCell parsed =
                parseClassCell(cell->value);
            const QString reference =
                cellReference(row.row, column);

            if (!parsed.parsed)
            {
                result.diagnostics.append(
                    {
                        worksheet.name,
                        result.name,
                        reference,
                        cell->value,
                        QObject::tr(
                            "This occupied timetable cell was not recognized as a class."
                            )
                    }
                    );
                continue;
            }

            int startMinutes =
                row.startMinutes;
            int endMinutes =
                row.endMinutes;

            if (parsed.explicitTime.valid())
            {
                const ResolvedTimeRange explicitRange =
                    resolvedExplicitRange(
                        parsed.explicitTime,
                        row.startMinutes
                        );
                if (explicitRange.valid())
                {
                    startMinutes =
                        explicitRange.startMinutes;
                    endMinutes =
                        explicitRange.endMinutes;
                }
            }
            else if (
                const CalendarImport::CellRange* range =
                    mergedRangeAt(
                        worksheet,
                        row.row,
                        column
                        )
                )
            {
                if (timeByRow.contains(range->lastRow))
                {
                    endMinutes =
                        timeByRow.value(range->lastRow).endMinutes;
                }
            }

            if (
                startMinutes < 0
                || endMinutes <= startMinutes
                )
            {
                result.diagnostics.append(
                    {
                        worksheet.name,
                        result.name,
                        reference,
                        cell->value,
                        QObject::tr(
                            "The class time could not be interpreted."
                            )
                    }
                    );
                continue;
            }

            ClassTime time;
            time.day = days[dayOffset];
            time.startTime = formattedTime(startMinutes);
            time.endTime = formattedTime(endMinutes);

            if (
                std::any_of(
                    occurrences.cbegin(),
                    occurrences.cend(),
                    [&parsed, &time](
                        const ParsedScheduleOccurrence& existing
                        )
                    {
                        return existing.teacherKey
                                == parsed.teacherKey
                            && existing.classGrade
                                == parsed.grade
                            && existing.classLevel
                                == parsed.level
                            && sameTime(existing.time, time);
                    }
                    )
                )
            {
                continue;
            }

            ParsedScheduleOccurrence occurrence;
            occurrence.teacherKey = parsed.teacherKey;
            occurrence.teacherKr = parsed.teacherKr;
            occurrence.room = parsed.room;
            occurrence.color =
                classCellColor(*cell, styles);
            occurrence.classGrade = parsed.grade;
            occurrence.classLevel = parsed.level;
            occurrence.time = time;
            occurrence.sourceCell = reference;
            occurrences.append(occurrence);
        }
    }

    result.classes =
        aggregateOccurrences(occurrences);

    return result;
}

bool validHeader(
    const CalendarImport::Cell& candidate,
    const QHash<qint64, const CalendarImport::Cell*>& cells
    )
{
    if (
        candidate.value.trimmed().isEmpty()
        || rawTimeRange(candidate.value).valid()
        )
    {
        return false;
    }

    for (int offset = 1; offset <= 5; ++offset)
    {
        const CalendarImport::Cell* day =
            cellAt(
                cells,
                candidate.row,
                candidate.column + offset
                );

        if (
            !day
            || weekdayFor(day->value).isEmpty()
            )
        {
            return false;
        }
    }

    return true;
}
}

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind,
    const ScheduleImportCancellation& isCancelled
    )
{
    const auto cancelled = [&isCancelled]()
    {
        return isCancelled && isCancelled();
    };

    if (cancelled())
    {
        return std::unexpected(QObject::tr("The schedule import was cancelled."));
    }

    QString errorMessage;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(
            data,
            &errorMessage
            );

    if (workbook.worksheets.isEmpty())
    {
        return std::unexpected(
            errorMessage.trimmed().isEmpty()
                ? QObject::tr("The workbook could not be read.")
                : errorMessage
            );
    }

    ScheduleImportWorkbook result;

    for (const CalendarImport::Worksheet& worksheet : workbook.worksheets)
    {
        if (cancelled())
        {
            return std::unexpected(QObject::tr("The schedule import was cancelled."));
        }

        ScheduleImportSheet sheet;
        sheet.name = worksheet.name;
        sheet.visible = worksheet.visible;

        QHash<qint64, const CalendarImport::Cell*> cells;
        for (const CalendarImport::Cell& cell : worksheet.cells)
        {
            if (cancelled())
            {
                return std::unexpected(QObject::tr("The schedule import was cancelled."));
            }

            cells.insert(
                positionKey(cell.row, cell.column),
                &cell
                );
        }

        for (const CalendarImport::Cell& cell : worksheet.cells)
        {
            if (!validHeader(cell, cells))
            {
                continue;
            }

            ScheduleImportUserBlock block =
                parseBlock(
                    worksheet,
                    workbook.styles,
                    cells,
                    cell,
                    kind,
                    isCancelled
                    );

            if (cancelled())
            {
                return std::unexpected(QObject::tr("The schedule import was cancelled."));
            }

            if (!block.classes.isEmpty())
            {
                sheet.users.append(block);
            }
            else if (!block.diagnostics.isEmpty())
            {
                sheet.diagnostics.append(
                    block.diagnostics
                    );
            }
        }

        result.sheets.append(sheet);
    }

    const bool hasVisibleSchedule =
        std::any_of(
            result.sheets.cbegin(),
            result.sheets.cend(),
            [](const ScheduleImportSheet& sheet)
            {
                return sheet.visible
                    && !sheet.users.isEmpty();
            }
            );

    if (!hasVisibleSchedule)
    {
        for (const ScheduleImportSheet& sheet : result.sheets)
        {
            if (
                !sheet.visible
                || sheet.diagnostics.isEmpty()
                )
            {
                continue;
            }
            const ScheduleImportDiagnostic& diagnostic =
                sheet.diagnostics.first();
            return std::unexpected(
                QObject::tr("%1!%2: %3")
                    .arg(
                        sheet.name,
                        diagnostic.cellReference,
                        diagnostic.message
                        )
                );
        }
        return std::unexpected(
            QObject::tr(
                "No supported user schedule blocks were found in the workbook."
                )
            );
    }

    return result;
}

QString normalizedScheduleImportUserName(
    const QString& value
    )
{
    const QString normalized =
        value.normalized(
            QString::NormalizationForm_KC
            ).toCaseFolded();
    QString result;
    bool pendingSpace = false;

    for (const QChar character : normalized)
    {
        if (character.isLetterOrNumber())
        {
            if (pendingSpace && !result.isEmpty())
            {
                result.append(QLatin1Char(' '));
            }
            result.append(character);
            pendingSpace = false;
        }
        else if (character.isSpace())
        {
            pendingSpace = true;
        }
    }

    return result.trimmed();
}
