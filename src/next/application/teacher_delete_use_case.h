#pragma once

#include "next/application/teacher_delete.h"

namespace ClassMngr::Next::Application
{

class TeacherDeleteUseCase final
{
public:
    [[nodiscard]] static TeacherDeleteResult execute(
        const TeacherDeleteRequest& request,
        const TeacherDeletePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return TeacherDeleteResult::failure(validation.error());
        }

        return port.deleteTeacher(request);
    }
};

} // namespace ClassMngr::Next::Application
