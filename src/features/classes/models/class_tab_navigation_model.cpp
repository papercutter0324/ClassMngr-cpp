#include "class_tab_navigation_model.h"

#include "classmngr/engine/class_tab_navigation.h"

#include <QObject>

#include <string>
#include <vector>

namespace
{
using EngineService = classmngr::engine::ClassTabNavigationService;

std::string toEngineString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString toQtString(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::ClassTime toEngineClassTime(const ClassTime& value)
{
    classmngr::engine::ClassTime result;
    result.day = toEngineString(value.day);
    result.startTime = toEngineString(value.startTime);
    result.endTime = toEngineString(value.endTime);
    return result;
}

std::vector<classmngr::engine::ClassTime> toEngineClassTimes(
    const QList<ClassTime>& values
    )
{
    std::vector<classmngr::engine::ClassTime> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (const ClassTime& value : values)
    {
        result.push_back(toEngineClassTime(value));
    }
    return result;
}

EngineService::ClassEntry toEngineClassEntry(
    const ClassTabNavigation::ClassEntry& value
    )
{
    EngineService::ClassEntry result;
    result.classId = value.classId;
    result.classroomName = toEngineString(value.classroomName);
    result.grade = toEngineString(value.grade);
    result.level = toEngineString(value.level);
    result.regularTimes = toEngineClassTimes(value.regularTimes);
    result.intensiveTimes = toEngineClassTimes(value.intensiveTimes);
    result.teacherEn = toEngineString(value.teacherEn);
    result.teacherKr = toEngineString(value.teacherKr);
    return result;
}

std::vector<EngineService::ClassEntry> toEngineEntries(
    const QList<ClassTabNavigation::ClassEntry>& values
    )
{
    std::vector<EngineService::ClassEntry> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (const ClassTabNavigation::ClassEntry& value : values)
    {
        result.push_back(toEngineClassEntry(value));
    }
    return result;
}

EngineService::GroupingPolicy toEngineGroupingPolicy(
    ClassTabNavigation::GroupingPolicy value
    )
{
    return value == ClassTabNavigation::GroupingPolicy::AlwaysGradeGrouped
        ? EngineService::GroupingPolicy::AlwaysGradeGrouped
        : EngineService::GroupingPolicy::Adaptive;
}

EngineService::DayFilter toEngineDayFilter(
    const ClassTabNavigation::DayFilter& value
    )
{
    EngineService::DayFilter result;
    result.selectedDays.reserve(
        static_cast<std::size_t>(value.selectedDays.size())
        );
    for (const QString& day : value.selectedDays)
    {
        result.selectedDays.push_back(toEngineString(day));
    }

    result.scheduleSource =
        value.scheduleSource == ClassTabNavigation::ScheduleSource::Intensive
        ? EngineService::ScheduleSource::Intensive
        : EngineService::ScheduleSource::Regular;
    result.visibilityScope =
        value.visibilityScope
            == ClassTabNavigation::VisibilityScope::ActiveSchedule
        ? EngineService::VisibilityScope::ActiveSchedule
        : EngineService::VisibilityScope::AllClasses;
    return result;
}

EngineService::Labels translatedLabels()
{
    EngineService::Labels result;
    result.other = toEngineString(QObject::tr("Other"));
    result.intensive = toEngineString(QObject::tr("Int"));
    result.noTime = toEngineString(QObject::tr("No time"));
    result.classFallback = toEngineString(QObject::tr("Class %1"));
    return result;
}

ClassTabNavigation::ClassTab toQtClassTab(
    const EngineService::ClassTab& value
    )
{
    ClassTabNavigation::ClassTab result;
    result.classId = value.classId;
    result.label = toQtString(value.label);
    return result;
}

ClassTabNavigation::GradeGroup toQtGradeGroup(
    const EngineService::GradeGroup& value
    )
{
    ClassTabNavigation::GradeGroup result;
    result.grade = toQtString(value.grade);
    result.label = toQtString(value.label);
    for (const EngineService::ClassTab& classTab : value.classes)
    {
        result.classes.append(toQtClassTab(classTab));
    }
    return result;
}

ClassTabNavigation::Model toQtModel(const EngineService::Model& value)
{
    ClassTabNavigation::Model result;
    result.mode = value.mode == EngineService::Mode::GradeGrouped
        ? ClassTabNavigation::Mode::GradeGrouped
        : ClassTabNavigation::Mode::Flat;

    for (const EngineService::ClassTab& classTab : value.allClasses)
    {
        result.allClasses.append(toQtClassTab(classTab));
    }
    for (const EngineService::ClassTab& classTab : value.flatClasses)
    {
        result.flatClasses.append(toQtClassTab(classTab));
    }
    for (const EngineService::GradeGroup& gradeGroup : value.gradeGroups)
    {
        result.gradeGroups.append(toQtGradeGroup(gradeGroup));
    }
    return result;
}
} // namespace

namespace ClassTabNavigation
{

Model build(
    const QList<ClassEntry>& entries,
    GroupingPolicy groupingPolicy,
    const DayFilter& dayFilter
    )
{
    const EngineService::Labels labels = translatedLabels();
    const EngineService::Model model = EngineService::build(
        toEngineEntries(entries),
        toEngineGroupingPolicy(groupingPolicy),
        toEngineDayFilter(dayFilter),
        labels
        );
    return toQtModel(model);
}

} // namespace ClassTabNavigation
