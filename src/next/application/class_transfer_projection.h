#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// One projection is intentionally bounded. A reader or file adapter must
// paginate or stage a larger transfer before it crosses this boundary. The
// adapter owns its raw file representation and releases it before, or while,
// creating the application projection.
inline constexpr std::size_t kClassTransferMaxSourceKeyLength = 256;
inline constexpr std::size_t kClassTransferMaxTeacherEntries = 4'096;
inline constexpr std::size_t kClassTransferMaxClassEntries = 4'096;
inline constexpr std::size_t kClassTransferMaxRegularTimeEntriesPerClass = 64;
inline constexpr std::size_t
    kClassTransferMaxIntensiveTimeEntriesPerClass = 64;
inline constexpr std::size_t kClassTransferMaxTimeEntriesPerClass =
    kClassTransferMaxRegularTimeEntriesPerClass
    + kClassTransferMaxIntensiveTimeEntriesPerClass;
inline constexpr std::size_t kClassTransferMaxPackageTimeEntries = 16'384;
inline constexpr std::size_t kClassTransferMaxPackageEntries =
    kClassTransferMaxTeacherEntries + kClassTransferMaxClassEntries;

inline constexpr std::size_t kClassTransferMaxTeacherDisplayNameLength = 256;
inline constexpr std::size_t kClassTransferMaxTeacherSummaryLength = 512;
inline constexpr std::size_t kClassTransferMaxTeacherNotesLength = 2'048;
inline constexpr std::size_t kClassTransferMaxClassNameLength = 256;
inline constexpr std::size_t kClassTransferMaxClassSummaryLength = 512;
inline constexpr std::size_t kClassTransferMaxClassGradeLength = 64;
inline constexpr std::size_t kClassTransferMaxClassLevelLength = 64;
inline constexpr std::size_t kClassTransferMaxClassBookLength = 256;
inline constexpr std::size_t kClassTransferMaxClassColorLength = 32;
inline constexpr std::size_t kClassTransferMaxClassNotesLength = 4'096;
inline constexpr std::size_t
    kClassTransferMaxTimeFillerActivitiesLength = 2'048;
inline constexpr std::size_t kClassTransferMaxTimeDayLength = 64;
inline constexpr std::size_t kClassTransferMaxTimeValueLength = 32;

// These shorter names make the collection limits easy to use at adapter call
// sites while retaining the field-specific constants above for validation.
inline constexpr std::size_t kClassTransferMaxTeachers =
    kClassTransferMaxTeacherEntries;
inline constexpr std::size_t kClassTransferMaxClasses =
    kClassTransferMaxClassEntries;
inline constexpr std::size_t kClassTransferMaxTimesPerClass =
    kClassTransferMaxTimeEntriesPerClass;
inline constexpr std::size_t kClassTransferMaxTimes =
    kClassTransferMaxPackageTimeEntries;

enum class TransferTimeCategory
{
    Regular,
    Intensive
};

inline constexpr std::int64_t kClassTransferMinutesPerDay = 24 * 60;
inline constexpr std::int64_t kClassTransferMinutesPerWeek =
    7 * kClassTransferMinutesPerDay;

// The repository adapter owns legacy weekday and time parsing. Once parsed,
// schedule intervals use absolute minutes from Monday at 00:00; an overnight
// interval may end after the final minute of the week.
class ClassTransferScheduleCandidate final
{
public:
    [[nodiscard]] static Domain::Result<ClassTransferScheduleCandidate> create(
        const TransferTimeCategory category,
        const std::int64_t weekdayIndex,
        const std::int64_t startMinuteOfDay,
        const std::int64_t endMinuteOfDay
        )
    {
        if ((category != TransferTimeCategory::Regular
                && category != TransferTimeCategory::Intensive)
            || weekdayIndex < 0 || weekdayIndex >= 7
            || startMinuteOfDay < 0
            || startMinuteOfDay >= kClassTransferMinutesPerDay
            || endMinuteOfDay < 0
            || endMinuteOfDay >= kClassTransferMinutesPerDay)
        {
            return Domain::Result<ClassTransferScheduleCandidate>::failure(
                Domain::OperationError{
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "The class transfer schedule interval is invalid.",
                    .recoverable = false
                }
                );
        }

        const std::int64_t startMinuteOfWeek =
            weekdayIndex * kClassTransferMinutesPerDay + startMinuteOfDay;
        std::int64_t endMinuteOfWeek =
            weekdayIndex * kClassTransferMinutesPerDay + endMinuteOfDay;
        if (endMinuteOfWeek <= startMinuteOfWeek)
        {
            endMinuteOfWeek += kClassTransferMinutesPerDay;
        }

        return Domain::Result<ClassTransferScheduleCandidate>::success(
            ClassTransferScheduleCandidate(
                category,
                startMinuteOfWeek,
                endMinuteOfWeek
                )
            );
    }

    [[nodiscard]] TransferTimeCategory category() const noexcept
    {
        return m_category;
    }

    [[nodiscard]] std::int64_t startMinuteOfWeek() const noexcept
    {
        return m_startMinuteOfWeek;
    }

    [[nodiscard]] std::int64_t endMinuteOfWeek() const noexcept
    {
        return m_endMinuteOfWeek;
    }

    friend bool operator==(
        const ClassTransferScheduleCandidate&,
        const ClassTransferScheduleCandidate&
        ) = default;

private:
    ClassTransferScheduleCandidate(
        const TransferTimeCategory category,
        const std::int64_t startMinuteOfWeek,
        const std::int64_t endMinuteOfWeek
        ) noexcept
        : m_category(category),
          m_startMinuteOfWeek(startMinuteOfWeek),
          m_endMinuteOfWeek(endMinuteOfWeek)
    {
    }

    TransferTimeCategory m_category;
    std::int64_t m_startMinuteOfWeek;
    std::int64_t m_endMinuteOfWeek;
};

struct ClassTransferScheduleConflict final
{
    std::size_t incomingIndex = 0;
    std::size_t otherIndex = 0;
    bool otherIsIncoming = false;

    friend bool operator==(
        const ClassTransferScheduleConflict&,
        const ClassTransferScheduleConflict&
        ) = default;
};

[[nodiscard]] inline bool classTransferScheduleIntervalsOverlap(
    const ClassTransferScheduleCandidate& first,
    const ClassTransferScheduleCandidate& second
    ) noexcept
{
    for (std::int64_t weekOffset = -1; weekOffset <= 1; ++weekOffset)
    {
        const std::int64_t offset =
            weekOffset * kClassTransferMinutesPerWeek;
        if (first.startMinuteOfWeek()
                < second.endMinuteOfWeek() + offset
            && second.startMinuteOfWeek() + offset
                < first.endMinuteOfWeek())
        {
            return true;
        }
    }

    return false;
}

// Conflict order is stable: Regular before Intensive; within a category,
// each incoming schedule is compared with later incoming schedules first,
// then existing schedules in their supplied order. Existing schedules are
// never compared with each other.
[[nodiscard]] inline std::vector<ClassTransferScheduleConflict>
findClassTransferScheduleConflicts(
    const std::vector<ClassTransferScheduleCandidate>& incoming,
    const std::vector<ClassTransferScheduleCandidate>& existing
    )
{
    std::vector<ClassTransferScheduleConflict> conflicts;

    for (const TransferTimeCategory category : {
             TransferTimeCategory::Regular,
             TransferTimeCategory::Intensive
         })
    {
        for (std::size_t first = 0; first < incoming.size(); ++first)
        {
            if (incoming[first].category() != category)
            {
                continue;
            }

            for (std::size_t second = first + 1;
                 second < incoming.size();
                 ++second)
            {
                if (incoming[second].category() == category
                    && classTransferScheduleIntervalsOverlap(
                        incoming[first], incoming[second]))
                {
                    conflicts.push_back({first, second, true});
                }
            }

            for (std::size_t second = 0; second < existing.size(); ++second)
            {
                if (existing[second].category() == category
                    && classTransferScheduleIntervalsOverlap(
                        incoming[first], existing[second]))
                {
                    conflicts.push_back({first, second, false});
                }
            }
        }
    }

    return conflicts;
}

// Reader/staging value. It contains only the source key and bounded flat
// text needed for matching and writing; it does not retain a source object or
// an external owner.
struct TransferTeacher final
{
    std::string sourceKey;
    std::string displayName;
    std::string summary;
    std::string notes;

    friend bool operator==(
        const TransferTeacher&,
        const TransferTeacher&
        ) = default;
};

// Reader/staging value. Regular and intensive times remain separate members,
// so the category cannot be lost when a writer consumes the projection.
struct TransferClassTime final
{
    std::string day;
    std::string startTime;
    std::string endTime;

    friend bool operator==(
        const TransferClassTime&,
        const TransferClassTime&
        ) = default;
};

// A transfer class is deliberately a flat, copyable record. Optional
// teacherSourceKey is the explicit missing-teacher fallback: absent means the
// class has no teacher. A present key must resolve in the teacher collection.
struct TransferClass final
{
    std::string sourceKey;
    std::string name;
    std::string summary;
    std::optional<std::string> teacherSourceKey;
    std::string grade;
    std::string level;
    std::string readingBook;
    std::string essayBook;
    std::string classColor;
    std::string fontColor;
    std::vector<TransferClassTime> regularTimes;
    std::vector<TransferClassTime> intensiveTimes;
    std::string notes;
    std::string timeFillerActivities;
    std::int32_t order = 0;

    [[nodiscard]] bool hasTeacher() const noexcept
    {
        return teacherSourceKey.has_value();
    }

    [[nodiscard]] std::size_t timeCount() const noexcept
    {
        return regularTimes.size() + intensiveTimes.size();
    }

    friend bool operator==(
        const TransferClass&,
        const TransferClass&
        ) = default;
};

// This is the reader-side package boundary. The reader may build it in
// bounded stages and pass it by value or move it into create(). Once create()
// succeeds, the application projection owns its copied compact records and
// the reader must release any raw file/parsed source representation.
struct ClassTransferStagingPackage final
{
    std::vector<TransferTeacher> teachers;
    std::vector<TransferClass> classes;

    friend bool operator==(
        const ClassTransferStagingPackage&,
        const ClassTransferStagingPackage&
        ) = default;
};

using ClassTransferProjectionInput = ClassTransferStagingPackage;
using ClassTransferInput = ClassTransferStagingPackage;
using ClassTransferPackage = ClassTransferStagingPackage;
using TransferReaderPackage = ClassTransferStagingPackage;

// These aliases make the ownership handoff visible to adapters without
// adding a second representation or retaining a reader/writer graph.
class ClassTransferProjection;

using ClassTransferWriterProjection = ClassTransferProjection;
using TransferWriterProjection = ClassTransferProjection;
using TransferTeacherRecord = TransferTeacher;
using TransferClassRecord = TransferClass;
using TransferClassTimeRecord = TransferClassTime;

namespace ClassTransferProjectionDetail
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

[[nodiscard]] inline bool isValidKey(
    const std::string_view value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kClassTransferMaxSourceKeyLength;
}

[[nodiscard]] inline bool isRequiredText(
    const std::string_view value,
    const std::size_t maxLength
    ) noexcept
{
    return !isBlank(value) && value.size() <= maxLength;
}

[[nodiscard]] inline bool isOptionalText(
    const std::string_view value,
    const std::size_t maxLength
    ) noexcept
{
    return value.empty()
        || (!isBlank(value) && value.size() <= maxLength);
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

[[nodiscard]] inline Domain::Result<void> invalid(
    const char* message
    )
{
    return Domain::Result<void>::failure(invalidInput(message));
}

[[nodiscard]] inline bool contains(
    const std::vector<std::string>& values,
    const std::string_view candidate
    ) noexcept
{
    return std::find(
               values.cbegin(),
               values.cend(),
               candidate
               )
        != values.cend();
}

[[nodiscard]] inline Domain::Result<void> validateTime(
    const TransferClassTime& time
    )
{
    if (!isRequiredText(time.day, kClassTransferMaxTimeDayLength)
        || !isRequiredText(time.startTime, kClassTransferMaxTimeValueLength)
        || !isRequiredText(time.endTime, kClassTransferMaxTimeValueLength))
    {
        return invalid(
            "Transfer class time day, start, and end values must be non-blank and bounded."
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateTimes(
    const std::vector<TransferClassTime>& times,
    const std::size_t maxEntries,
    const char* message
    )
{
    if (times.size() > maxEntries)
    {
        return invalid(message);
    }

    for (const auto& time : times)
    {
        const auto validation = validateTime(time);
        if (!validation)
        {
            return validation;
        }
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateTeacher(
    const TransferTeacher& teacher,
    const std::vector<std::string>& existingKeys
    )
{
    if (!isValidKey(teacher.sourceKey))
    {
        return invalid(
            "Transfer teacher source key must be non-blank and bounded."
            );
    }

    if (contains(existingKeys, teacher.sourceKey))
    {
        return invalid("Transfer teacher source keys must be unique.");
    }

    if (!isRequiredText(
            teacher.displayName,
            kClassTransferMaxTeacherDisplayNameLength
            )
        || !isOptionalText(
            teacher.summary,
            kClassTransferMaxTeacherSummaryLength
            )
        || !isOptionalText(
            teacher.notes,
            kClassTransferMaxTeacherNotesLength
            ))
    {
        return invalid(
            "Transfer teacher name must be required and bounded; summary and notes must be empty or bounded."
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateClass(
    const TransferClass& transferClass,
    const std::vector<std::string>& existingKeys,
    const std::vector<std::string>& teacherKeys
    )
{
    if (!isValidKey(transferClass.sourceKey))
    {
        return invalid(
            "Transfer class source key must be non-blank and bounded."
            );
    }

    if (contains(existingKeys, transferClass.sourceKey))
    {
        return invalid("Transfer class source keys must be unique.");
    }

    if (!isRequiredText(
            transferClass.name,
            kClassTransferMaxClassNameLength
            )
        || !isOptionalText(
            transferClass.summary,
            kClassTransferMaxClassSummaryLength
            )
        || !isOptionalText(
            transferClass.grade,
            kClassTransferMaxClassGradeLength
            )
        || !isOptionalText(
            transferClass.level,
            kClassTransferMaxClassLevelLength
            )
        || !isOptionalText(
            transferClass.readingBook,
            kClassTransferMaxClassBookLength
            )
        || !isOptionalText(
            transferClass.essayBook,
            kClassTransferMaxClassBookLength
            )
        || !isOptionalText(
            transferClass.classColor,
            kClassTransferMaxClassColorLength
            )
        || !isOptionalText(
            transferClass.fontColor,
            kClassTransferMaxClassColorLength
            )
        || !isOptionalText(
            transferClass.notes,
            kClassTransferMaxClassNotesLength
            )
        || !isOptionalText(
            transferClass.timeFillerActivities,
            kClassTransferMaxTimeFillerActivitiesLength
            ))
    {
        return invalid(
            "Transfer class name and metadata must be bounded; optional text must be empty or bounded."
            );
    }

    if (transferClass.teacherSourceKey.has_value())
    {
        if (!isValidKey(*transferClass.teacherSourceKey))
        {
            return invalid(
                "Optional transfer teacher source key must be non-blank and bounded."
                );
        }

        if (!contains(teacherKeys, *transferClass.teacherSourceKey))
        {
            return invalid(
                "Transfer class references an unknown teacher source key."
                );
        }
    }

    if (transferClass.order < 0)
    {
        return invalid("Transfer class order must not be negative.");
    }

    const auto regularValidation = validateTimes(
        transferClass.regularTimes,
        kClassTransferMaxRegularTimeEntriesPerClass,
        "Regular transfer class time collection exceeds its bounded limit."
        );
    if (!regularValidation)
    {
        return regularValidation;
    }

    const auto intensiveValidation = validateTimes(
        transferClass.intensiveTimes,
        kClassTransferMaxIntensiveTimeEntriesPerClass,
        "Intensive transfer class time collection exceeds its bounded limit."
        );
    if (!intensiveValidation)
    {
        return intensiveValidation;
    }

    if (transferClass.timeCount() > kClassTransferMaxTimeEntriesPerClass)
    {
        return invalid(
            "Transfer class time collection exceeds its bounded per-class limit."
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateInput(
    const ClassTransferStagingPackage& input
    )
{
    if (input.teachers.size() > kClassTransferMaxTeacherEntries
        || input.classes.size() > kClassTransferMaxClassEntries)
    {
        return invalid(
            "Transfer teacher or class collection exceeds its bounded limit."
            );
    }

    if (input.teachers.size() > kClassTransferMaxPackageEntries
        || input.classes.size() > kClassTransferMaxPackageEntries - input.teachers.size())
    {
        return invalid(
            "Transfer package record collection exceeds its bounded limit."
            );
    }

    std::vector<std::string> teacherKeys;
    teacherKeys.reserve(input.teachers.size());
    for (const auto& teacher : input.teachers)
    {
        const auto validation = validateTeacher(teacher, teacherKeys);
        if (!validation)
        {
            return validation;
        }

        teacherKeys.push_back(teacher.sourceKey);
    }

    std::vector<std::string> classKeys;
    classKeys.reserve(input.classes.size());
    std::size_t totalTimes = 0;
    for (const auto& transferClass : input.classes)
    {
        const auto validation = validateClass(
            transferClass,
            classKeys,
            teacherKeys
            );
        if (!validation)
        {
            return validation;
        }

        if (totalTimes > kClassTransferMaxPackageTimeEntries
            - transferClass.timeCount())
        {
            return invalid(
                "Transfer package time collection exceeds its bounded limit."
                );
        }

        totalTimes += transferClass.timeCount();
        classKeys.push_back(transferClass.sourceKey);
    }

    return Domain::Result<void>::success();
}

}

// Writer/application-side immutable value projection. It owns only the
// compact copied records. All record and schedule lookups return values, so a
// caller cannot mutate projection storage or retain a pointer into it.
class ClassTransferProjection final
{
public:
    using Input = ClassTransferStagingPackage;
    using Teacher = TransferTeacher;
    using Class = TransferClass;
    using Time = TransferClassTime;

    ClassTransferProjection() = default;

    [[nodiscard]] static Domain::Result<ClassTransferProjection> create(
        Input input
        )
    {
        const auto validation = ClassTransferProjectionDetail::validateInput(
            input
            );
        if (!validation)
        {
            return Domain::Result<ClassTransferProjection>::failure(
                validation.error()
                );
        }

        std::size_t totalTimes = 0;
        for (const auto& transferClass : input.classes)
        {
            totalTimes += transferClass.timeCount();
        }

        return Domain::Result<ClassTransferProjection>::success(
            ClassTransferProjection(
                std::move(input.teachers),
                std::move(input.classes),
                totalTimes
                )
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& input
        )
    {
        return ClassTransferProjectionDetail::validateInput(input);
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Teacher& teacher
        )
    {
        return ClassTransferProjectionDetail::validateTeacher(
            teacher,
            {}
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Time& time
        )
    {
        return ClassTransferProjectionDetail::validateTime(time);
    }

    [[nodiscard]] const std::vector<Teacher>& teachers() const noexcept
    {
        return m_teachers;
    }

    [[nodiscard]] const std::vector<Class>& classes() const noexcept
    {
        return m_classes;
    }

    [[nodiscard]] const std::vector<Teacher>& transferTeachers() const noexcept
    {
        return teachers();
    }

    [[nodiscard]] const std::vector<Class>& transferClasses() const noexcept
    {
        return classes();
    }

    [[nodiscard]] std::size_t teacherCount() const noexcept
    {
        return m_teachers.size();
    }

    [[nodiscard]] std::size_t classCount() const noexcept
    {
        return m_classes.size();
    }

    [[nodiscard]] std::size_t timeCount() const noexcept
    {
        return m_totalTimeCount;
    }

    [[nodiscard]] std::optional<Teacher> findTeacher(
        const std::string_view sourceKey
        ) const
    {
        const auto teacher = std::find_if(
            m_teachers.cbegin(),
            m_teachers.cend(),
            [sourceKey](const Teacher& candidate)
            {
                return candidate.sourceKey == sourceKey;
            }
            );
        if (teacher == m_teachers.cend())
        {
            return std::nullopt;
        }

        return *teacher;
    }

    [[nodiscard]] std::optional<Teacher> lookupTeacher(
        const std::string_view sourceKey
        ) const
    {
        return findTeacher(sourceKey);
    }

    [[nodiscard]] std::optional<Class> findClass(
        const std::string_view sourceKey
        ) const
    {
        const auto transferClass = std::find_if(
            m_classes.cbegin(),
            m_classes.cend(),
            [sourceKey](const Class& candidate)
            {
                return candidate.sourceKey == sourceKey;
            }
            );
        if (transferClass == m_classes.cend())
        {
            return std::nullopt;
        }

        return *transferClass;
    }

    [[nodiscard]] std::optional<Class> lookupClass(
        const std::string_view sourceKey
        ) const
    {
        return findClass(sourceKey);
    }

    [[nodiscard]] std::optional<std::vector<Time>> findTimes(
        const std::string_view sourceKey,
        const TransferTimeCategory category
        ) const
    {
        const auto transferClass = findClass(sourceKey);
        if (!transferClass.has_value())
        {
            return std::nullopt;
        }

        if (category == TransferTimeCategory::Regular)
        {
            return transferClass->regularTimes;
        }

        return transferClass->intensiveTimes;
    }

    [[nodiscard]] std::optional<std::vector<Time>> lookupTimes(
        const std::string_view sourceKey,
        const TransferTimeCategory category
        ) const
    {
        return findTimes(sourceKey, category);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_teachers.empty() && m_classes.empty();
    }

    friend bool operator==(
        const ClassTransferProjection&,
        const ClassTransferProjection&
        ) = default;

private:
    ClassTransferProjection(
        std::vector<Teacher> teachers,
        std::vector<Class> classes,
        const std::size_t totalTimeCount
        )
        : m_teachers(std::move(teachers)),
          m_classes(std::move(classes)),
          m_totalTimeCount(totalTimeCount)
    {
    }

    std::vector<Teacher> m_teachers;
    std::vector<Class> m_classes;
    std::size_t m_totalTimeCount = 0;
};

using ClassTransferSnapshot = ClassTransferProjection;
using TransferProjection = ClassTransferProjection;

// Class Transfer review is validated against the match set produced by the
// matching policy. Keeping these inputs and decisions as standard-C++ values
// lets the dialog and apply boundary share the same choice rules; persistence
// remains owned by the repository.
enum class ClassTransferReviewClassAction
{
    Create,
    Replace,
    Skip,
    Unselected,
    Invalid
};

enum class ClassTransferReviewTeacherAction
{
    Create,
    KeepExisting,
    ReplaceExisting,
    Unselected,
    Invalid
};

struct ClassTransferReviewClassCandidate final
{
    int packageClassIndex = -1;
    std::vector<Domain::ClassId> matchingClassIds;
};

struct ClassTransferReviewTeacherCandidate final
{
    std::string teacherKey;
    std::vector<Domain::TeacherId> matchingTeacherIds;
};

struct ClassTransferReviewClassResolution final
{
    int packageClassIndex = -1;
    ClassTransferReviewClassAction action =
        ClassTransferReviewClassAction::Unselected;
    std::optional<Domain::ClassId> targetClassId = std::nullopt;
};

struct ClassTransferReviewTeacherResolution final
{
    std::string teacherKey;
    ClassTransferReviewTeacherAction action =
        ClassTransferReviewTeacherAction::Unselected;
    std::optional<Domain::TeacherId> targetTeacherId = std::nullopt;
};

struct ClassTransferReviewDecisionRequest final
{
    std::vector<ClassTransferReviewClassCandidate> classes;
    std::vector<ClassTransferReviewTeacherCandidate> teachers;
    std::vector<ClassTransferReviewClassResolution> classResolutions;
    std::vector<ClassTransferReviewTeacherResolution> teacherResolutions;
};

enum class ClassTransferReviewDecisionIssueCode
{
    InvalidClassAction,
    InvalidClassIndex,
    DuplicateClassResolution,
    UnknownClassResolution,
    MissingClassResolution,
    ReplaceClassMissingTarget,
    ClassTargetNotInMatchSet,
    NonReplaceClassHasTarget,
    DuplicateClassReplacementTarget,
    InvalidTeacherAction,
    EmptyTeacherKey,
    DuplicateTeacherResolution,
    UnknownTeacherResolution,
    MissingTeacherResolution,
    UniqueTeacherCannotBeCreated,
    TeacherActionMissingTarget,
    TeacherTargetNotInMatchSet,
    CreateTeacherHasTarget,
    DuplicateTeacherReplacementTarget
};

struct ClassTransferReviewDecisionIssue final
{
    ClassTransferReviewDecisionIssueCode code;
    int packageClassIndex = -1;
    std::string teacherKey;
    std::optional<Domain::ClassId> targetClassId = std::nullopt;
    std::optional<Domain::TeacherId> targetTeacherId = std::nullopt;
};

struct ClassTransferReviewDecisionResult final
{
    std::vector<ClassTransferReviewDecisionIssue> issues;

    [[nodiscard]] bool accepted() const noexcept
    {
        return issues.empty();
    }
};

[[nodiscard]] inline ClassTransferReviewDecisionResult
validateClassTransferReviewDecisions(
    const ClassTransferReviewDecisionRequest& request
    )
{
    using Issue = ClassTransferReviewDecisionIssue;
    using IssueCode = ClassTransferReviewDecisionIssueCode;
    using ClassAction = ClassTransferReviewClassAction;
    using TeacherAction = ClassTransferReviewTeacherAction;

    ClassTransferReviewDecisionResult result;
    const auto addClassTargetIssue = [
        &result
    ](
        const IssueCode code,
        const int packageClassIndex,
        const std::optional<Domain::ClassId>& target
        )
    {
        Issue issue{code, packageClassIndex};
        issue.targetClassId = target;
        result.issues.push_back(std::move(issue));
    };
    const auto addTeacherTargetIssue = [
        &result
    ](
        const IssueCode code,
        const std::string& teacherKey,
        const std::optional<Domain::TeacherId>& target
        )
    {
        Issue issue{code, -1, teacherKey};
        issue.targetTeacherId = target;
        result.issues.push_back(std::move(issue));
    };

    std::map<int, const ClassTransferReviewClassCandidate*> classCandidates;
    for (const auto& candidate : request.classes)
    {
        if (candidate.packageClassIndex < 0)
        {
            result.issues.push_back(
                Issue{IssueCode::InvalidClassIndex,
                      candidate.packageClassIndex}
                );
            continue;
        }
        classCandidates.emplace(candidate.packageClassIndex, &candidate);
    }

    std::map<std::string, const ClassTransferReviewTeacherCandidate*>
        teacherCandidates;
    for (const auto& candidate : request.teachers)
    {
        if (candidate.teacherKey.empty())
        {
            result.issues.push_back(Issue{IssueCode::EmptyTeacherKey});
            continue;
        }
        teacherCandidates.emplace(candidate.teacherKey, &candidate);
    }

    const auto validClassAction = [](const ClassAction action)
    {
        switch (action)
        {
        case ClassAction::Create:
        case ClassAction::Replace:
        case ClassAction::Skip:
            return true;
        case ClassAction::Unselected:
        case ClassAction::Invalid:
            return false;
        }
        return false;
    };
    const auto validTeacherAction = [](const TeacherAction action)
    {
        switch (action)
        {
        case TeacherAction::Create:
        case TeacherAction::KeepExisting:
        case TeacherAction::ReplaceExisting:
            return true;
        case TeacherAction::Unselected:
        case TeacherAction::Invalid:
            return false;
        }
        return false;
    };

    std::map<int, const ClassTransferReviewClassResolution*> classResolutions;
    std::map<Domain::ClassId, std::vector<int>> classTargetClaimants;
    for (const auto& resolution : request.classResolutions)
    {
        if (resolution.packageClassIndex < 0)
        {
            result.issues.push_back(
                Issue{IssueCode::InvalidClassIndex,
                      resolution.packageClassIndex}
                );
            continue;
        }
        const auto candidate = classCandidates.find(
            resolution.packageClassIndex);
        if (candidate == classCandidates.end())
        {
            result.issues.push_back(
                Issue{IssueCode::UnknownClassResolution,
                      resolution.packageClassIndex}
                );
            continue;
        }
        if (classResolutions.contains(resolution.packageClassIndex))
        {
            result.issues.push_back(
                Issue{IssueCode::DuplicateClassResolution,
                      resolution.packageClassIndex}
                );
            continue;
        }
        classResolutions.emplace(resolution.packageClassIndex, &resolution);

        if (!validClassAction(resolution.action))
        {
            result.issues.push_back(
                Issue{IssueCode::InvalidClassAction,
                      resolution.packageClassIndex}
                );
            continue;
        }

        if (resolution.action == ClassAction::Replace)
        {
            if (!resolution.targetClassId)
            {
                addClassTargetIssue(
                    IssueCode::ReplaceClassMissingTarget,
                    resolution.packageClassIndex,
                    resolution.targetClassId);
                continue;
            }
            const auto& matchIds = candidate->second->matchingClassIds;
            if (std::find(
                    matchIds.cbegin(),
                    matchIds.cend(),
                    *resolution.targetClassId
                    ) == matchIds.cend())
            {
                addClassTargetIssue(
                    IssueCode::ClassTargetNotInMatchSet,
                    resolution.packageClassIndex,
                    resolution.targetClassId);
                continue;
            }
            auto& claimants = classTargetClaimants[*resolution.targetClassId];
            if (!claimants.empty())
            {
                addClassTargetIssue(
                    IssueCode::DuplicateClassReplacementTarget,
                    resolution.packageClassIndex,
                    resolution.targetClassId);
            }
            claimants.push_back(resolution.packageClassIndex);
        }
        else if (resolution.targetClassId)
        {
            addClassTargetIssue(
                IssueCode::NonReplaceClassHasTarget,
                resolution.packageClassIndex,
                resolution.targetClassId);
        }
    }

    for (const auto& [packageClassIndex, candidate] : classCandidates)
    {
        static_cast<void>(candidate);
        if (!classResolutions.contains(packageClassIndex))
        {
            result.issues.push_back(
                Issue{IssueCode::MissingClassResolution, packageClassIndex}
                );
        }
    }

    std::map<std::string, const ClassTransferReviewTeacherResolution*>
        teacherResolutions;
    std::map<Domain::TeacherId, std::vector<std::string>>
        teacherReplacementClaimants;
    for (const auto& resolution : request.teacherResolutions)
    {
        if (resolution.teacherKey.empty())
        {
            result.issues.push_back(Issue{IssueCode::EmptyTeacherKey});
            continue;
        }
        const auto candidate = teacherCandidates.find(resolution.teacherKey);
        if (candidate == teacherCandidates.end())
        {
            result.issues.push_back(
                Issue{IssueCode::UnknownTeacherResolution,
                      -1,
                      resolution.teacherKey}
                );
            continue;
        }
        if (teacherResolutions.contains(resolution.teacherKey))
        {
            result.issues.push_back(
                Issue{IssueCode::DuplicateTeacherResolution,
                      -1,
                      resolution.teacherKey}
                );
            continue;
        }
        teacherResolutions.emplace(resolution.teacherKey, &resolution);

        if (!validTeacherAction(resolution.action))
        {
            result.issues.push_back(
                Issue{IssueCode::InvalidTeacherAction,
                      -1,
                      resolution.teacherKey}
                );
            continue;
        }

        const auto& matchIds = candidate->second->matchingTeacherIds;
        if (resolution.action == TeacherAction::Create)
        {
            if (matchIds.size() == 1)
            {
                addTeacherTargetIssue(
                    IssueCode::UniqueTeacherCannotBeCreated,
                    resolution.teacherKey,
                    resolution.targetTeacherId);
            }
            if (resolution.targetTeacherId)
            {
                addTeacherTargetIssue(
                    IssueCode::CreateTeacherHasTarget,
                    resolution.teacherKey,
                    resolution.targetTeacherId);
            }
            continue;
        }

        if (!resolution.targetTeacherId)
        {
            addTeacherTargetIssue(
                IssueCode::TeacherActionMissingTarget,
                resolution.teacherKey,
                resolution.targetTeacherId);
            continue;
        }
        if (std::find(
                matchIds.cbegin(),
                matchIds.cend(),
                *resolution.targetTeacherId
                ) == matchIds.cend())
        {
            addTeacherTargetIssue(
                IssueCode::TeacherTargetNotInMatchSet,
                resolution.teacherKey,
                resolution.targetTeacherId);
            continue;
        }
        if (resolution.action == TeacherAction::ReplaceExisting)
        {
            auto& claimants = teacherReplacementClaimants[
                *resolution.targetTeacherId];
            if (!claimants.empty())
            {
                addTeacherTargetIssue(
                    IssueCode::DuplicateTeacherReplacementTarget,
                    resolution.teacherKey,
                    resolution.targetTeacherId);
            }
            claimants.push_back(resolution.teacherKey);
        }
    }

    for (const auto& [teacherKey, candidate] : teacherCandidates)
    {
        static_cast<void>(candidate);
        if (!teacherResolutions.contains(teacherKey))
        {
            result.issues.push_back(
                Issue{IssueCode::MissingTeacherResolution,
                      -1,
                      teacherKey}
                );
        }
    }

    return result;
}

}
