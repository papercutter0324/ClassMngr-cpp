#pragma once

#include "next/application/initial_setup_teacher_choices_read_snapshot.h"

namespace ClassMngr::Next::Application
{

class InitialSetupTeacherChoicesReadPort
{
public:
    virtual ~InitialSetupTeacherChoicesReadPort() = default;

    [[nodiscard]] virtual InitialSetupTeacherChoicesResult
    readInitialSetupTeacherChoices() const = 0;
};

} // namespace ClassMngr::Next::Application
