#pragma once

#include "next/application/my_classes_class_information_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using MyClassesClassInformationReadResult =
    Domain::Result<MyClassesClassInformationSnapshot>;

class MyClassesClassInformationReadPort
{
public:
    virtual ~MyClassesClassInformationReadPort() = default;

    [[nodiscard]] virtual MyClassesClassInformationReadResult
    readMyClassesClassInformation(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
