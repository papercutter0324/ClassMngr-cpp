#pragma once

#include "next/application/class_transfer_apply.h"

namespace ClassMngr::Next::Application
{

class ClassTransferApplyUseCase final
{
public:
    [[nodiscard]] static ClassTransferApplyResult execute(
        const ClassTransferApplyCommand& command,
        const ClassTransferApplyPort& port
        )
    {
        return port.apply(command);
    }
};

} // namespace ClassMngr::Next::Application
