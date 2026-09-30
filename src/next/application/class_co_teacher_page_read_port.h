#pragma once

#include "next/application/class_co_teacher_page_read_snapshot.h"

namespace ClassMngr::Next::Application
{

using ClassCoTeacherPageReadResult =
    Domain::Result<ClassCoTeacherPageReadSnapshot>;

class ClassCoTeacherPageReadPort
{
public:
    virtual ~ClassCoTeacherPageReadPort() = default;

    [[nodiscard]] virtual ClassCoTeacherPageReadResult readClassCoTeacherPage(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
