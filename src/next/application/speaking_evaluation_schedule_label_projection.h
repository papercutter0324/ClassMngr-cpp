#pragma once

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationScheduleLabel final
{
    std::u16string dayAbbreviation;
    std::u16string startTimeLabel;
};

// Formats already localized/formatted schedule labels without depending on Qt.
// Days retain source order inside each exact start-time group, and a day may
// appear once in each distinct group.
[[nodiscard]] inline std::u16string projectSpeakingEvaluationScheduleLabel(
    const std::vector<SpeakingEvaluationScheduleLabel>& labels
    )
{
    struct Group final
    {
        std::u16string startTimeLabel;
        std::vector<std::u16string> days;
    };

    std::vector<Group> groups;
    for (const SpeakingEvaluationScheduleLabel& label : labels)
    {
        auto group = std::find_if(
            groups.begin(),
            groups.end(),
            [&label](const Group& candidate)
            {
                return candidate.startTimeLabel == label.startTimeLabel;
            }
            );
        if (group == groups.end())
        {
            groups.push_back(Group{.startTimeLabel = label.startTimeLabel});
            group = std::prev(groups.end());
        }

        if (!label.dayAbbreviation.empty()
            && std::find(
                group->days.begin(),
                group->days.end(),
                label.dayAbbreviation
                ) == group->days.end())
        {
            group->days.push_back(label.dayAbbreviation);
        }
    }

    std::vector<std::u16string> groupLabels;
    groupLabels.reserve(groups.size());
    for (const Group& group : groups)
    {
        std::u16string days;
        for (const std::u16string& day : group.days)
        {
            days += day;
        }

        if (days.empty())
        {
            if (!group.startTimeLabel.empty())
            {
                groupLabels.push_back(group.startTimeLabel);
            }
        }
        else if (group.startTimeLabel.empty())
        {
            groupLabels.push_back(std::move(days));
        }
        else
        {
            groupLabels.push_back(
                std::move(days) + u" - " + group.startTimeLabel
                );
        }
    }

    std::u16string result;
    for (const std::u16string& groupLabel : groupLabels)
    {
        if (!result.empty())
        {
            result += u", ";
        }
        result += groupLabel;
    }
    return result;
}

} // namespace ClassMngr::Next::Application
