#pragma once

#include "next/application/my_classes_class_information_batch_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using MyClassesClassInformationBatchReadResult =
    Domain::Result<MyClassesClassInformationBatchReadSnapshot>;

class MyClassesClassInformationBatchReadPort
{
public:
    virtual ~MyClassesClassInformationBatchReadPort() = default;

    [[nodiscard]] virtual MyClassesClassInformationBatchReadResult
    readMyClassesClassInformationBatch(
        const std::vector<Domain::ClassId>& classIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
