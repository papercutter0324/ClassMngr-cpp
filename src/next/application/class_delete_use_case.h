#pragma once

#include "next/application/class_delete.h"

namespace ClassMngr::Next::Application
{

class ClassDeleteUseCase final
{
public:
    [[nodiscard]] static ClassDeleteResult execute(
        const ClassDeleteRequest& request,
        const ClassDeletePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return ClassDeleteResult::failure(validation.error());
        }

        return port.deleteClass(request);
    }
};

} // namespace ClassMngr::Next::Application
