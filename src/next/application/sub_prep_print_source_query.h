#pragma once

#include "next/application/class_summary_projection.h"
#include "next/application/schedule_view_projection.h"
#include "next/application/sub_prep_schedule_summary_query.h"
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

inline constexpr std::size_t kSubPrepPrintSourceMaxIdentifierLength =
    kSummaryMaxIdentifierLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxClassIds =
    kClassSummaryMaxEntries;
inline constexpr std::size_t kSubPrepPrintSourceMaxSelectedDays = 7;
inline constexpr std::size_t kSubPrepPrintSourceMaxClasses =
    kClassSummaryMaxEntries;
inline constexpr std::size_t kSubPrepPrintSourceMaxTeachers =
    kTeacherSummaryMaxEntries;
inline constexpr std::size_t kSubPrepPrintSourceMaxMeetingsPerClass = 64;
inline constexpr std::size_t kSubPrepPrintSourceMaxMeetings = 16'384;
inline constexpr std::size_t kSubPrepPrintSourceMaxGradeLength =
    kClassSummaryMaxGradeLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxLevelLength =
    kClassSummaryMaxLevelLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxClassNotesLength =
    kSelectedClassDetailsMaxClassNotesLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxTeacherNotesLength =
    kSelectedClassDetailsMaxTeacherNotesLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxEnglishNameLength =
    kTeacherSummaryMaxDisplayNameLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxKoreanNameLength =
    kTeacherSummaryMaxDisplayNameLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxPreferredNameLength =
    kTeacherSummaryMaxDisplayNameLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxPreferredRomanizationLength =
    kTeacherSummaryMaxDisplayNameLength;
inline constexpr std::size_t kSubPrepPrintSourceMaxRoomLength = 256;
inline constexpr std::size_t kSubPrepPrintSourceMaxWifiNameLength = 256;
inline constexpr std::size_t kSubPrepPrintSourceMaxWifiPasswordLength = 256;
inline constexpr std::size_t kSubPrepPrintSourceMaxInternetTypeLength = 64;
inline constexpr std::size_t kSubPrepPrintSourceMaxZoomIdLength = 256;
inline constexpr std::size_t kSubPrepPrintSourceMaxZoomPasswordLength = 256;
inline constexpr std::size_t kSubPrepPrintSourceMaxProjectionTypeLength = 64;
inline constexpr std::size_t kSubPrepPrintSourceMaxColorLength = 32;
inline constexpr std::size_t kSubPrepPrintSourceMaxMeetingTimeLength = 32;
inline constexpr std::size_t kSubPrepPrintSourceMaxStudentCount =
    kClassSummaryMaxStudentCount;

struct SubPrepPrintSourceRequest final
{
    std::vector<Domain::ClassId> selectedClassIds;
    std::vector<SubPrepWeekday> selectedDays;
    ScheduleViewMode mode = ScheduleViewMode::Regular;

    friend bool operator==(
        const SubPrepPrintSourceRequest&,
        const SubPrepPrintSourceRequest&
        ) = default;
};

struct SubPrepPrintMeeting final
{
    SubPrepWeekday weekday = SubPrepWeekday::Monday;
    std::string startTime;
    std::string endTime;

    friend bool operator==(
        const SubPrepPrintMeeting&,
        const SubPrepPrintMeeting&
        ) = default;
};

struct SubPrepPrintTeacher final
{
    Domain::TeacherId id;
    std::string englishName;
    std::string koreanName;
    std::string preferredName;
    std::string preferredRomanization;
    std::string room;
    std::string wifiName;
    std::string wifiPassword;
    std::string internetType;
    std::string zoomId;
    std::string zoomPassword;
    std::string projectionType;
    std::string teacherNotes;

    friend bool operator==(
        const SubPrepPrintTeacher&,
        const SubPrepPrintTeacher&
        ) = default;
};

struct SubPrepPrintClass final
{
    Domain::ClassId id;
    std::optional<Domain::TeacherId> teacherId;
    std::string grade;
    std::string level;
    std::string classNotes;
    std::string classColor;
    std::string fontColor;
    std::size_t studentCount = 0;
    std::vector<SubPrepPrintMeeting> meetings;

    friend bool operator==(
        const SubPrepPrintClass&,
        const SubPrepPrintClass&
        ) = default;
};

struct SubPrepPrintSourceInput final
{
    std::vector<SubPrepPrintTeacher> teachers;
    std::vector<SubPrepPrintClass> classes;

    friend bool operator==(
        const SubPrepPrintSourceInput&,
        const SubPrepPrintSourceInput&
        ) = default;
};

using SubPrepPrintSourceReadResult =
    Domain::Result<SubPrepPrintSourceInput>;

// Adapters filter by the full class/day/mode request before loading rich
// records. They return owning copies in stable order and retain neither the
// request reference nor raw source records after this call returns. The
// source vectors preserve that order for the caller.
class SubPrepPrintSourceReadPort
{
public:
    virtual ~SubPrepPrintSourceReadPort() = default;

    [[nodiscard]] virtual SubPrepPrintSourceReadResult loadSource(
        const SubPrepPrintSourceRequest& request
        ) = 0;
};

// This value owns only the bounded facts needed for one information-sheet
// stage. It is caller-owned and should be released when that stage finishes.
// Accessors expose immutable views; all text and collections are value copies.
class SubPrepPrintSource final
{
public:
    using Teacher = SubPrepPrintTeacher;
    using Class = SubPrepPrintClass;

    SubPrepPrintSource() = default;

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
        const SubPrepPrintSource&,
        const SubPrepPrintSource&
        ) = default;

private:
    friend class SubPrepPrintSourceQuery;

    SubPrepPrintSource(
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

using SubPrepPrintSourceQueryResult = Domain::Result<SubPrepPrintSource>;

namespace SubPrepPrintSourceQueryDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
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
    const std::size_t maxLength
    ) noexcept
{
    return value.empty()
        || (!isBlank(value) && value.size() <= maxLength);
}

template <typename TypedId>
[[nodiscard]] inline bool isValidId(
    const TypedId& id
    ) noexcept
{
    return !isBlank(id.value())
        && id.value().size() <= kSubPrepPrintSourceMaxIdentifierLength;
}

[[nodiscard]] inline bool isValidWeekday(
    const SubPrepWeekday weekday
    ) noexcept
{
    const auto dayValue = static_cast<std::uint8_t>(weekday);
    return dayValue >= static_cast<std::uint8_t>(SubPrepWeekday::Monday)
        && dayValue <= static_cast<std::uint8_t>(SubPrepWeekday::Sunday);
}

[[nodiscard]] inline bool isValidMode(
    const ScheduleViewMode mode
    ) noexcept
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
    const SubPrepPrintSourceRequest& request
    )
{
    if (request.selectedClassIds.size() > kSubPrepPrintSourceMaxClassIds)
    {
        return Domain::Result<void>::failure(
            invalidInput("Selected Sub Prep class scope exceeds its limit.")
            );
    }

    std::set<Domain::ClassId> selectedClassIds;
    for (const auto& classId : request.selectedClassIds)
    {
        if (!isValidId(classId))
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Selected class identifiers must be non-blank and bounded."
                    )
                );
        }

        if (!selectedClassIds.insert(classId).second)
        {
            return Domain::Result<void>::failure(
                invalidInput("Selected class identifiers must be unique.")
                );
        }
    }

    if (request.selectedDays.size() > kSubPrepPrintSourceMaxSelectedDays)
    {
        return Domain::Result<void>::failure(
            invalidInput("Selected Sub Prep days exceed the weekly limit.")
            );
    }

    std::set<SubPrepWeekday> selectedDays;
    for (const auto day : request.selectedDays)
    {
        if (!isValidWeekday(day))
        {
            return Domain::Result<void>::failure(
                invalidInput("A selected Sub Prep weekday is invalid.")
                );
        }

        if (!selectedDays.insert(day).second)
        {
            return Domain::Result<void>::failure(
                invalidInput("Selected Sub Prep weekdays must be unique.")
                );
        }
    }

    if (!isValidMode(request.mode))
    {
        return Domain::Result<void>::failure(
            invalidInput("The Sub Prep schedule mode is invalid.")
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline bool isValidTeacherText(
    const SubPrepPrintTeacher& teacher
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
            )
        && isOptionalText(teacher.room, kSubPrepPrintSourceMaxRoomLength)
        && isOptionalText(
            teacher.wifiName,
            kSubPrepPrintSourceMaxWifiNameLength
            )
        && isOptionalText(
            teacher.wifiPassword,
            kSubPrepPrintSourceMaxWifiPasswordLength
            )
        && isOptionalText(
            teacher.internetType,
            kSubPrepPrintSourceMaxInternetTypeLength
            )
        && isOptionalText(teacher.zoomId, kSubPrepPrintSourceMaxZoomIdLength)
        && isOptionalText(
            teacher.zoomPassword,
            kSubPrepPrintSourceMaxZoomPasswordLength
            )
        && isOptionalText(
            teacher.projectionType,
            kSubPrepPrintSourceMaxProjectionTypeLength
            )
        && isOptionalText(
            teacher.teacherNotes,
            kSubPrepPrintSourceMaxTeacherNotesLength
            );
}

[[nodiscard]] inline bool isValidClassText(
    const SubPrepPrintClass& classRecord
    ) noexcept
{
    return isOptionalText(classRecord.grade, kSubPrepPrintSourceMaxGradeLength)
        && isOptionalText(classRecord.level, kSubPrepPrintSourceMaxLevelLength)
        && isOptionalText(
            classRecord.classNotes,
            kSubPrepPrintSourceMaxClassNotesLength
            )
        && isOptionalText(
            classRecord.classColor,
            kSubPrepPrintSourceMaxColorLength
            )
        && isOptionalText(
            classRecord.fontColor,
            kSubPrepPrintSourceMaxColorLength
            )
        && classRecord.studentCount <= kSubPrepPrintSourceMaxStudentCount;
}

[[nodiscard]] inline bool isValidMeetingText(
    const SubPrepPrintMeeting& meeting
    ) noexcept
{
    return isOptionalText(
               meeting.startTime,
               kSubPrepPrintSourceMaxMeetingTimeLength
               )
        && isOptionalText(
            meeting.endTime,
            kSubPrepPrintSourceMaxMeetingTimeLength
            );
}

[[nodiscard]] inline Domain::Result<void> validateSource(
    const SubPrepPrintSourceRequest& request,
    const SubPrepPrintSourceInput& input
    )
{
    if (input.teachers.size() > kSubPrepPrintSourceMaxTeachers)
    {
        return Domain::Result<void>::failure(
            validationError("Sub Prep print teacher collection exceeds its limit.")
            );
    }

    if (input.classes.size() > kSubPrepPrintSourceMaxClasses)
    {
        return Domain::Result<void>::failure(
            validationError("Sub Prep print class collection exceeds its limit.")
            );
    }

    std::set<Domain::TeacherId> teacherIds;
    for (const auto& teacher : input.teachers)
    {
        if (!isValidId(teacher.id))
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print teacher identifier is malformed."
                    )
                );
        }

        if (!teacherIds.insert(teacher.id).second)
        {
            return Domain::Result<void>::failure(
                validationError(
                    "Sub Prep print teacher identifiers must be unique."
                    )
                );
        }

        if (!isValidTeacherText(teacher))
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print teacher field exceeds its bounded limit or is malformed."
                    )
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
    std::set<Domain::ClassId> classIds;
    std::set<Domain::TeacherId> referencedTeacherIds;
    std::size_t totalMeetings = 0;
    for (const auto& classRecord : input.classes)
    {
        if (!isValidId(classRecord.id))
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print class identifier is malformed."
                    )
                );
        }

        if (!classIds.insert(classRecord.id).second)
        {
            return Domain::Result<void>::failure(
                validationError(
                    "Sub Prep print class identifiers must be unique."
                    )
                );
        }

        if (!selectedClassIds.contains(classRecord.id))
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print class is outside the requested scope."
                    )
                );
        }

        if (!isValidClassText(classRecord))
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print class field exceeds its bounded limit or is malformed."
                    )
                );
        }

        if (classRecord.teacherId.has_value())
        {
            if (!isValidId(*classRecord.teacherId))
            {
                return Domain::Result<void>::failure(
                    validationError(
                        "A Sub Prep print class has a malformed teacher identifier."
                        )
                    );
            }

            if (!teacherIds.contains(*classRecord.teacherId))
            {
                return Domain::Result<void>::failure(
                    validationError(
                        "A Sub Prep print class references a missing teacher."
                        )
                    );
            }

            referencedTeacherIds.insert(*classRecord.teacherId);
        }

        if (classRecord.meetings.size()
            > kSubPrepPrintSourceMaxMeetingsPerClass)
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print class exceeds its meeting limit."
                    )
                );
        }

        if (totalMeetings
            > kSubPrepPrintSourceMaxMeetings - classRecord.meetings.size())
        {
            return Domain::Result<void>::failure(
                validationError(
                    "Sub Prep print meetings exceed the total bounded limit."
                    )
                );
        }

        totalMeetings += classRecord.meetings.size();
        for (const auto& meeting : classRecord.meetings)
        {
            if (!isValidWeekday(meeting.weekday))
            {
                return Domain::Result<void>::failure(
                    validationError(
                        "A Sub Prep print meeting has an invalid weekday."
                        )
                    );
            }

            if (!selectedDays.contains(meeting.weekday))
            {
                return Domain::Result<void>::failure(
                    validationError(
                        "A Sub Prep print meeting is outside the requested days."
                        )
                    );
            }

            if (!isValidMeetingText(meeting))
            {
                return Domain::Result<void>::failure(
                    validationError(
                        "A Sub Prep print meeting time exceeds its bounded limit or is malformed."
                        )
                    );
            }
        }
    }

    for (const auto& teacherId : teacherIds)
    {
        if (!referencedTeacherIds.contains(teacherId))
        {
            return Domain::Result<void>::failure(
                validationError(
                    "A Sub Prep print source contains an unused teacher."
                    )
                );
        }
    }

    return Domain::Result<void>::success();
}

} // namespace SubPrepPrintSourceQueryDetail

// This query has no cache. It validates the complete scope before I/O, then
// returns one owned, bounded source value or a structured error. The caller
// releases a successful value after its information-sheet stage completes.
class SubPrepPrintSourceQuery final
{
public:
    explicit SubPrepPrintSourceQuery(
        SubPrepPrintSourceReadPort& readPort
        ) noexcept
        : m_readPort(readPort)
    {
    }

    [[nodiscard]] SubPrepPrintSourceQueryResult execute(
        const SubPrepPrintSourceRequest& request
        ) const
    {
        const auto requestValidation =
            SubPrepPrintSourceQueryDetail::validateRequest(request);
        if (!requestValidation)
        {
            return SubPrepPrintSourceQueryResult::failure(
                requestValidation.error()
                );
        }

        if (request.selectedClassIds.empty() || request.selectedDays.empty())
        {
            return SubPrepPrintSourceQueryResult::success(
                SubPrepPrintSource({}, {})
                );
        }

        auto source = m_readPort.loadSource(request);
        if (!source)
        {
            return SubPrepPrintSourceQueryResult::failure(source.error());
        }

        auto input = std::move(source.value());
        const auto sourceValidation =
            SubPrepPrintSourceQueryDetail::validateSource(request, input);
        if (!sourceValidation)
        {
            return SubPrepPrintSourceQueryResult::failure(
                sourceValidation.error()
                );
        }

        return SubPrepPrintSourceQueryResult::success(
            SubPrepPrintSource(
                std::move(input.teachers),
                std::move(input.classes)
                )
            );
    }

private:
    SubPrepPrintSourceReadPort& m_readPort;
};

} // namespace ClassMngr::Next::Application
