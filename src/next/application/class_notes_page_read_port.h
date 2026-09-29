#pragma once

#include "next/application/class_notes_page_read_snapshot.h"

namespace ClassMngr::Next::Application
{

using ClassNotesPageReadResult =
    Domain::Result<ClassNotesPageReadSnapshot>;

class ClassNotesPageReadPort
{
public:
    virtual ~ClassNotesPageReadPort() = default;

    [[nodiscard]] virtual ClassNotesPageReadResult readClassNotesPage(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
