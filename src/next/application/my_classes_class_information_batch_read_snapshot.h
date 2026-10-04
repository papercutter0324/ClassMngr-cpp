#pragma once

#include "next/application/my_classes_class_information_snapshot.h"
#include "next/domain/operation_result.h"

#include <vector>

namespace ClassMngr::Next::Application
{

struct MyClassesClassInformationBatchReadEntry final
{
    Domain::ClassId classId;
    Domain::Result<MyClassesClassInformationFields> information;
};

using MyClassesClassInformationBatchReadSnapshot =
    std::vector<MyClassesClassInformationBatchReadEntry>;

} // namespace ClassMngr::Next::Application
