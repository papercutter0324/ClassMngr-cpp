#pragma once

#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
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

}
