#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kMyClassesPageMaximumClasses = 4'096;
inline constexpr std::size_t kMyClassesPageMaximumScheduleRows = 14;
inline constexpr std::size_t kMyClassesPageMaximumScheduleTokenLength = 64;
inline constexpr std::size_t kMyClassesPageMaximumTextLength = 4'096;

struct MyClassesPageScheduleEntry final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;

    friend bool operator==(const MyClassesPageScheduleEntry&, const MyClassesPageScheduleEntry&) = default;
};

// Navigation owns only the fields needed to group, order, and label each
// class. Roster cells and teacher facilities/notes are loaded for selection.
struct MyClassesPageClassSummary final
{
    Domain::ClassId classId;
    std::u16string className;
    std::u16string grade;
    std::u16string level;
    std::vector<MyClassesPageScheduleEntry> regularSchedule;
    std::vector<MyClassesPageScheduleEntry> intensiveSchedule;
    std::optional<Domain::TeacherId> teacherId;
    std::u16string teacherEnglishName;
    std::u16string teacherKoreanName;

    friend bool operator==(const MyClassesPageClassSummary&, const MyClassesPageClassSummary&) = default;
};

using MyClassesPageSummarySnapshot = std::vector<MyClassesPageClassSummary>;
using MyClassesPageSummaryResult = Domain::Result<MyClassesPageSummarySnapshot>;

struct MyClassesPageTeacherDetails final
{
    std::u16string koreanName;
    std::u16string englishName;
    std::u16string preferredRomanization;
    std::u16string preferredName;
    std::u16string roomNumber;
    std::u16string internetType;
    std::u16string wifiName;
    std::u16string wifiPassword;
    std::u16string projectionType;
    std::u16string zoomId;
    std::u16string zoomPassword;
    std::u16string notes;

    friend bool operator==(const MyClassesPageTeacherDetails&, const MyClassesPageTeacherDetails&) = default;
};

// One selected-class record. This DTO is returned only for the active ID;
// it contains no other class, roster, or widget state.
struct MyClassesPageSelectedDetails final
{
    Domain::ClassId classId;
    std::u16string className;
    std::u16string grade;
    std::u16string level;
    std::vector<MyClassesPageScheduleEntry> regularSchedule;
    std::vector<MyClassesPageScheduleEntry> intensiveSchedule;
    std::optional<int> studentCount;
    std::u16string classNotes;
    std::u16string timeFillerActivities;
    std::optional<Domain::TeacherId> teacherId;
    std::optional<MyClassesPageTeacherDetails> teacher;

    friend bool operator==(const MyClassesPageSelectedDetails&, const MyClassesPageSelectedDetails&) = default;
};

using MyClassesPageSelectedDetailsResult =
    Domain::Result<MyClassesPageSelectedDetails>;

class MyClassesPageReadPort
{
public:
    virtual ~MyClassesPageReadPort() = default;

    [[nodiscard]] virtual MyClassesPageSummaryResult readSummaries() const = 0;
    [[nodiscard]] virtual MyClassesPageSelectedDetailsResult readSelectedDetails(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
