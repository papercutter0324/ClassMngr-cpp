#pragma once

#include "next/application/my_classes_student_count.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <vector>

namespace ClassMngr::Next::Application
{

struct MyClassesStudentCountBatchReadEntry final
{
    Domain::ClassId classId;
    Domain::Result<MyClassesStudentCount> studentCount;
};

using MyClassesStudentCountBatchReadSnapshot =
    std::vector<MyClassesStudentCountBatchReadEntry>;

} // namespace ClassMngr::Next::Application
