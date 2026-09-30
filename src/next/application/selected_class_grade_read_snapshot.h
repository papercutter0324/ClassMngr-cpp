#pragma once

#include "next/domain/domain_types.h"

#include <string>

namespace ClassMngr::Next::Application
{

struct SelectedClassGradeReadSnapshot final
{
    Domain::ClassId classId;
    std::string classGrade;
};

} // namespace ClassMngr::Next::Application
