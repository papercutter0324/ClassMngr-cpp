#include "schedule_builder.h"

#include "data/data_service.h"

#include <QTime>

namespace
{
constexpr int DefaultStartHour = 16;
constexpr int FinalHour = 21;
constexpr int FullIntensiveStartHour = 9;
constexpr int FullIntensiveFinalHour = 21;

ScheduleEntry toEntry(
    int classId,
    const ClassInfo& info
    )
{
    ScheduleEntry entry;

    entry.classId = classId;
    entry.teacherKr = info.teacherKr;
    entry.teacherEn = info.teacherEn;
    entry.roomNumber = info.roomNumber;
    entry.classGrade = info.classGrade;
    entry.classLevel = info.classLevel;
    entry.classColor =
        info.classColor.isEmpty()
            ? QStringLiteral("#FFFFFF")
            : info.classColor;
    entry.fontColor =
        info.fontColor.isEmpty()
            ? QStringLiteral("#000000")
            : info.fontColor;

    return entry;
}
}

ScheduleBuilder::ScheduleBuilder(
    DataService* dataService
    )
    : m_dataService(dataService)
{
}

ScheduleBuildResult ScheduleBuilder::build(
    bool useIntensive,
    const QStringList& visibleDays,
    bool showAllHours
    ) const
{
    ScheduleBuildResult result;
    result.days = visibleDays;

    if (
        !m_dataService
        || !m_dataService->isOpen()
        )
    {
        return result;
    }

    for (const QString& day : visibleDays)
    {
        result.schedule.insert(
            day,
            {}
            );
    }

    QList<ParsedClass> parsedClasses;
    bool hasEarliestHour = false;
    bool hasLatestHour = false;
    int earliestHour = 0;
    int latestHour = 0;
    int scheduleOffset = 0;

    const QList<Classroom> classes =
        m_dataService->getClasses();

    for (const Classroom& classroom : classes)
    {
        const ClassInfo info =
            m_dataService->loadClassInfo(
                classroom.id
                );

        const QList<ClassTime>& times =
            useIntensive
                ? info.intensiveTimes
                : info.classTimes;

        for (const ClassTime& time : times)
        {
            const QString day =
                time.day.trimmed().isEmpty()
                    ? QStringLiteral("Monday")
                    : time.day.trimmed();

            if (!visibleDays.contains(day))
            {
                continue;
            }

            const QTime startTime =
                parseTime(time.startTime);

            if (!startTime.isValid())
            {
                continue;
            }

            const QTime endTime =
                parseTime(time.endTime);

            int adjustedHour =
                startTime.hour();

            if (startTime.minute() == 55)
            {
                ++adjustedHour;
            }

            if (!hasEarliestHour || adjustedHour < earliestHour)
            {
                earliestHour = adjustedHour;
                hasEarliestHour = true;
            }

            if (endTime.isValid())
            {
                int adjustedEndHour =
                    endTime.hour();

                if (endTime.minute() == 55)
                {
                    ++adjustedEndHour;
                }

                if (!hasLatestHour || adjustedEndHour > latestHour)
                {
                    latestHour = adjustedEndHour;
                    hasLatestHour = true;
                }

                if (endTime.minute() == 55)
                {
                    result.uses55Endings = true;
                }
            }

            if (startTime.minute() == 55)
            {
                scheduleOffset = 55;
            }
            else if (
                startTime.minute() == 5
                && scheduleOffset != 55
                )
            {
                scheduleOffset = 5;
            }

            parsedClasses.append(
                {
                    day,
                    startTime,
                    toEntry(classroom.id, info)
                }
                );
        }
    }

    const bool showFullIntensiveHours =
        useIntensive && showAllHours;

    const int startHour =
        showFullIntensiveHours
            ? FullIntensiveStartHour
            : hasEarliestHour && earliestHour < DefaultStartHour
                ? earliestHour
                : DefaultStartHour;

    const int finalHour =
        showFullIntensiveHours
            ? FullIntensiveFinalHour
            : useIntensive && hasLatestHour
                ? latestHour
                : FinalHour;

    if (showFullIntensiveHours)
    {
        scheduleOffset = 0;
        result.uses55Endings = false;
    }

    result.scheduleOffset = scheduleOffset;
    result.rows =
        buildRows(
            startHour,
            finalHour,
            scheduleOffset
            );

    for (const ParsedClass& parsedClass : parsedClasses)
    {
        const QString label =
            parsedClass.startTime.toString(
                QStringLiteral("HH:mm")
                );

        result.schedule[parsedClass.day][label].append(
            parsedClass.entry
            );
    }

    return result;
}

QTime ScheduleBuilder::parseTime(
    const QString& value
    ) const
{
    const QString trimmed =
        value.trimmed();

    if (trimmed.isEmpty())
    {
        return {};
    }

    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("hh:mm AP"),
        QStringLiteral("hh:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm"),
        QStringLiteral("H:mm:ss"),
        QStringLiteral("HH:mm:ss")
    };

    for (const QString& format : formats)
    {
        const QTime time =
            QTime::fromString(
                trimmed,
                format
                );

        if (time.isValid())
        {
            return time;
        }
    }

    return {};
}

QList<ScheduleRow> ScheduleBuilder::buildRows(
    int startHour,
    int finalHour,
    int offset
    ) const
{
    QList<ScheduleRow> rows;

    for (int hour = startHour; hour <= finalHour; ++hour)
    {
        int displayHour = hour;

        if (offset == 55)
        {
            --displayHour;
        }

        rows.append(
            {
                QStringLiteral("%1:%2")
                    .arg(
                        displayHour,
                        2,
                        10,
                        QLatin1Char('0')
                        )
                    .arg(
                        offset,
                        2,
                        10,
                        QLatin1Char('0')
                        )
            }
            );
    }

    return rows;
}
