#pragma once

#include "next/application/class_notes_save_port.h"

namespace ClassMngr::Next::Application
{

class ClassNotesSaveUseCase final
{
public:
    [[nodiscard]] static ClassNotesSaveResult execute(
        const ClassNotesSaveRequest& request,
        const ClassNotesSavePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return ClassNotesSaveResult::failure(validation.error());
        }

        return port.saveClassNotes(request);
    }
};

} // namespace ClassMngr::Next::Application
