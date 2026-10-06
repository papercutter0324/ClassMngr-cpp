#pragma once

#include "next/application/class_create.h"

namespace ClassMngr::Next::Application
{

class ClassCreateUseCase final
{
public:
    [[nodiscard]] static ClassCreateResult execute(
        const ClassCreateRequest& request,
        const ClassCreatePort& port
        )
    {
        return port.createClass(request);
    }
};

} // namespace ClassMngr::Next::Application
