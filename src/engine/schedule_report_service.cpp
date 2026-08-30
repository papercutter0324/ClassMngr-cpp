#include "classmngr/engine/schedule_report.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace classmngr::engine
{
namespace
{
constexpr int RegularEarlyEmptyFinalHour = 15;

std::string trimAscii(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(static_cast<unsigned char>(value[first])))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(static_cast<unsigned char>(value[last - 1])))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string collapseAsciiWhitespace(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;

    for (const unsigned char character : value)
    {
        if (std::isspace(character))
        {
            pendingSpace = !result.empty();
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(static_cast<char>(character));
    }

    if (!result.empty() && result.back() == ' ')
    {
        result.pop_back();
    }

    return result;
}

void replaceAll(
    std::string* value,
    std::string_view from,
    std::string_view to
    )
{
    if (!value || from.empty())
    {
        return;
    }

    std::size_t offset = 0;
    while ((offset = value->find(from, offset)) != std::string::npos)
    {
        value->replace(offset, from.size(), to);
        offset += to.size();
    }
}

std::string upperAscii(std::string_view value)
{
    std::string result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::toupper(character));
        }
        );
    return result;
}

std::optional<int> parseClock(std::string_view value)
{
    if (value.size() != 5 || value[2] != ':'
        || !std::isdigit(static_cast<unsigned char>(value[0]))
        || !std::isdigit(static_cast<unsigned char>(value[1]))
        || !std::isdigit(static_cast<unsigned char>(value[3]))
        || !std::isdigit(static_cast<unsigned char>(value[4])))
    {
        return std::nullopt;
    }

    const int hour =
        (value[0] - '0') * 10
        + (value[1] - '0');
    const int minute =
        (value[3] - '0') * 10
        + (value[4] - '0');
    if (hour > 23 || minute > 59)
    {
        return std::nullopt;
    }

    return hour * 60 + minute;
}

std::string twoDigits(int value)
{
    if (value < 10)
    {
        return "0" + std::to_string(value);
    }
    return std::to_string(value);
}

std::string clock24(int totalMinutes)
{
    totalMinutes = (totalMinutes % 1440 + 1440) % 1440;
    return twoDigits(totalMinutes / 60)
        + ":"
        + twoDigits(totalMinutes % 60);
}

std::string clock12(int totalMinutes)
{
    totalMinutes = (totalMinutes % 1440 + 1440) % 1440;
    const int hour24 = totalMinutes / 60;
    const int displayHour = hour24 % 12 == 0 ? 12 : hour24 % 12;
    return std::to_string(displayHour)
        + ":"
        + twoDigits(totalMinutes % 60)
        + (hour24 >= 12 ? " PM" : " AM");
}

const std::vector<ScheduleReportEntry>* entriesAt(
    const ScheduleReportBuildResult& result,
    std::string_view day,
    std::string_view timeLabel
    )
{
    const auto dayIterator = result.schedule.find(std::string(day));
    if (dayIterator == result.schedule.end())
    {
        return nullptr;
    }

    const auto slotIterator = dayIterator->second.find(
        std::string(timeLabel)
        );
    return slotIterator == dayIterator->second.end()
        ? nullptr
        : &slotIterator->second;
}

bool regularEarlyEmptySlot(std::string_view timeLabel)
{
    const std::optional<int> minutes = parseClock(timeLabel);
    return minutes.has_value()
        && *minutes / 60 <= RegularEarlyEmptyFinalHour;
}

bool testingSuppressesEntry(
    const ScheduleReportEntry& entry,
    bool testingAffectsM1
    )
{
    const std::string grade = upperAscii(trimAscii(entry.classGrade));
    return grade == "M2"
        || grade == "M3"
        || (testingAffectsM1 && grade == "M1");
}

bool rowHasVisibleContent(
    const ScheduleReportRow& scheduleRow,
    const ScheduleReportBuildResult& result,
    const ScheduleReportRequest& request,
    const std::vector<std::string>& days
    )
{
    const bool useIntensive =
        ScheduleReportService::modeUsesIntensiveTimes(request.displayMode);

    for (const std::string& day : days)
    {
        const auto* entries = entriesAt(result, day, scheduleRow.label);
        if (entries != nullptr && !entries->empty())
        {
            return true;
        }

        const std::string defaultState =
            ScheduleReportService::defaultSlotState(
                day,
                scheduleRow.label,
                useIntensive
                );
        const std::string state =
            ScheduleReportService::slotTogglingEnabled(
                day,
                useIntensive,
                request.regularWeekdaySlotTogglingEnabled
                )
                ? ScheduleReportService::slotState(
                    day,
                    scheduleRow.label,
                    defaultState,
                    request.slotStateOverrides
                    )
                : defaultState;

        if (state != ScheduleReportService::emptySlotState())
        {
            return true;
        }
    }

    return false;
}

std::vector<ScheduleReportRow> filteredRows(
    const ScheduleReportBuildResult& result,
    const ScheduleReportRequest& request,
    const std::vector<std::string>& days
    )
{
    if (!ScheduleReportService::modeUsesIntensiveTimes(request.displayMode)
        || request.rowFilter == ScheduleReportRowFilter::None)
    {
        return result.rows;
    }

    int firstVisibleRow = -1;
    int lastVisibleRow = -1;
    for (std::size_t rowIndex = 0; rowIndex < result.rows.size(); ++rowIndex)
    {
        if (rowHasVisibleContent(result.rows[rowIndex], result, request, days))
        {
            if (firstVisibleRow < 0)
            {
                firstVisibleRow = static_cast<int>(rowIndex);
            }
            lastVisibleRow = static_cast<int>(rowIndex);
        }
    }

    if (firstVisibleRow < 0)
    {
        return {};
    }

    return {
        result.rows.begin() + firstVisibleRow,
        result.rows.begin() + lastVisibleRow + 1
    };
}
} // namespace

std::string ScheduleReportService::emptySlotState()
{
    return "empty";
}

std::string ScheduleReportService::essaySlotState()
{
    return "essay";
}

std::string ScheduleReportService::lunchSlotState()
{
    return "lunch";
}

std::string ScheduleReportService::testingSlotState()
{
    return "testing";
}

bool ScheduleReportService::modeUsesIntensiveTimes(
    ScheduleReportDisplayMode mode
    )
{
    return mode == ScheduleReportDisplayMode::Intensive;
}

std::string ScheduleReportService::nextSlotState(
    std::string_view currentState
    )
{
    if (currentState == essaySlotState())
    {
        return lunchSlotState();
    }
    if (currentState == lunchSlotState())
    {
        return emptySlotState();
    }
    return essaySlotState();
}

std::string ScheduleReportService::slotKey(
    std::string_view day,
    std::string_view timeLabel
    )
{
    std::string result(day);
    result.push_back('\x1f');
    result.append(timeLabel);
    return result;
}

std::vector<std::string> ScheduleReportService::visibleDays(
    bool includeWeekends
    )
{
    std::vector<std::string> days{
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday"
    };
    if (includeWeekends)
    {
        days.emplace_back("Saturday");
        days.emplace_back("Sunday");
    }
    return days;
}

bool ScheduleReportService::isWeekendDay(std::string_view day)
{
    return day == "Saturday" || day == "Sunday";
}

std::string ScheduleReportService::defaultSlotState(
    std::string_view day,
    std::string_view timeLabel,
    bool useIntensive
    )
{
    if (isWeekendDay(day))
    {
        return emptySlotState();
    }
    if (useIntensive)
    {
        return essaySlotState();
    }
    return regularEarlyEmptySlot(timeLabel)
        ? emptySlotState()
        : essaySlotState();
}

bool ScheduleReportService::slotTogglingEnabled(
    std::string_view day,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    return useIntensive
        || isWeekendDay(day)
        || regularWeekdaySlotTogglingEnabled;
}

std::string ScheduleReportService::slotState(
    std::string_view day,
    std::string_view timeLabel,
    std::string_view defaultState,
    const std::map<std::string, std::string>& overrides
    )
{
    const auto iterator = overrides.find(slotKey(day, timeLabel));
    return iterator == overrides.end()
        ? std::string(defaultState)
        : iterator->second;
}

std::string ScheduleReportService::displayTime(
    std::string_view timeLabel,
    bool use24h
    )
{
    const std::optional<int> start = parseClock(timeLabel);
    if (!start.has_value())
    {
        return std::string(timeLabel);
    }
    return use24h ? clock24(*start) : clock12(*start);
}

std::string ScheduleReportService::rangeLabel(
    std::string_view startLabel,
    bool uses55Endings,
    bool use24h
    )
{
    const std::optional<int> start = parseClock(startLabel);
    if (!start.has_value())
    {
        return std::string(startLabel);
    }

    const int end = *start + (uses55Endings ? 55 : 50);
    if (use24h)
    {
        return clock24(*start) + " - " + clock24(end);
    }

    const std::string startDisplay = clock12(*start);
    const std::string endDisplay = clock12(end);
    const bool samePeriod =
        startDisplay.substr(startDisplay.size() - 2)
        == endDisplay.substr(endDisplay.size() - 2);
    if (samePeriod)
    {
        return startDisplay.substr(0, startDisplay.size() - 3)
            + " -\n"
            + endDisplay.substr(0, endDisplay.size() - 3)
            + " "
            + endDisplay.substr(endDisplay.size() - 2);
    }

    return startDisplay + "\n- " + endDisplay;
}

std::string ScheduleReportService::classLine(
    std::string_view classGrade,
    std::string_view classLevel,
    bool compact
    )
{
    const std::string grade = trimAscii(classGrade);
    const std::string level = trimAscii(classLevel);
    if (grade.empty())
    {
        return level;
    }
    if (level.empty())
    {
        return grade;
    }
    return grade + (compact ? "-" : " - ") + level;
}

std::string ScheduleReportService::excelDayLabel(std::string_view day)
{
    if (day == "Monday") return "\xEC\x9B\x94(MON)";
    if (day == "Tuesday") return "\xED\x99\x94(TUE)";
    if (day == "Wednesday") return "\xEC\x88\x98(WED)";
    if (day == "Thursday") return "\xEB\xAA\xA9(THU)";
    if (day == "Friday") return "\xEA\xB8\x88(FRI)";
    if (day == "Saturday") return "\xED\x86\xA0(SAT)";
    if (day == "Sunday") return "\xEC\x9D\xBC(SUN)";
    return std::string(day);
}

std::string ScheduleReportService::excelTimeLabel(
    std::string_view rangeLabel
    )
{
    std::string label(rangeLabel);
    replaceAll(&label, " AM", {});
    replaceAll(&label, " PM", {});
    replaceAll(&label, " -\n", "~");
    replaceAll(&label, "\n- ", "~");
    replaceAll(&label, " - ", "~");
    return label;
}

std::string ScheduleReportService::teacherName(
    const ScheduleReportEntry& entry,
    bool showEnglishName
    )
{
    const std::string preferred = trimAscii(
        showEnglishName ? entry.teacherPreferredName : entry.teacherKr
        );
    const std::string fallback = trimAscii(entry.teacherEn);
    if (!preferred.empty())
    {
        return preferred;
    }
    if (!fallback.empty())
    {
        return fallback;
    }
    return trimAscii(entry.teacherKr);
}

std::string ScheduleReportService::teacherRoomLine(
    const ScheduleReportEntry& entry,
    bool showEnglishName
    )
{
    return collapseAsciiWhitespace(
        teacherName(entry, showEnglishName) + " " + entry.roomNumber
        );
}

ScheduleReportModel ScheduleReportService::build(
    const ScheduleReportBuildResult& result,
    const ScheduleReportRequest& request
    )
{
    ScheduleReportModel model;
    model.days = request.days.empty() ? result.days : request.days;
    model.uses55Endings = result.uses55Endings;

    const bool useIntensive = modeUsesIntensiveTimes(request.displayMode);
    const std::vector<ScheduleReportRow> rows = filteredRows(
        result,
        request,
        model.days
        );

    model.rows.reserve(rows.size());
    for (const ScheduleReportRow& scheduleRow : rows)
    {
        ScheduleReportRowView row;
        row.timeLabel = scheduleRow.label;
        row.timeRangeLabel = rangeLabel(
            scheduleRow.label,
            result.uses55Endings,
            request.use24h
            );
        row.cells.reserve(model.days.size());

        for (const std::string& day : model.days)
        {
            ScheduleReportCell cell;
            cell.day = day;
            cell.timeLabel = scheduleRow.label;

            const auto* sourceEntries = entriesAt(
                result,
                day,
                scheduleRow.label
                );
            if (sourceEntries != nullptr)
            {
                cell.entries = *sourceEntries;
            }

            const auto assignment = request.testingAssignments.find(
                slotKey(day, scheduleRow.label)
                );
            const bool hasExplicitAssignment =
                request.displayMode == ScheduleReportDisplayMode::Testing
                && assignment != request.testingAssignments.end();
            bool removedAffectedEntry = false;

            if (request.displayMode == ScheduleReportDisplayMode::Testing
                && !hasExplicitAssignment)
            {
                cell.entries.erase(
                    std::remove_if(
                        cell.entries.begin(),
                        cell.entries.end(),
                        [&removedAffectedEntry, &request](
                            const ScheduleReportEntry& entry
                            )
                        {
                            if (!testingSuppressesEntry(
                                    entry,
                                    request.testingAffectsM1
                                    ))
                            {
                                return false;
                            }
                            removedAffectedEntry = true;
                            return true;
                        }
                        ),
                    cell.entries.end()
                    );
            }

            cell.defaultSlotState = defaultSlotState(
                day,
                scheduleRow.label,
                useIntensive
                );
            cell.slotTogglingEnabled = slotTogglingEnabled(
                day,
                useIntensive,
                request.regularWeekdaySlotTogglingEnabled
                );
            cell.slotState = cell.slotTogglingEnabled
                ? slotState(
                    day,
                    scheduleRow.label,
                    cell.defaultSlotState,
                    request.slotStateOverrides
                    )
                : cell.defaultSlotState;

            if (hasExplicitAssignment)
            {
                cell.entries.clear();
                if (assignment->second.assignment.kind
                    == ScheduleReportTestingAssignmentKind::SpecialClass)
                {
                    cell.entries.push_back(
                        assignment->second.testingClassEntry
                        );
                    cell.testingClassAssignment = true;
                    cell.testingClassId = assignment->second.assignment.classId;
                }
                else
                {
                    cell.slotState = testingSlotState();
                    cell.testingRoom = assignment->second.assignment.room;
                }
            }

            if (request.displayMode == ScheduleReportDisplayMode::Testing
                && cell.entries.empty()
                && !hasExplicitAssignment)
            {
                if (removedAffectedEntry)
                {
                    cell.slotState = essaySlotState();
                }
                cell.testingBlockCreationEnabled =
                    cell.slotState == essaySlotState();
            }

            if (!cell.entries.empty())
            {
                row.maxEntryCount = std::max(
                    row.maxEntryCount,
                    static_cast<int>(cell.entries.size())
                    );
                ++model.summary.scheduledBlocks;
                if (cell.testingClassAssignment)
                {
                    ++model.summary.testingClassBlocks;
                }
            }
            else if (cell.slotState == essaySlotState())
            {
                ++model.summary.essayBlocks;
                ++model.summary.scheduledBlocks;
            }
            else if (cell.slotState == testingSlotState())
            {
                ++model.summary.testingBlocks;
                ++model.summary.scheduledBlocks;
            }

            row.cells.push_back(std::move(cell));
        }

        model.rows.push_back(std::move(row));
    }

    return model;
}

} // namespace classmngr::engine
