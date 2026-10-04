#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/teacher_profile_fields.h"

#include <vector>

namespace ClassMngr::Next::Application
{

struct MyClassesTeacherProfileBatchReadEntry final
{
    Domain::TeacherId teacherId;
    Domain::Result<Domain::TeacherProfileFields> profile;
};

using MyClassesTeacherProfileBatchReadSnapshot =
    std::vector<MyClassesTeacherProfileBatchReadEntry>;

} // namespace ClassMngr::Next::Application
