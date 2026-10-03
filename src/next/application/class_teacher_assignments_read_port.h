#pragma once

#include "next/application/class_teacher_assignments_read_snapshot.h"

namespace ClassMngr::Next::Application
{

class ClassTeacherAssignmentsReadPort
{
public:
    virtual ~ClassTeacherAssignmentsReadPort() = default;

    [[nodiscard]] virtual ClassTeacherAssignmentsReadResult
    readClassTeacherAssignments() const = 0;
};

} // namespace ClassMngr::Next::Application
