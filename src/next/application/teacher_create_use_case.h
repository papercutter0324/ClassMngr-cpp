#pragma once

#include "next/application/teacher_create.h"

namespace ClassMngr::Next::Application
{

class TeacherCreateUseCase final
{
public:
    [[nodiscard]] static TeacherCreateResult execute(
        const TeacherCreateRequest& request,
        const TeacherCreateValidationPolicy& validationPolicy,
        const TeacherCreatePort& port
        )
    {
        const Domain::Result<void> ready = port.preflight();
        if (!ready)
        {
            return TeacherCreateResult::failure(ready.error());
        }

        const TeacherCreateValidationResult validated =
            validationPolicy.validate(request.fields);
        if (!validated)
        {
            return TeacherCreateResult::failure(validated.error());
        }

        return port.createTeacher({.fields = validated.value()});
    }
};

} // namespace ClassMngr::Next::Application
