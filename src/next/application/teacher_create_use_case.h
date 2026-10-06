#pragma once

#include "next/application/teacher_create.h"

namespace ClassMngr::Next::Application
{

class TeacherCreateUseCase final
{
public:
    [[nodiscard]] static TeacherCreateResult execute(
        const TeacherCreateRequest& request,
        const TeacherCreatePort& port
        )
    {
        return port.createTeacher(request);
    }
};

} // namespace ClassMngr::Next::Application
