#pragma once

#include "next/application/my_classes_class_summary_batch_read_snapshot.h"

namespace ClassMngr::Next::Application
{

class MyClassesClassSummaryBatchReadPort
{
public:
    virtual ~MyClassesClassSummaryBatchReadPort() = default;

    [[nodiscard]] virtual MyClassesClassSummaryBatchReadResult
    readMyClassesClassSummaries(
        const std::vector<Domain::ClassId>& classIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
