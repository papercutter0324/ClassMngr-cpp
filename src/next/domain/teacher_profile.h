#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/teacher_profile_fields.h"

namespace ClassMngr::Next::Domain
{

struct TeacherProfile final
{
    TeacherId id;
    TeacherProfileFields fields;

    friend bool operator==(
        const TeacherProfile&,
        const TeacherProfile&
        ) = default;
};

} // namespace ClassMngr::Next::Domain
