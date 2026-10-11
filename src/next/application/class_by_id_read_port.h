#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

struct ClassByIdReadSnapshot final
{
    Domain::ClassId classId;
    std::u16string className;

    friend bool operator==(
        const ClassByIdReadSnapshot&,
        const ClassByIdReadSnapshot&
        ) = default;
};

using ClassByIdReadResult = Domain::Result<ClassByIdReadSnapshot>;

class ClassByIdReadPort
{
public:
    virtual ~ClassByIdReadPort() = default;

    [[nodiscard]] virtual ClassByIdReadResult readClassById(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
