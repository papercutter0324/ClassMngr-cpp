#pragma once

#include "next/application/class_co_teacher_teacher_choices_read_snapshot.h"

namespace ClassMngr::Next::Application
{

class ClassCoTeacherTeacherChoicesReadPort
{
public:
    virtual ~ClassCoTeacherTeacherChoicesReadPort() = default;

    [[nodiscard]] virtual ClassCoTeacherTeacherChoicesResult
    readClassCoTeacherTeacherChoices() const = 0;
};

} // namespace ClassMngr::Next::Application
