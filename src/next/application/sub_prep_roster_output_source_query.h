#pragma once

#include "next/application/sub_prep_print_source_query.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kSubPrepRosterOutputMaxRosterColumns = 128;
inline constexpr std::size_t kSubPrepRosterOutputMaxRowsPerClass = 4'096;
inline constexpr std::size_t kSubPrepRosterOutputMaxTotalRows = 65'536;
inline constexpr std::size_t kSubPrepRosterOutputMaxTotalCells = 1'000'000;
inline constexpr std::size_t kSubPrepRosterOutputMaxCellBytes = 16'384;
inline constexpr std::size_t kSubPrepRosterOutputMaxTotalTextBytes =
    32 * 1024 * 1024;
inline constexpr std::size_t kSubPrepRosterOutputMaxClassNameBytes = 256;
inline constexpr std::size_t kSubPrepRosterOutputMaxColumnNameBytes = 256;

struct SubPrepRosterOutputSourceRequest final
{
    std::vector<Domain::ClassId> selectedClassIds;
    std::vector<SubPrepWeekday> selectedDays;
    ScheduleViewMode mode = ScheduleViewMode::Regular;
    std::vector<std::string> selectedExtraColumns;

    friend bool operator==(
        const SubPrepRosterOutputSourceRequest&,
        const SubPrepRosterOutputSourceRequest&
        ) = default;
};

struct SubPrepRosterOutputMeeting final
{
    SubPrepWeekday weekday = SubPrepWeekday::Monday;
    std::string startTime;
    std::string endTime;

    friend bool operator==(
        const SubPrepRosterOutputMeeting&,
        const SubPrepRosterOutputMeeting&
        ) = default;
};

struct SubPrepRosterOutputTeacher final
{
    Domain::TeacherId id;
    std::string englishName;
    std::string koreanName;
    std::string preferredName;
    std::string preferredRomanization;

    friend bool operator==(
        const SubPrepRosterOutputTeacher&,
        const SubPrepRosterOutputTeacher&
        ) = default;
};

struct SubPrepRosterOutputClass final
{
    Domain::ClassId id;
    std::optional<Domain::TeacherId> teacherId;
    std::string classroomName;
    std::string grade;
    std::string level;
    std::string classTeacherEnglishName;
    std::string classTeacherKoreanName;
    std::string room;
    std::string wifiName;
    std::string wifiPassword;
    std::string zoomId;
    std::string zoomPassword;
    std::vector<SubPrepRosterOutputMeeting> meetings;
    std::vector<std::string> rosterColumns;
    std::vector<std::vector<std::string>> rosterRows;

    friend bool operator==(
        const SubPrepRosterOutputClass&,
        const SubPrepRosterOutputClass&
        ) = default;
};

struct SubPrepRosterOutputSourceInput final
{
    std::vector<SubPrepRosterOutputTeacher> teachers;
    std::vector<SubPrepRosterOutputClass> classes;

    friend bool operator==(
        const SubPrepRosterOutputSourceInput&,
        const SubPrepRosterOutputSourceInput&
        ) = default;
};

using SubPrepRosterOutputSourceReadResult =
    Domain::Result<SubPrepRosterOutputSourceInput>;

// The read port returns owning, operation-scoped values for the selected
// class/day/mode scope. Roster columns are restricted to English, Korean, and
// the requested extra columns. Implementations must enforce the same finite
// limits before building these values so oversized legacy data is not first
// materialized as an unbounded roster.
class SubPrepRosterOutputSourceReadPort
{
public:
    virtual ~SubPrepRosterOutputSourceReadPort() = default;

    [[nodiscard]] virtual SubPrepRosterOutputSourceReadResult loadSource(
        const SubPrepRosterOutputSourceRequest& request
        ) = 0;
};

class SubPrepRosterOutputSource final
{
public:
    using Teacher = SubPrepRosterOutputTeacher;
    using Class = SubPrepRosterOutputClass;

    SubPrepRosterOutputSource() = default;

    [[nodiscard]] const std::vector<Teacher>& teachers() const noexcept
    {
        return m_teachers;
    }

    [[nodiscard]] const std::vector<Class>& classes() const noexcept
    {
        return m_classes;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_classes.empty();
    }

    friend bool operator==(
        const SubPrepRosterOutputSource&,
        const SubPrepRosterOutputSource&
        ) = default;

private:
    friend class SubPrepRosterOutputSourceQuery;

    SubPrepRosterOutputSource(
        std::vector<Teacher> teachers,
        std::vector<Class> classes
        )
        : m_teachers(std::move(teachers)),
          m_classes(std::move(classes))
    {
    }

    std::vector<Teacher> m_teachers;
    std::vector<Class> m_classes;
};

using SubPrepRosterOutputSourceQueryResult =
    Domain::Result<SubPrepRosterOutputSource>;

namespace SubPrepRosterOutputSourceQueryDetail
{

[[nodiscard]] inline bool isBlank(const std::string_view value) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline bool isOptionalText(
    const std::string& value,
    const std::size_t maxBytes
    ) noexcept
{
    return value.empty() || value.size() <= maxBytes;
}

template <typename TypedId>
[[nodiscard]] inline bool isValidId(const TypedId& id) noexcept
{
    return !isBlank(id.value())
        && id.value().size() <= kSubPrepPrintSourceMaxIdentifierLength;
}

[[nodiscard]] inline bool isValidWeekday(
    const SubPrepWeekday weekday
    ) noexcept
{
    const auto value = static_cast<std::uint8_t>(weekday);
    return value >= static_cast<std::uint8_t>(SubPrepWeekday::Monday)
        && value <= static_cast<std::uint8_t>(SubPrepWeekday::Sunday);
}

[[nodiscard]] inline bool isValidMode(const ScheduleViewMode mode) noexcept
{
    return mode == ScheduleViewMode::Regular
        || mode == ScheduleViewMode::Intensive;
}

[[nodiscard]] inline Domain::OperationError invalidInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

[[nodiscard]] inline Domain::OperationError validationError(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::Validation,
        .message = message,
        .recoverable = false
    };
}

[[nodiscard]] inline Domain::Result<void> validateRequest(
    const SubPrepRosterOutputSourceRequest& request
    )
{
    if (request.selectedClassIds.size() > kSubPrepPrintSourceMaxClassIds)
    {
        return Domain::Result<void>::failure(
            invalidInput("Selected roster-output class scope exceeds its limit.")
            );
    }

    std::set<Domain::ClassId> selectedClassIds;
    for (const auto& classId : request.selectedClassIds)
    {
        if (!isValidId(classId))
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Selected roster-output class identifiers must be non-blank and bounded."
                    )
                );
        }
        if (!selectedClassIds.insert(classId).second)
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Selected roster-output class identifiers must be unique."
                    )
                );
        }
    }

    if (request.selectedDays.size() > kSubPrepPrintSourceMaxSelectedDays)
    {
        return Domain::Result<void>::failure(
            invalidInput("Selected roster-output days exceed the weekly limit.")
            );
    }

    std::set<SubPrepWeekday> selectedDays;
    for (const auto day : request.selectedDays)
    {
        if (!isValidWeekday(day))
        {
            return Domain::Result<void>::failure(
                invalidInput("A selected roster-output weekday is invalid.")
                );
        }
        if (!selectedDays.insert(day).second)
        {
            return Domain::Result<void>::failure(
                invalidInput("Selected roster-output weekdays must be unique.")
                );
        }
    }

    if (!isValidMode(request.mode))
    {
        return Domain::Result<void>::failure(
            invalidInput("The roster-output schedule mode is invalid.")
            );
    }

    if (request.selectedExtraColumns.size()
        > kSubPrepRosterOutputMaxRosterColumns)
    {
        return Domain::Result<void>::failure(
            invalidInput("Selected roster-output columns exceed their limit.")
            );
    }

    std::set<std::string> selectedColumns;
    for (const auto& column : request.selectedExtraColumns)
    {
        if (isBlank(column)
            || column.size() > kSubPrepRosterOutputMaxColumnNameBytes)
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Selected roster-output column names must be non-blank and bounded."
                    )
                );
        }
        if (!selectedColumns.insert(column).second)
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Selected roster-output column names must be unique."
                    )
                );
        }
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline bool addTextBytes(
    const std::string& value,
    std::size_t* totalBytes
    ) noexcept
{
    if (value.size() > kSubPrepRosterOutputMaxCellBytes
        || *totalBytes > kSubPrepRosterOutputMaxTotalTextBytes - value.size())
    {
        return false;
    }

    *totalBytes += value.size();
    return true;
}

[[nodiscard]] inline bool isTeacherTextValid(
    const SubPrepRosterOutputTeacher& teacher
    ) noexcept
{
    return isOptionalText(
               teacher.englishName,
               kSubPrepPrintSourceMaxEnglishNameLength
               )
        && isOptionalText(
            teacher.koreanName,
            kSubPrepPrintSourceMaxKoreanNameLength
            )
        && isOptionalText(
            teacher.preferredName,
            kSubPrepPrintSourceMaxPreferredNameLength
            )
        && isOptionalText(
            teacher.preferredRomanization,
            kSubPrepPrintSourceMaxPreferredRomanizationLength
            );
}

[[nodiscard]] inline bool isClassTextValid(
    const SubPrepRosterOutputClass& classRecord
    ) noexcept
{
    return isOptionalText(
               classRecord.classroomName,
               kSubPrepRosterOutputMaxClassNameBytes
               )
        && isOptionalText(
            classRecord.grade,
            kSubPrepPrintSourceMaxGradeLength
            )
        && isOptionalText(
            classRecord.level,
            kSubPrepPrintSourceMaxLevelLength
            )
        && isOptionalText(
            classRecord.classTeacherEnglishName,
            kSubPrepPrintSourceMaxEnglishNameLength
            )
        && isOptionalText(
            classRecord.classTeacherKoreanName,
            kSubPrepPrintSourceMaxKoreanNameLength
            )
        && isOptionalText(classRecord.room, kSubPrepPrintSourceMaxRoomLength)
        && isOptionalText(
            classRecord.wifiName,
            kSubPrepPrintSourceMaxWifiNameLength
            )
        && isOptionalText(
            classRecord.wifiPassword,
            kSubPrepPrintSourceMaxWifiPasswordLength
            )
        && isOptionalText(classRecord.zoomId, kSubPrepPrintSourceMaxZoomIdLength)
        && isOptionalText(
            classRecord.zoomPassword,
            kSubPrepPrintSourceMaxZoomPasswordLength
            );
}

[[nodiscard]] inline bool addClassText(
    const SubPrepRosterOutputClass& classRecord,
    std::size_t* totalBytes
    ) noexcept
{
    return addTextBytes(classRecord.classroomName, totalBytes)
        && addTextBytes(classRecord.grade, totalBytes)
        && addTextBytes(classRecord.level, totalBytes)
        && addTextBytes(classRecord.classTeacherEnglishName, totalBytes)
        && addTextBytes(classRecord.classTeacherKoreanName, totalBytes)
        && addTextBytes(classRecord.room, totalBytes)
        && addTextBytes(classRecord.wifiName, totalBytes)
        && addTextBytes(classRecord.wifiPassword, totalBytes)
        && addTextBytes(classRecord.zoomId, totalBytes)
        && addTextBytes(classRecord.zoomPassword, totalBytes);
}

[[nodiscard]] inline Domain::Result<void> validateSource(
    const SubPrepRosterOutputSourceRequest& request,
    const SubPrepRosterOutputSourceInput& input
    )
{
    if (input.teachers.size() > kSubPrepPrintSourceMaxTeachers)
    {
        return Domain::Result<void>::failure(
            validationError("Roster-output teacher collection exceeds its limit.")
            );
    }
    if (input.classes.size() > kSubPrepPrintSourceMaxClasses)
    {
        return Domain::Result<void>::failure(
            validationError("Roster-output class collection exceeds its limit.")
            );
    }

    std::size_t totalTextBytes = 0;
    std::set<Domain::TeacherId> teacherIds;
    for (const auto& teacher : input.teachers)
    {
        if (!isValidId(teacher.id))
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output teacher identifier is malformed.")
                );
        }
        if (!teacherIds.insert(teacher.id).second)
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output teacher identifiers must be unique.")
                );
        }
        if (!isTeacherTextValid(teacher))
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output teacher field exceeds its limit.")
                );
        }
        if (!addTextBytes(teacher.englishName, &totalTextBytes)
            || !addTextBytes(teacher.koreanName, &totalTextBytes)
            || !addTextBytes(teacher.preferredName, &totalTextBytes)
            || !addTextBytes(teacher.preferredRomanization, &totalTextBytes))
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output text exceeds its total byte limit.")
                );
        }
    }

    const std::set<Domain::ClassId> selectedClassIds(
        request.selectedClassIds.cbegin(),
        request.selectedClassIds.cend()
        );
    const std::set<SubPrepWeekday> selectedDays(
        request.selectedDays.cbegin(),
        request.selectedDays.cend()
        );
    std::set<std::string> allowedRosterColumns{
        "English",
        "Korean"
    };
    allowedRosterColumns.insert(
        request.selectedExtraColumns.cbegin(),
        request.selectedExtraColumns.cend()
        );

    std::set<Domain::ClassId> classIds;
    std::set<Domain::TeacherId> referencedTeacherIds;
    std::size_t totalMeetings = 0;
    std::size_t totalRows = 0;
    std::size_t totalCells = 0;
    for (const auto& classRecord : input.classes)
    {
        if (!isValidId(classRecord.id))
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output class identifier is malformed.")
                );
        }
        if (!classIds.insert(classRecord.id).second)
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output class identifiers must be unique.")
                );
        }
        if (!selectedClassIds.contains(classRecord.id))
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output class is outside the requested scope.")
                );
        }
        if (!isClassTextValid(classRecord))
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output class field exceeds its limit.")
                );
        }
        if (!addClassText(classRecord, &totalTextBytes))
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output text exceeds its total byte limit.")
                );
        }

        if (classRecord.teacherId.has_value())
        {
            if (!isValidId(*classRecord.teacherId))
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output class teacher identifier is malformed.")
                    );
            }
            if (!teacherIds.contains(*classRecord.teacherId))
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output class references a missing teacher.")
                    );
            }
            referencedTeacherIds.insert(*classRecord.teacherId);
        }

        if (classRecord.meetings.size()
            > kSubPrepPrintSourceMaxMeetingsPerClass)
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output class exceeds its meeting limit.")
                );
        }
        if (totalMeetings
            > kSubPrepPrintSourceMaxMeetings - classRecord.meetings.size())
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output meetings exceed their total limit.")
                );
        }
        totalMeetings += classRecord.meetings.size();
        for (const auto& meeting : classRecord.meetings)
        {
            if (!isValidWeekday(meeting.weekday)
                || !selectedDays.contains(meeting.weekday))
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output meeting is outside the requested days.")
                    );
            }
            if (!isOptionalText(
                    meeting.startTime,
                    kSubPrepPrintSourceMaxMeetingTimeLength
                    )
                || !isOptionalText(
                    meeting.endTime,
                    kSubPrepPrintSourceMaxMeetingTimeLength
                    )
                || !addTextBytes(meeting.startTime, &totalTextBytes)
                || !addTextBytes(meeting.endTime, &totalTextBytes))
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output meeting time exceeds its limit.")
                    );
            }
        }

        if (classRecord.rosterColumns.size()
            > kSubPrepRosterOutputMaxRosterColumns)
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output column collection exceeds its limit.")
                );
        }
        std::set<std::string> rosterColumns;
        for (const auto& column : classRecord.rosterColumns)
        {
            if (isBlank(column)
                || column.size() > kSubPrepRosterOutputMaxColumnNameBytes)
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output column name is malformed or too long.")
                    );
            }
            if (!rosterColumns.insert(column).second)
            {
                return Domain::Result<void>::failure(
                    validationError("Roster-output column names must be unique.")
                    );
            }
            if (!allowedRosterColumns.contains(column))
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output column was not requested.")
                    );
            }
            if (!addTextBytes(column, &totalTextBytes))
            {
                return Domain::Result<void>::failure(
                    validationError("Roster-output text exceeds its total byte limit.")
                    );
            }
        }

        if (classRecord.rosterRows.size()
            > kSubPrepRosterOutputMaxRowsPerClass)
        {
            return Domain::Result<void>::failure(
                validationError("A roster-output class exceeds its row limit.")
                );
        }
        if (totalRows
            > kSubPrepRosterOutputMaxTotalRows - classRecord.rosterRows.size())
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output rows exceed their total limit.")
                );
        }
        totalRows += classRecord.rosterRows.size();

        for (const auto& row : classRecord.rosterRows)
        {
            if (row.size() != classRecord.rosterColumns.size())
            {
                return Domain::Result<void>::failure(
                    validationError("A roster-output row has an invalid cell count.")
                    );
            }
            if (totalCells
                > kSubPrepRosterOutputMaxTotalCells - row.size())
            {
                return Domain::Result<void>::failure(
                    validationError("Roster-output cells exceed their total limit.")
                    );
            }
            totalCells += row.size();
            for (const auto& cell : row)
            {
                if (!addTextBytes(cell, &totalTextBytes))
                {
                    return Domain::Result<void>::failure(
                        validationError("Roster-output cell text exceeds its byte limit.")
                        );
                }
            }
        }
    }

    for (const auto& teacherId : teacherIds)
    {
        if (!referencedTeacherIds.contains(teacherId))
        {
            return Domain::Result<void>::failure(
                validationError("Roster-output source contains an unused teacher.")
                );
        }
    }

    return Domain::Result<void>::success();
}

} // namespace SubPrepRosterOutputSourceQueryDetail

// This uncached query validates scope and bounds before and after its single
// read. Its caller owns the returned values for one output operation only.
class SubPrepRosterOutputSourceQuery final
{
public:
    explicit SubPrepRosterOutputSourceQuery(
        SubPrepRosterOutputSourceReadPort& readPort
        ) noexcept
        : m_readPort(readPort)
    {
    }

    [[nodiscard]] SubPrepRosterOutputSourceQueryResult execute(
        const SubPrepRosterOutputSourceRequest& request
        ) const
    {
        const auto requestValidation =
            SubPrepRosterOutputSourceQueryDetail::validateRequest(request);
        if (!requestValidation)
        {
            return SubPrepRosterOutputSourceQueryResult::failure(
                requestValidation.error()
                );
        }

        if (request.selectedClassIds.empty() || request.selectedDays.empty())
        {
            return SubPrepRosterOutputSourceQueryResult::success(
                SubPrepRosterOutputSource({}, {})
                );
        }

        auto source = m_readPort.loadSource(request);
        if (!source)
        {
            return SubPrepRosterOutputSourceQueryResult::failure(
                source.error()
                );
        }

        auto input = std::move(source.value());
        const auto sourceValidation =
            SubPrepRosterOutputSourceQueryDetail::validateSource(
                request,
                input
                );
        if (!sourceValidation)
        {
            return SubPrepRosterOutputSourceQueryResult::failure(
                sourceValidation.error()
                );
        }

        return SubPrepRosterOutputSourceQueryResult::success(
            SubPrepRosterOutputSource(
                std::move(input.teachers),
                std::move(input.classes)
                )
            );
    }

private:
    SubPrepRosterOutputSourceReadPort& m_readPort;
};

} // namespace ClassMngr::Next::Application
