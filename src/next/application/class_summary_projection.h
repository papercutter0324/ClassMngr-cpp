#pragma once

#include "next/domain/domain_types.h"
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

// These limits keep navigation and one selected detail record compact. An
// adapter or query owner must filter or paginate a larger scope before it
// crosses this boundary.
inline constexpr std::size_t kSummaryMaxIdentifierLength = 256;
inline constexpr std::size_t kTeacherSummaryMaxEntries = 4'096;
inline constexpr std::size_t kClassSummaryMaxEntries = 4'096;
inline constexpr std::size_t kTeacherSummaryMaxDisplayNameLength = 256;
inline constexpr std::size_t kTeacherSummaryMaxFacilitiesLength = 1'024;
inline constexpr std::size_t kTeacherSummaryMaxNotesLength = 2'048;
inline constexpr std::size_t kClassSummaryMaxGradeLength = 64;
inline constexpr std::size_t kClassSummaryMaxLevelLength = 64;
inline constexpr std::size_t kClassSummaryMaxDisplayLabelLength = 256;
inline constexpr std::size_t kClassSummaryMaxMeetingTextLength = 256;
inline constexpr std::size_t kClassSummaryMaxStudentCount = 10'000;
inline constexpr std::size_t kSelectedClassDetailsMaxClassNotesLength = 4'096;
inline constexpr std::size_t
    kSelectedClassDetailsMaxTeacherDisplayNameLength = 256;
inline constexpr std::size_t
    kSelectedClassDetailsMaxTeacherFacilitiesLength = 1'024;
inline constexpr std::size_t kSelectedClassDetailsMaxTeacherNotesLength =
    2'048;

// A teacher row contains only the identity and bounded text needed by
// navigation and the selected-class detail surface. It owns no rich teacher
// record graph or UI object.
struct TeacherSummary final
{
    Domain::TeacherId id;
    std::string displayName;
    std::string facilities;
    std::string notes;

    friend bool operator==(
        const TeacherSummary&,
        const TeacherSummary&
        ) = default;
};

// A class row contains navigation metadata only. An absent teacherId is the
// explicit missing-teacher fallback; a present teacherId must resolve through
// the projection's TeacherSummaryIndex.
struct ClassSummary final
{
    Domain::ClassId id;
    std::optional<Domain::TeacherId> teacherId;
    std::string grade;
    std::string level;
    std::string displayLabel;
    std::string meetingText;
    std::size_t studentCount = 0;
    std::int32_t order = 0;

    [[nodiscard]] bool hasTeacher() const noexcept
    {
        return teacherId.has_value();
    }

    friend bool operator==(
        const ClassSummary&,
        const ClassSummary&
        ) = default;
};

// This is the one detail value for the selected class. Teacher fields are
// copied only for that active selection and are intentionally not a nested
// rich record graph. Empty teacher fields are valid for the missing-teacher
// fallback; all text remains explicitly bounded by the factory.
struct SelectedClassDetails final
{
    Domain::ClassId classId;
    std::optional<Domain::TeacherId> teacherId;
    std::string classNotes;
    std::string teacherDisplayName;
    std::string teacherFacilities;
    std::string teacherNotes;

    [[nodiscard]] bool hasTeacher() const noexcept
    {
        return teacherId.has_value();
    }

    friend bool operator==(
        const SelectedClassDetails&,
        const SelectedClassDetails&
        ) = default;
};

using ClassDetails = SelectedClassDetails;
using SubPrepClassSummary = ClassSummary;
using SubPrepClassDetails = SelectedClassDetails;

namespace ClassSummaryProjectionDetail
{

[[nodiscard]] inline bool isBlank(
    std::string_view value
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

[[nodiscard]] inline bool isValidIdentifier(
    std::string_view value
    ) noexcept
{
    return !isBlank(value) && value.size() <= kSummaryMaxIdentifierLength;
}

[[nodiscard]] inline bool isRequiredText(
    const std::string& value,
    const std::size_t maxLength
    ) noexcept
{
    return !isBlank(value) && value.size() <= maxLength;
}

[[nodiscard]] inline bool isOptionalText(
    const std::string& value,
    const std::size_t maxLength
    ) noexcept
{
    return value.size() <= maxLength;
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

template <typename TypedId>
[[nodiscard]] inline bool isValidId(
    const TypedId& id
    ) noexcept
{
    return isValidIdentifier(id.value());
}

template <typename TypedId>
[[nodiscard]] inline bool containsId(
    const std::vector<TypedId>& ids,
    const TypedId& candidate
    ) noexcept
{
    return std::find(ids.cbegin(), ids.cend(), candidate) != ids.cend();
}

[[nodiscard]] inline Domain::Result<void> validateTeacher(
    const TeacherSummary& teacher,
    const std::vector<Domain::TeacherId>& existingIds
    )
{
    if (!isValidId(teacher.id))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Teacher identifier must be non-blank and bounded."
                )
            );
    }

    if (containsId(existingIds, teacher.id))
    {
        return Domain::Result<void>::failure(
            invalidInput("Teacher identifiers must be unique.")
            );
    }

    if (!isRequiredText(
            teacher.displayName,
            kTeacherSummaryMaxDisplayNameLength
            )
        || !isOptionalText(
            teacher.facilities,
            kTeacherSummaryMaxFacilitiesLength
            )
        || !isOptionalText(teacher.notes, kTeacherSummaryMaxNotesLength))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Teacher display name must be required and bounded; facilities and notes must be bounded."
                )
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateClass(
    const ClassSummary& classSummary,
    const std::vector<Domain::ClassId>& existingIds,
    const std::vector<Domain::TeacherId>& teacherIds
    )
{
    if (!isValidId(classSummary.id))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Class identifier must be non-blank and bounded."
                )
            );
    }

    if (containsId(existingIds, classSummary.id))
    {
        return Domain::Result<void>::failure(
            invalidInput("Class identifiers must be unique.")
            );
    }

    if (classSummary.teacherId.has_value())
    {
        if (!isValidId(*classSummary.teacherId))
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Optional class teacher identifier must be non-blank and bounded."
                    )
                );
        }

        if (!containsId(teacherIds, *classSummary.teacherId))
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Class references an unknown teacher identifier."
                    )
                );
        }
    }

    if (!isRequiredText(classSummary.grade, kClassSummaryMaxGradeLength)
        || !isRequiredText(classSummary.level, kClassSummaryMaxLevelLength)
        || !isRequiredText(
            classSummary.displayLabel,
            kClassSummaryMaxDisplayLabelLength
            )
        || !isRequiredText(
            classSummary.meetingText,
            kClassSummaryMaxMeetingTextLength
            ))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Class grade, level, display label, and meeting text must be non-blank and bounded."
                )
            );
    }

    if (classSummary.studentCount > kClassSummaryMaxStudentCount)
    {
        return Domain::Result<void>::failure(
            invalidInput("Class student count exceeds its bounded limit.")
            );
    }

    if (classSummary.order < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Class summary order must not be negative.")
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateSelectedDetails(
    const SelectedClassDetails& details,
    const std::vector<ClassSummary>& classes,
    const std::vector<Domain::TeacherId>& teacherIds
    )
{
    if (!isValidId(details.classId))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Selected class identifier must be non-blank and bounded."
                )
            );
    }

    const auto selectedClass = std::find_if(
        classes.cbegin(),
        classes.cend(),
        [&details](const ClassSummary& candidate)
        {
            return candidate.id == details.classId;
        }
        );
    if (selectedClass == classes.cend())
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Selected class details must identify an existing class summary."
                )
            );
    }

    if (details.teacherId != selectedClass->teacherId)
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Selected teacher identity must match the selected class summary."
                )
            );
    }

    if (details.teacherId.has_value()
        && (!isValidId(*details.teacherId)
            || !containsId(teacherIds, *details.teacherId)))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Selected details reference an unknown teacher identifier."
                )
            );
    }

    if (!isOptionalText(
            details.classNotes,
            kSelectedClassDetailsMaxClassNotesLength
            )
        || !isOptionalText(
            details.teacherDisplayName,
            kSelectedClassDetailsMaxTeacherDisplayNameLength
            )
        || !isOptionalText(
            details.teacherFacilities,
            kSelectedClassDetailsMaxTeacherFacilitiesLength
            )
        || !isOptionalText(
            details.teacherNotes,
            kSelectedClassDetailsMaxTeacherNotesLength
            ))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Selected class and teacher detail text must be bounded."
                )
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateTeachers(
    const std::vector<TeacherSummary>& teachers
    )
{
    if (teachers.size() > kTeacherSummaryMaxEntries)
    {
        return Domain::Result<void>::failure(
            invalidInput("Teacher summary collection exceeds its bounded limit.")
            );
    }

    std::vector<Domain::TeacherId> teacherIds;
    teacherIds.reserve(teachers.size());
    for (const auto& teacher : teachers)
    {
        const auto validation = validateTeacher(teacher, teacherIds);
        if (!validation)
        {
            return validation;
        }

        teacherIds.push_back(teacher.id);
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateInput(
    const std::vector<TeacherSummary>& teachers,
    const std::vector<ClassSummary>& classes,
    const std::optional<SelectedClassDetails>& selectedDetails
    )
{
    const auto teacherValidation = validateTeachers(teachers);
    if (!teacherValidation)
    {
        return teacherValidation;
    }

    if (classes.size() > kClassSummaryMaxEntries)
    {
        return Domain::Result<void>::failure(
            invalidInput("Class summary collection exceeds its bounded limit.")
            );
    }

    std::vector<Domain::TeacherId> teacherIds;
    teacherIds.reserve(teachers.size());
    for (const auto& teacher : teachers)
    {
        teacherIds.push_back(teacher.id);
    }

    std::vector<Domain::ClassId> classIds;
    classIds.reserve(classes.size());
    for (const auto& classSummary : classes)
    {
        const auto validation = validateClass(
            classSummary,
            classIds,
            teacherIds
            );
        if (!validation)
        {
            return validation;
        }

        classIds.push_back(classSummary.id);
    }

    if (selectedDetails.has_value())
    {
        const auto validation = validateSelectedDetails(
            *selectedDetails,
            classes,
            teacherIds
            );
        if (!validation)
        {
            return validation;
        }
    }

    return Domain::Result<void>::success();
}

}

// The index owns only copied teacher summaries. Lookup returns an optional
// value copy, so a caller never receives a mutable pointer into the projection.
class TeacherSummaryIndex final
{
public:
    using Input = std::vector<TeacherSummary>;
    using Summary = TeacherSummary;

    TeacherSummaryIndex() = default;

    [[nodiscard]] static Domain::Result<TeacherSummaryIndex> create(
        Input teachers
        )
    {
        const auto validation = ClassSummaryProjectionDetail::validateTeachers(
            teachers
            );
        if (!validation)
        {
            return Domain::Result<TeacherSummaryIndex>::failure(
                validation.error()
                );
        }

        return Domain::Result<TeacherSummaryIndex>::success(
            TeacherSummaryIndex(std::move(teachers))
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& teachers
        )
    {
        return ClassSummaryProjectionDetail::validateTeachers(teachers);
    }

    [[nodiscard]] const Input& summaries() const noexcept
    {
        return m_summaries;
    }

    [[nodiscard]] const Input& teachers() const noexcept
    {
        return summaries();
    }

    [[nodiscard]] std::optional<Summary> find(
        const Domain::TeacherId& id
        ) const
    {
        const auto summary = std::find_if(
            m_summaries.cbegin(),
            m_summaries.cend(),
            [&id](const Summary& candidate)
            {
                return candidate.id == id;
            }
            );
        if (summary == m_summaries.cend())
        {
            return std::nullopt;
        }

        return *summary;
    }

    [[nodiscard]] std::optional<Summary> lookup(
        const Domain::TeacherId& id
        ) const
    {
        return find(id);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_summaries.empty();
    }

    friend bool operator==(
        const TeacherSummaryIndex&,
        const TeacherSummaryIndex&
        ) = default;

private:
    explicit TeacherSummaryIndex(
        Input summaries
        )
        : m_summaries(std::move(summaries))
    {
    }

    Input m_summaries;
};

struct ClassSummaryProjectionInput final
{
    std::vector<TeacherSummary> teachers;
    std::vector<ClassSummary> classes;
    std::optional<SelectedClassDetails> selectedDetails;

    friend bool operator==(
        const ClassSummaryProjectionInput&,
        const ClassSummaryProjectionInput&
        ) = default;
};

using ClassSummaryInput = ClassSummaryProjectionInput;

// This immutable application snapshot owns compact copied summaries and at
// most one selected detail record. The adapter/query owner can release rich
// teacher, class, roster, and schedule records immediately after create(). No
// rich source graph or external owner is retained by this projection.
class ClassSummaryProjection final
{
public:
    using Input = ClassSummaryProjectionInput;
    using Class = ClassSummary;
    using Teacher = TeacherSummary;
    using Details = SelectedClassDetails;

    ClassSummaryProjection() = default;

    [[nodiscard]] static Domain::Result<ClassSummaryProjection> create(
        Input input
        )
    {
        const auto validation = ClassSummaryProjectionDetail::validateInput(
            input.teachers,
            input.classes,
            input.selectedDetails
            );
        if (!validation)
        {
            return Domain::Result<ClassSummaryProjection>::failure(
                validation.error()
                );
        }

        auto teacherIndex = TeacherSummaryIndex::create(
            std::move(input.teachers)
            );
        if (!teacherIndex)
        {
            return Domain::Result<ClassSummaryProjection>::failure(
                teacherIndex.error()
                );
        }

        return Domain::Result<ClassSummaryProjection>::success(
            ClassSummaryProjection(
                std::move(teacherIndex.value()),
                std::move(input.classes),
                std::move(input.selectedDetails)
                )
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& input
        )
    {
        return ClassSummaryProjectionDetail::validateInput(
            input.teachers,
            input.classes,
            input.selectedDetails
            );
    }

    [[nodiscard]] const std::vector<Class>& classes() const noexcept
    {
        return m_classes;
    }

    [[nodiscard]] const std::vector<Class>& classSummaries() const noexcept
    {
        return classes();
    }

    [[nodiscard]] const TeacherSummaryIndex& teacherIndex() const noexcept
    {
        return m_teacherIndex;
    }

    [[nodiscard]] const TeacherSummaryIndex& teachers() const noexcept
    {
        return teacherIndex();
    }

    [[nodiscard]] std::optional<Class> findClass(
        const Domain::ClassId& id
        ) const
    {
        const auto summary = std::find_if(
            m_classes.cbegin(),
            m_classes.cend(),
            [&id](const Class& candidate)
            {
                return candidate.id == id;
            }
            );
        if (summary == m_classes.cend())
        {
            return std::nullopt;
        }

        return *summary;
    }

    [[nodiscard]] std::optional<Class> lookupClass(
        const Domain::ClassId& id
        ) const
    {
        return findClass(id);
    }

    [[nodiscard]] std::optional<Teacher> findTeacher(
        const Domain::TeacherId& id
        ) const
    {
        return m_teacherIndex.find(id);
    }

    [[nodiscard]] std::optional<Teacher> lookupTeacher(
        const Domain::TeacherId& id
        ) const
    {
        return findTeacher(id);
    }

    [[nodiscard]] std::optional<Details> selectedDetails() const
    {
        return m_selectedDetails;
    }

    [[nodiscard]] std::optional<Details> selectedClassDetails() const
    {
        return selectedDetails();
    }

    [[nodiscard]] std::optional<Details> findSelectedDetails(
        const Domain::ClassId& id
        ) const
    {
        if (!m_selectedDetails.has_value()
            || m_selectedDetails->classId != id)
        {
            return std::nullopt;
        }

        return m_selectedDetails;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_classes.empty();
    }

    friend bool operator==(
        const ClassSummaryProjection&,
        const ClassSummaryProjection&
        ) = default;

private:
    ClassSummaryProjection(
        TeacherSummaryIndex teacherIndex,
        std::vector<Class> classes,
        std::optional<Details> selectedDetails
        )
        : m_teacherIndex(std::move(teacherIndex)),
          m_classes(std::move(classes)),
          m_selectedDetails(std::move(selectedDetails))
    {
    }

    TeacherSummaryIndex m_teacherIndex;
    std::vector<Class> m_classes;
    std::optional<Details> m_selectedDetails;
};

using ClassSummarySnapshot = ClassSummaryProjection;

} // namespace ClassMngr::Next::Application
