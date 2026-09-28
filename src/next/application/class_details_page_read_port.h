#pragma once

#include "next/application/class_details_page_snapshot.h"

namespace ClassMngr::Next::Application
{

using ClassDetailsPageReadResult =
    Domain::Result<ClassDetailsPageReadSnapshot>;

// Reads one screen-shaped snapshot for the selected class. Implementations
// copy all values and do not retain the typed request reference.
class ClassDetailsPageReadPort
{
public:
    virtual ~ClassDetailsPageReadPort() = default;

    [[nodiscard]] virtual ClassDetailsPageReadResult readClassDetailsPage(
        const Domain::ClassId& classId
        ) = 0;
};

} // namespace ClassMngr::Next::Application
