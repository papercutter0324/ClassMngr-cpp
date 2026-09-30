#pragma once

#include "next/application/classes_list_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using ClassesListReadResult = Domain::Result<ClassesListSnapshot>;

class ClassesListReadPort
{
public:
    virtual ~ClassesListReadPort() = default;

    [[nodiscard]] virtual ClassesListReadResult readClassesList() const = 0;
};

} // namespace ClassMngr::Next::Application
