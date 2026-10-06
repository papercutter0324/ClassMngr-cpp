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

class TeacherCreatePort
{
public:
    virtual ~TeacherCreatePort() = default;

    [[nodiscard]] virtual TeacherCreateResult createTeacher(
        const TeacherCreateRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
