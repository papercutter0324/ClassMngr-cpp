#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/teacher_profile_fields.h"

namespace ClassMngr::Next::Application
{

struct TeacherCreateRequest final
{
    Domain::TeacherProfileFields fields;

    friend bool operator==(
        const TeacherCreateRequest&,
        const TeacherCreateRequest&
        ) = default;
};

using TeacherCreateResult = Domain::Result<Domain::TeacherId>;
using TeacherCreateValidationResult =
    Domain::Result<Domain::TeacherProfileFields>;

// Validation and normalization are supplied by a platform policy that adapts
// the existing domain validator. The application contract stays Qt-free.
class TeacherCreateValidationPolicy
{
public:
    virtual ~TeacherCreateValidationPolicy() = default;

    [[nodiscard]] virtual TeacherCreateValidationResult validate(
        const Domain::TeacherProfileFields& fields
        ) const = 0;
};

class TeacherCreatePort
{
public:
    virtual ~TeacherCreatePort() = default;

    // Keep availability errors ahead of field validation, matching the
    // existing setup flow's database/repository error precedence.
    [[nodiscard]] virtual Domain::Result<void> preflight() const = 0;

    [[nodiscard]] virtual TeacherCreateResult createTeacher(
        const TeacherCreateRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
