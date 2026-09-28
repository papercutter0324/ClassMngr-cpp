#pragma once

#include "next/domain/operation_result.h"
#include "next/domain/teacher_profile.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct TeacherProfileEditRequest final
{
    // A missing checked ID represents an invalid/nonpositive ID supplied at
    // the UI boundary. The use case rejects it before policy or persistence.
    std::optional<Domain::TeacherId> id;
    Domain::TeacherProfileFields fields;

    [[nodiscard]] static TeacherProfileEditRequest fromIntId(
        const int id,
        Domain::TeacherProfileFields fields
        )
    {
        return {
            .id = Domain::TeacherId::fromInt(id),
            .fields = std::move(fields)
        };
    }
};

enum class TeacherProfileValidationSeverity
{
    Warning,
    Error
};

struct TeacherProfileValidationArgument final
{
    std::string key;
    std::u16string value;

    friend bool operator==(
        const TeacherProfileValidationArgument&,
        const TeacherProfileValidationArgument&
    ) = default;
};

class TeacherProfileValidationArguments final
{
public:
    static constexpr std::size_t MaximumCount = 8;
    static constexpr std::size_t MaximumKeyBytes = 64;
    static constexpr std::size_t MaximumValueCodeUnits = 256;

    [[nodiscard]] bool add(TeacherProfileValidationArgument argument)
    {
        if (m_values.size() >= MaximumCount
            || argument.key.size() > MaximumKeyBytes
            || argument.value.size() > MaximumValueCodeUnits)
        {
            return false;
        }

        m_values.push_back(std::move(argument));
        return true;
    }

    [[nodiscard]] std::span<const TeacherProfileValidationArgument> values() const
    {
        return {m_values.data(), m_values.size()};
    }

    friend bool operator==(
        const TeacherProfileValidationArguments&,
        const TeacherProfileValidationArguments&
        ) = default;

private:
    std::vector<TeacherProfileValidationArgument> m_values;
};

struct TeacherProfileValidationIssue final
{
    std::string code;
    std::string field;
    TeacherProfileValidationSeverity severity =
        TeacherProfileValidationSeverity::Error;
    // This bounded value type keeps legacy validation payloads from crossing
    // the boundary as arbitrary Qt values.
    TeacherProfileValidationArguments arguments;

    friend bool operator==(
        const TeacherProfileValidationIssue&,
        const TeacherProfileValidationIssue&
        ) = default;
};

struct TeacherProfileValidationResult final
{
    Domain::TeacherProfileFields normalizedFields;
    std::vector<TeacherProfileValidationIssue> issues;
};

// This is the semantic-validation boundary. Production policy adapters must
// delegate normalization and validation to the existing TeacherValidator;
// this contract intentionally contains no duplicate rules.
class TeacherProfileValidationPolicy
{
public:
    virtual ~TeacherProfileValidationPolicy() = default;

    [[nodiscard]] virtual TeacherProfileValidationResult validate(
        const Domain::TeacherProfile& profile
        ) const = 0;
};

// update and reload are separate so the use case can report a reload failure
// while explicitly preserving that the update already succeeded.
class TeacherProfileEditPersistencePort
{
public:
    virtual ~TeacherProfileEditPersistencePort() = default;

    [[nodiscard]] virtual Domain::Result<void> update(
        const Domain::TeacherProfile& profile
        ) const = 0;

    [[nodiscard]] virtual Domain::Result<Domain::TeacherProfile> reload(
        Domain::TeacherId id
        ) const = 0;
};

enum class TeacherProfileEditFailureKind
{
    InvalidTeacherId,
    ValidationFailed,
    UpdateFailed,
    ReloadFailed
};

struct TeacherProfileEditFailure final
{
    TeacherProfileEditFailureKind kind;
    Domain::OperationError cause;
    bool writeSucceeded = false;
    std::vector<TeacherProfileValidationIssue> validationIssues;
};

using TeacherProfileEditResult = std::expected<
    Domain::TeacherProfile,
    TeacherProfileEditFailure>;

class TeacherProfileEditUseCase final
{
public:
    [[nodiscard]] static TeacherProfileEditResult execute(
        const TeacherProfileEditRequest& request,
        const TeacherProfileValidationPolicy& validation,
        const TeacherProfileEditPersistencePort& persistence
        )
    {
        if (!request.id)
        {
            return std::unexpected(TeacherProfileEditFailure{
                .kind = TeacherProfileEditFailureKind::InvalidTeacherId,
                .cause = {
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Teacher ID must be positive.",
                    .recoverable = true
                }
            });
        }

        const Domain::TeacherProfile candidate{
            .id = *request.id,
            .fields = request.fields
        };

        const TeacherProfileValidationResult validationResult =
            validation.validate(candidate);
        const bool hasValidationErrors = std::any_of(
            validationResult.issues.begin(),
            validationResult.issues.end(),
            [](const TeacherProfileValidationIssue& issue)
            {
                return issue.severity == TeacherProfileValidationSeverity::Error;
            }
            );
        if (hasValidationErrors)
        {
            return std::unexpected(TeacherProfileEditFailure{
                .kind = TeacherProfileEditFailureKind::ValidationFailed,
                .cause = {
                    .code = Domain::ErrorCode::Validation,
                    .message = "Teacher profile validation failed.",
                    .recoverable = true
                },
                .validationIssues = validationResult.issues
            });
        }

        const Domain::TeacherProfile normalizedCandidate{
            .id = candidate.id,
            .fields = validationResult.normalizedFields
        };
        const Domain::Result<void> updateResult =
            persistence.update(normalizedCandidate);
        if (!updateResult)
        {
            return std::unexpected(TeacherProfileEditFailure{
                .kind = TeacherProfileEditFailureKind::UpdateFailed,
                .cause = updateResult.error()
            });
        }

        Domain::Result<Domain::TeacherProfile> reloadResult =
            persistence.reload(candidate.id);
        if (!reloadResult)
        {
            return std::unexpected(TeacherProfileEditFailure{
                .kind = TeacherProfileEditFailureKind::ReloadFailed,
                .cause = reloadResult.error(),
                .writeSucceeded = true
            });
        }

        return std::move(reloadResult).value();
    }
};

} // namespace ClassMngr::Next::Application
