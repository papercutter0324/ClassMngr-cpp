#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassesListEntry final
{
    Domain::ClassId classId;
    std::u16string className;

    friend bool operator==(
        const ClassesListEntry&,
        const ClassesListEntry&
        ) = default;
};

struct ClassesListSnapshot final
{
    std::vector<ClassesListEntry> classes;

    friend bool operator==(
        const ClassesListSnapshot&,
        const ClassesListSnapshot&
        ) = default;
};

} // namespace ClassMngr::Next::Application
