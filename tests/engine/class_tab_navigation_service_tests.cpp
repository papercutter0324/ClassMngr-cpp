#include "classmngr/engine/class_tab_navigation.h"

#include <iostream>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

using classmngr::engine::ClassTabNavigationService;
using classmngr::engine::ClassTime;

namespace
{
bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineClassTabNavigationServiceTests: "
              << message << '\n';
    return false;
}

ClassTime classTime(
    std::string day,
    std::string startTime
    )
{
    ClassTime time;
    time.day = std::move(day);
    time.startTime = std::move(startTime);
    return time;
}

ClassTabNavigationService::ClassEntry classEntry(
    int classId,
    std::string grade,
    std::string level,
    std::vector<ClassTime> regularTimes = {},
    std::vector<ClassTime> intensiveTimes = {},
    std::string teacherEn = {}
    )
{
    ClassTabNavigationService::ClassEntry entry;
    entry.classId = classId;
    entry.grade = std::move(grade);
    entry.level = std::move(level);
    entry.regularTimes = std::move(regularTimes);
    entry.intensiveTimes = std::move(intensiveTimes);
    entry.teacherEn = std::move(teacherEn);
    return entry;
}

std::vector<int> visibleClassIds(
    const ClassTabNavigationService::Model& model
    )
{
    std::vector<int> result;

    for (const ClassTabNavigationService::ClassTab& tab : model.flatClasses)
    {
        result.push_back(tab.classId);
    }

    for (const ClassTabNavigationService::GradeGroup& group : model.gradeGroups)
    {
        for (const ClassTabNavigationService::ClassTab& tab : group.classes)
        {
            result.push_back(tab.classId);
        }
    }

    return result;
}

bool equals(
    const std::vector<int>& left,
    std::initializer_list<int> right
    )
{
    return left == std::vector<int>(right);
}
} // namespace

int main()
{
    bool passed = true;

    const ClassTabNavigationService::Model flatModel =
        ClassTabNavigationService::build(
            {
                classEntry(
                    1,
                    "E4",
                    "Perseus",
                    {
                        classTime("Monday", "4:00 PM"),
                        classTime("Wednesday", "4:00 PM"),
                        classTime("Friday", "4:00 PM")
                    }
                    ),
                classEntry(
                    2,
                    "M1",
                    "Solis",
                    {
                        classTime("Tuesday", "5:00 PM"),
                        classTime("Thursday", "5:00 PM")
                    }
                    )
            }
            );
    passed &= expect(
        flatModel.mode == ClassTabNavigationService::Mode::Flat,
        "six or fewer classes should use flat tabs"
        );
    passed &= expect(
        flatModel.flatClasses.size() == 2
            && flatModel.flatClasses[0].label
                == "E4 Perseus \xE2\x80\xA2 M/W/F 4:00"
            && flatModel.flatClasses[1].label
                == "M1 Solis \xE2\x80\xA2 T/Th 5:00",
        "flat labels should preserve grade, level, and schedule formatting"
        );

    const ClassTabNavigationService::Model groupedModel =
        ClassTabNavigationService::build(
            {
                classEntry(1, "M1", "Solis"),
                classEntry(2, "E4", "Perseus"),
                classEntry(3, "", "Custom")
            },
            ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped
            );
    passed &= expect(
        groupedModel.mode == ClassTabNavigationService::Mode::GradeGrouped
            && groupedModel.gradeGroups.size() == 3
            && groupedModel.gradeGroups[0].label == "E4"
            && groupedModel.gradeGroups[1].label == "M1"
            && groupedModel.gradeGroups[2].label == "Other"
            && equals(visibleClassIds(groupedModel), {2, 1, 3}),
        "forced grouping should order known grades before Other"
        );

    std::vector<ClassTabNavigationService::ClassEntry> moreThanSix;
    moreThanSix.emplace_back(classEntry(1, "M2", "Ursa"));
    moreThanSix.emplace_back(classEntry(2, "E5", "Apollo"));
    moreThanSix.emplace_back(classEntry(3, "E4", "Theseus"));
    moreThanSix.emplace_back(classEntry(4, "M1", "Major"));
    moreThanSix.emplace_back(classEntry(5, "", "Custom"));
    moreThanSix.emplace_back(classEntry(6, "E6", "Gaia"));
    moreThanSix.emplace_back(classEntry(7, "M3", "Song's"));
    const ClassTabNavigationService::Model adaptiveGrouped =
        ClassTabNavigationService::build(moreThanSix);
    passed &= expect(
        adaptiveGrouped.mode == ClassTabNavigationService::Mode::GradeGrouped
            && adaptiveGrouped.gradeGroups.size() == 7
            && adaptiveGrouped.gradeGroups[0].label == "E4"
            && adaptiveGrouped.gradeGroups[1].label == "E5"
            && adaptiveGrouped.gradeGroups[2].label == "E6"
            && adaptiveGrouped.gradeGroups[3].label == "M1"
            && adaptiveGrouped.gradeGroups[4].label == "M2"
            && adaptiveGrouped.gradeGroups[5].label == "M3"
            && adaptiveGrouped.gradeGroups[6].label == "Other",
        "more than six classes should use ordered grade groups"
        );

    std::vector<ClassTabNavigationService::ClassEntry> scheduleEntries{
        classEntry(
            1,
            "E4",
            "Perseus",
            {
                classTime("Monday", "4:00 PM"),
                classTime("Wednesday", "4:00 PM"),
                classTime("Friday", "4:00 PM")
            }
            ),
        classEntry(
            2,
            "E4",
            "Odysseus",
            {},
            {
                classTime("Tuesday", "10:00 AM"),
                classTime("Thursday", "10:00 AM")
            }
            ),
        classEntry(3, "E4", "Hercules")
    };
    for (int classId = 4; classId <= 7; ++classId)
    {
        scheduleEntries.emplace_back(
            classEntry(classId, "M1", "Major")
            );
    }
    const ClassTabNavigationService::Model scheduleModel =
        ClassTabNavigationService::build(scheduleEntries);
    passed &= expect(
        scheduleModel.gradeGroups[0].classes.size() == 3
            && scheduleModel.gradeGroups[0].classes[0].label
                == "Perseus \xE2\x80\xA2 M/W/F 4:00"
            && scheduleModel.gradeGroups[0].classes[1].label
                == "Odysseus \xE2\x80\xA2 Int T/Th 10:00"
            && scheduleModel.gradeGroups[0].classes[2].label
                == "Hercules \xE2\x80\xA2 No time",
        "grade-group labels should include regular, intensive, and missing schedules"
        );

    const ClassTabNavigationService::Model sortedModel =
        ClassTabNavigationService::build(
            {
                classEntry(
                    1,
                    "E4",
                    "Perseus",
                    {classTime("Friday", "4:00 PM")}
                    ),
                classEntry(
                    2,
                    "E4",
                    "Perseus",
                    {classTime("Monday", "4:00 PM")}
                    ),
                classEntry(
                    3,
                    "E4",
                    "Theseus",
                    {classTime("Sunday", "4:00 PM")}
                    ),
                classEntry(
                    4,
                    "E4",
                    "Perseus",
                    {classTime("Sunday", "4:00 PM")}
                    ),
                classEntry(5, "M1", "Major"),
                classEntry(6, "M1", "Major"),
                classEntry(7, "M1", "Major"),
                classEntry(8, "M1", "Major")
            }
            );
    passed &= expect(
        equals(
            {
                sortedModel.gradeGroups[0].classes[0].classId,
                sortedModel.gradeGroups[0].classes[1].classId,
                sortedModel.gradeGroups[0].classes[2].classId,
                sortedModel.gradeGroups[0].classes[3].classId
            },
            {3, 2, 1, 4}
            ),
        "classes should sort by level, then day, then time"
        );

    const ClassTabNavigationService::Model duplicateModel =
        ClassTabNavigationService::build(
            {
                classEntry(
                    1,
                    "E4",
                    "Perseus",
                    {classTime("Monday", "4:00 PM")},
                    {},
                    "Alice"
                    ),
                classEntry(
                    2,
                    "E4",
                    "Perseus",
                    {classTime("Monday", "4:00 PM")},
                    {},
                    "Bob"
                    ),
                classEntry(
                    3,
                    "E4",
                    "Perseus",
                    {classTime("Monday", "4:00 PM")},
                    {},
                    "Bob"
                    )
            }
            );
    passed &= expect(
        duplicateModel.flatClasses.size() == 3
            && duplicateModel.flatClasses[0].label
                == "E4 Perseus \xE2\x80\xA2 M 4:00 \xE2\x80\xA2 Alice"
            && duplicateModel.flatClasses[1].label
                == "E4 Perseus \xE2\x80\xA2 M 4:00 \xE2\x80\xA2 Bob #2"
            && duplicateModel.flatClasses[2].label
                == "E4 Perseus \xE2\x80\xA2 M 4:00 \xE2\x80\xA2 Bob #3",
        "duplicate labels should use teacher names and class ids"
        );

    const std::vector<ClassTabNavigationService::ClassEntry> filterEntries{
        classEntry(
            1,
            "E4",
            "Perseus",
            {
                classTime("Monday", "4:00 PM"),
                classTime("Friday", "4:00 PM")
            },
            {classTime("Saturday", "10:00 AM")}
            ),
        classEntry(
            2,
            "E4",
            "Theseus",
            {classTime("Wednesday", "4:00 PM")},
            {classTime("Sunday", "11:00 AM")}
            ),
        classEntry(3, "E5", "Apollo")
    };
    ClassTabNavigationService::DayFilter dayFilter;
    dayFilter.selectedDays = {"Friday", "Wednesday"};
    const ClassTabNavigationService::Model selectedDaysModel =
        ClassTabNavigationService::build(
            filterEntries,
            ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            dayFilter
            );
    passed &= expect(
        equals(visibleClassIds(selectedDaysModel), {2, 1}),
        "day filters should match any selected regular day"
        );

    dayFilter.selectedDays = {"Wkend"};
    dayFilter.scheduleSource =
        ClassTabNavigationService::ScheduleSource::Intensive;
    const ClassTabNavigationService::Model weekendModel =
        ClassTabNavigationService::build(
            filterEntries,
            ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            dayFilter
            );
    const std::vector<int> weekendIds = visibleClassIds(weekendModel);
    passed &= expect(
        equals(weekendIds, {2, 1}),
        "weekend filter should match Saturday and Sunday"
        );

    dayFilter.selectedDays = {"Friday"};
    dayFilter.scheduleSource =
        ClassTabNavigationService::ScheduleSource::Intensive;
    const ClassTabNavigationService::Model intensiveFilterModel =
        ClassTabNavigationService::build(
            filterEntries,
            ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            dayFilter
            );
    passed &= expect(
        intensiveFilterModel.gradeGroups.empty(),
        "selected schedule source should control day filtering"
        );

    dayFilter.selectedDays.clear();
    dayFilter.visibilityScope =
        ClassTabNavigationService::VisibilityScope::ActiveSchedule;
    const ClassTabNavigationService::Model activeIntensiveModel =
        ClassTabNavigationService::build(
            filterEntries,
            ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            dayFilter
            );
    const std::vector<int> activeIntensiveIds =
        visibleClassIds(activeIntensiveModel);
    passed &= expect(
        equals(activeIntensiveIds, {2, 1}),
        "active schedule scope should exclude entries without the selected schedule"
        );

    ClassTabNavigationService::Labels customLabels;
    customLabels.other = "Other label";
    customLabels.intensive = "Intensive label";
    customLabels.noTime = "Missing label";
    customLabels.classFallback = "Room %1";
    const ClassTabNavigationService::Model customLabelModel =
        ClassTabNavigationService::build(
            {classEntry(42, "", "", {}, {classTime("Monday", "1:00 PM")})},
            ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            {},
            customLabels
            );
    passed &= expect(
        customLabelModel.gradeGroups[0].label == "Other label"
            && customLabelModel.gradeGroups[0].classes[0].label
                == "Room 42 \xE2\x80\xA2 Intensive label M 1:00",
        "portable callers should be able to supply localized labels"
        );

    return passed ? 0 : 1;
}
