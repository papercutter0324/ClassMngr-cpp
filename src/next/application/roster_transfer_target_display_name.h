#pragma once

#include "next/application/classes_list_read_snapshot.h"
#include "next/application/qt_compatible_text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterTransferTargetRegularTime final
{
    std::u16string day;
    std::u16string startTime;

    friend bool operator==(
        const RosterTransferTargetRegularTime&,
        const RosterTransferTargetRegularTime&
        ) = default;
};

struct RosterTransferTargetDisplayMetadata final
{
    std::u16string grade;
    std::u16string level;
    std::u16string teacherEnglishName;
    std::u16string teacherKoreanName;
    std::u16string teacherPreferredRomanization;
    std::u16string teacherPreferredName;
    std::vector<RosterTransferTargetRegularTime> regularSchedule;

    friend bool operator==(
        const RosterTransferTargetDisplayMetadata&,
        const RosterTransferTargetDisplayMetadata&
        ) = default;
};

namespace RosterTransferTargetDisplayNameDetail
{

[[nodiscard]] inline std::u16string abbreviatedDay(std::u16string day)
{
    constexpr std::array<std::pair<std::u16string_view, std::u16string_view>, 7>
        abbreviations{{
            {u"Monday", u"Mon"},
            {u"Tuesday", u"Tues"},
            {u"Wednesday", u"Wed"},
            {u"Thursday", u"Thurs"},
            {u"Friday", u"Fri"},
            {u"Saturday", u"Sat"},
            {u"Sunday", u"Sun"}
        }};
    const auto found = std::find_if(abbreviations.begin(), abbreviations.end(),
        [&day](const auto& entry) { return entry.first == day; });
    return found == abbreviations.end()
        ? std::move(day) : std::u16string(found->second);
}

inline void replaceAll(
    std::u16string& value,
    const std::u16string_view needle,
    const std::u16string_view replacement
    )
{
    std::size_t position = 0;
    while ((position = value.find(needle, position)) != std::u16string::npos)
    {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

[[nodiscard]] inline std::u16string regularScheduleText(
    const std::vector<RosterTransferTargetRegularTime>& schedule
    )
{
    std::vector<std::u16string> dayLabels;
    std::vector<std::u16string> timeLabels;
    dayLabels.reserve(schedule.size());
    timeLabels.reserve(schedule.size());
    for (const RosterTransferTargetRegularTime& time : schedule)
    {
        dayLabels.push_back(abbreviatedDay(time.day));
        std::u16string startTime = time.startTime;
        replaceAll(startTime, u" AM", u"");
        replaceAll(startTime, u" PM", u"");
        timeLabels.push_back(std::move(startTime));
    }

    std::u16string daysText;
    const auto hasDays = [&dayLabels](
        const std::initializer_list<std::u16string_view> expected) {
        return dayLabels.size() == expected.size()
            && std::equal(dayLabels.begin(), dayLabels.end(), expected.begin(),
                [](const std::u16string& actual,
                   const std::u16string_view value) {
                    return actual == value;
                });
    };
    if (hasDays({u"Mon", u"Wed"}))
    {
        daysText = u"M/W";
    }
    else if (hasDays({u"Mon", u"Fri"}))
    {
        daysText = u"M/F";
    }
    else if (hasDays({u"Wed", u"Fri"}))
    {
        daysText = u"W/F";
    }
    else if (hasDays({u"Mon", u"Wed", u"Fri"}))
    {
        daysText = u"M/W/F";
    }
    else if (hasDays({u"Tues", u"Thurs"}))
    {
        daysText = u"T/Th";
    }
    else
    {
        for (const std::u16string& day : dayLabels)
        {
            if (!daysText.empty())
            {
                daysText.push_back(u'/');
            }
            daysText.append(day);
        }
    }

    std::vector<std::u16string> uniqueTimes;
    for (const std::u16string& time : timeLabels)
    {
        if (std::find(uniqueTimes.begin(), uniqueTimes.end(), time)
            == uniqueTimes.end())
        {
            uniqueTimes.push_back(time);
        }
    }

    std::u16string timeText;
    if (uniqueTimes.size() == 1)
    {
        timeText = uniqueTimes.front();
    }
    else
    {
        for (const std::u16string& time : timeLabels)
        {
            if (!timeText.empty())
            {
                timeText.append(u" / ");
            }
            timeText.append(time);
        }
    }

    return daysText + u" (" + timeText + u")";
}

} // namespace RosterTransferTargetDisplayNameDetail

[[nodiscard]] inline std::optional<std::u16string>
formatRosterTransferTargetDisplayName(
    const RosterTransferTargetDisplayMetadata& metadata,
    const std::u16string_view unknownClassLabel,
    const std::u16string_view noTeacherLabel,
    const std::size_t maximumLength = kClassesNavigationMaximumTextLength
    )
{
    const std::u16string grade = trimQtWhitespace(metadata.grade);
    const std::u16string level = trimQtWhitespace(metadata.level);
    std::u16string levelText;
    if (!grade.empty() && !level.empty())
    {
        levelText = grade + u" " + level;
    }
    else if (!grade.empty())
    {
        levelText = grade;
    }
    else if (!level.empty())
    {
        levelText = level;
    }
    else
    {
        levelText = unknownClassLabel;
    }

    const std::array<std::u16string, 4> teacherNames{
        trimQtWhitespace(metadata.teacherPreferredName),
        trimQtWhitespace(metadata.teacherEnglishName),
        trimQtWhitespace(metadata.teacherPreferredRomanization),
        trimQtWhitespace(metadata.teacherKoreanName)
    };
    const auto teacherName = std::find_if(teacherNames.begin(), teacherNames.end(),
        [](const std::u16string& value) { return !value.empty(); });
    const std::u16string teacher = teacherName == teacherNames.end()
        ? std::u16string(noTeacherLabel) : *teacherName;

    std::u16string displayName = levelText + u" • " + teacher;
    if (!metadata.regularSchedule.empty())
    {
        displayName.append(u" • ");
        displayName.append(
            RosterTransferTargetDisplayNameDetail::regularScheduleText(
                metadata.regularSchedule));
    }

    if (displayName.size() > maximumLength)
    {
        return std::nullopt;
    }
    return displayName;
}

} // namespace ClassMngr::Next::Application
