#include "schedule_builder.h"

#include "domain/rules/schedule_value_parser.h"

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
                QString::fromStdU16String(time.day.value()).trimmed().isEmpty()
                    ? QStringLiteral("Monday")
                    : QString::fromStdU16String(time.day.value()).trimmed();

            if (!visibleDays.contains(day))
            {
                continue;
            }

            const QString startText =
                QString::fromStdU16String(time.startTime.value());
            const auto parsedStartTime =
                ScheduleValueParser::parseDisplayTime(startText);

            if (!parsedStartTime)
            {
                continue;
            }
            const QTime startTime = *parsedStartTime;

            const QString endText =
                QString::fromStdU16String(time.endTime.value());
            const auto parsedEndTime =
                ScheduleValueParser::parseDisplayTime(endText);
            const QTime endTime =
                parsedEndTime ? *parsedEndTime : QTime{};

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
