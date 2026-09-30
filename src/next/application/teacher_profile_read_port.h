#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/teacher_profile_fields.h"

namespace ClassMngr::Next::Application
{

struct TeacherProfileReadSnapshot final
{
    Domain::TeacherId teacherId;
    Domain::TeacherProfileFields fields;

    friend bool operator==(
        const TeacherProfileReadSnapshot&,
        const TeacherProfileReadSnapshot&
        ) = default;
};

using TeacherProfileReadResult = Domain::Result<TeacherProfileReadSnapshot>;

class TeacherProfileReadPort
{
public:
    virtual ~TeacherProfileReadPort() = default;

    [[nodiscard]] virtual TeacherProfileReadResult readTeacherProfile(
        const Domain::TeacherId& id
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
