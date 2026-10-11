#pragma once

#include "next/application/class_transfer_apply.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class ClassTransferApplyUseCase final
{
public:
    [[nodiscard]] static ClassTransferApplyResult execute(
        ClassTransferApplyCommand command,
        const ClassTransferApplyPort& port
        )
    {
        return port.apply(std::move(command));
    }
};

} // namespace ClassMngr::Next::Application
