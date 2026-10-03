#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassTeacherAssignmentReadRow final
{
    std::optional<Domain::TeacherId> teacherId;

    friend bool operator==(
        const ClassTeacherAssignmentReadRow&,
        const ClassTeacherAssignmentReadRow&
        ) = default;
};

struct ClassTeacherAssignmentsReadSnapshot final
{
    // One row per regular class, including classes without an assigned teacher.
    std::vector<ClassTeacherAssignmentReadRow> assignments;

    friend bool operator==(
        const ClassTeacherAssignmentsReadSnapshot&,
        const ClassTeacherAssignmentsReadSnapshot&
        ) = default;
};

using ClassTeacherAssignmentsReadResult =
    Domain::Result<ClassTeacherAssignmentsReadSnapshot>;

} // namespace ClassMngr::Next::Application
