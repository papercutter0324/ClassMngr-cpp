#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class ScheduleReportEntryKind
{
    RegularClass,
    TestingClass
};

enum class ScheduleReportTestingAssignmentKind
{
    PlainTesting,
    SpecialClass
};

enum class ScheduleReportRowFilter
{
    None,
    TrimEmptyOuterRows
};

enum class ScheduleReportDisplayMode
{
    Regular,
    Intensive,
    Testing
};

struct ScheduleReportEntry
{
    int classId = -1;
    ScheduleReportEntryKind kind = ScheduleReportEntryKind::RegularClass;
    std::string className;
    std::string teacherKr;
    std::string teacherEn;
    std::string teacherPreferredName;
    std::string roomNumber;
    std::string classGrade;
    std::string classLevel;
    std::string classColor = "#FFFFFF";
    std::string fontColor = "#000000";
};

struct ScheduleReportRow
{
    std::string label;
};

using ScheduleReportSlots =
    std::map<std::string, std::map<std::string, std::vector<ScheduleReportEntry>>>;

struct ScheduleReportBuildResult
{
    std::vector<std::string> days;
    std::vector<ScheduleReportRow> rows;
    ScheduleReportSlots schedule;
    int scheduleOffset = 0;
    bool uses55Endings = false;
};

struct ScheduleReportTestingAssignment
{
    std::string day;
    std::string startTime;
    ScheduleReportTestingAssignmentKind kind =
        ScheduleReportTestingAssignmentKind::PlainTesting;
    std::string room;
    int classId = -1;
};

struct ScheduleReportTestingAssignmentView
{
    ScheduleReportTestingAssignment assignment;
    ScheduleReportEntry testingClassEntry;
};

struct ScheduleReportRequest
{
    std::vector<std::string> days;
    std::map<std::string, std::string> slotStateOverrides;
    std::map<std::string, ScheduleReportTestingAssignmentView>
        testingAssignments;
    bool use24h = false;
    ScheduleReportDisplayMode displayMode = ScheduleReportDisplayMode::Regular;
    bool testingAffectsM1 = false;
    bool regularWeekdaySlotTogglingEnabled = false;
    ScheduleReportRowFilter rowFilter = ScheduleReportRowFilter::None;
};

struct ScheduleReportCell
{
    std::string day;
    std::string timeLabel;
    std::vector<ScheduleReportEntry> entries;
    std::string defaultSlotState;
    std::string slotState;
    std::string testingRoom;
    bool testingClassAssignment = false;
    int testingClassId = -1;
    bool slotTogglingEnabled = false;
    bool testingBlockCreationEnabled = false;
};

struct ScheduleReportRowView
{
    std::string timeLabel;
    std::string timeRangeLabel;
    std::vector<ScheduleReportCell> cells;
    int maxEntryCount = 1;
};

struct ScheduleReportSummary
{
    int essayBlocks = 0;
    int testingBlocks = 0;
    int testingClassBlocks = 0;
    int scheduledBlocks = 0;
};

struct ScheduleReportModel
{
    std::vector<std::string> days;
    std::vector<ScheduleReportRowView> rows;
    bool uses55Endings = false;
    ScheduleReportSummary summary;
};

class ScheduleReportService final
{
public:
    [[nodiscard]] static std::string emptySlotState();

    [[nodiscard]] static std::string essaySlotState();

    [[nodiscard]] static std::string lunchSlotState();

    [[nodiscard]] static std::string testingSlotState();

    [[nodiscard]] static bool modeUsesIntensiveTimes(
        ScheduleReportDisplayMode mode
        );

    [[nodiscard]] static std::string nextSlotState(
        std::string_view currentState
        );

    [[nodiscard]] static std::string slotKey(
        std::string_view day,
        std::string_view timeLabel
        );

    [[nodiscard]] static std::vector<std::string> visibleDays(
        bool includeWeekends
        );

    [[nodiscard]] static bool isWeekendDay(
        std::string_view day
        );

    [[nodiscard]] static std::string defaultSlotState(
        std::string_view day,
        std::string_view timeLabel,
        bool useIntensive
        );

    [[nodiscard]] static bool slotTogglingEnabled(
        std::string_view day,
        bool useIntensive,
        bool regularWeekdaySlotTogglingEnabled
        );

    [[nodiscard]] static std::string slotState(
        std::string_view day,
        std::string_view timeLabel,
        std::string_view defaultState,
        const std::map<std::string, std::string>& overrides
        );

    [[nodiscard]] static std::string displayTime(
        std::string_view timeLabel,
        bool use24h
        );

    [[nodiscard]] static std::string rangeLabel(
        std::string_view startLabel,
        bool uses55Endings,
        bool use24h
        );

    [[nodiscard]] static std::string classLine(
        std::string_view classGrade,
        std::string_view classLevel,
        bool compact = false
        );

    [[nodiscard]] static std::string excelDayLabel(
        std::string_view day
        );

    [[nodiscard]] static std::string excelTimeLabel(
        std::string_view rangeLabel
        );

    [[nodiscard]] static std::string teacherName(
        const ScheduleReportEntry& entry,
        bool showEnglishName
        );

    [[nodiscard]] static std::string teacherRoomLine(
        const ScheduleReportEntry& entry,
        bool showEnglishName
        );

    [[nodiscard]] static ScheduleReportModel build(
        const ScheduleReportBuildResult& result,
        const ScheduleReportRequest& request
        );
};

} // namespace classmngr::engine
