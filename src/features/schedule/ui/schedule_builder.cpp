#include "schedule_builder.h"

#include <QTime>

#include <charconv>
#include <string>
#include <system_error>

namespace
{
constexpr int DefaultStartHour = 16;
constexpr int FinalHour = 21;
constexpr int FullIntensiveStartHour = 9;
constexpr int FullIntensiveFinalHour = 21;

ScheduleEntry toEntry(
    const ClassMngr::Next::Application::ScheduleBuilderSourceClass& source
    )
{
    ScheduleEntry entry;

    const std::string& classId = source.classId.value();
    const auto [end, error] = std::from_chars(
        classId.data(),
        classId.data() + classId.size(),
        entry.classId
        );
    if (error != std::errc{} || end != classId.data() + classId.size())
    {
        entry.classId = -1;
    }

    entry.teacherKr = QString::fromStdU16String(source.teacherKoreanName);
    entry.teacherEn = QString::fromStdU16String(source.teacherEnglishName);
    entry.teacherPreferredName = QString::fromStdU16String(
        source.teacherPreferredName
        );
    entry.roomNumber = QString::fromStdU16String(source.roomNumber);
    entry.classGrade = QString::fromStdU16String(source.grade);
    entry.classLevel = QString::fromStdU16String(source.level);
    entry.classColor =
        source.classColor.empty()
            ? QStringLiteral("#FFFFFF")
            : QString::fromStdU16String(source.classColor);
    entry.fontColor =
        source.fontColor.empty()
            ? QStringLiteral("#000000")
            : QString::fromStdU16String(source.fontColor);

    return entry;
}
}

ScheduleBuildResult ScheduleBuilder::build(
    const ClassMngr::Next::Application::
        ScheduleBuilderSourceSnapshot& source,
    const bool useIntensive,
    const QStringList& visibleDays
    ) const
{
    ScheduleBuildResult result;
    result.days = visibleDays;

    for (const QString& day : visibleDays)
    {
        result.schedule.insert(
            day,
            {}
            );
    }

    QList<ParsedClass> parsedClasses;
    bool hasEarliestHour = false;
    int earliestHour = 0;
    int scheduleOffset = 0;

    for (const auto& info : source.classes)
    {
        const auto& times =
            useIntensive
                ? info.intensiveSchedule
                : info.regularSchedule;

        for (const auto& time : times)
        {
            const QString day =
                QString::fromStdU16String(time.day).trimmed().isEmpty()
                    ? QStringLiteral("Monday")
                    : QString::fromStdU16String(time.day).trimmed();

            if (!visibleDays.contains(day))
            {
                continue;
            }

            const QTime startTime =
                parseTime(QString::fromStdU16String(time.startTime));

            if (!startTime.isValid())
            {
                continue;
            }

            const QTime endTime =
                parseTime(QString::fromStdU16String(time.endTime));

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
                    toEntry(info)
                }
                );
        }
    }

    const int startHour =
        useIntensive
            ? FullIntensiveStartHour
            : hasEarliestHour && earliestHour < DefaultStartHour
                ? earliestHour
                : DefaultStartHour;

    const int finalHour =
        useIntensive
            ? FullIntensiveFinalHour
            : FinalHour;

    if (useIntensive)
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
