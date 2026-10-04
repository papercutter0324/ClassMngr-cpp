#pragma once

#include "next/application/my_classes_student_count_batch_read_snapshot.h"

namespace ClassMngr::Next::Application
{

using MyClassesStudentCountBatchReadResult =
    Domain::Result<MyClassesStudentCountBatchReadSnapshot>;

class MyClassesStudentCountBatchReadPort
{
public:
    virtual ~MyClassesStudentCountBatchReadPort() = default;

    [[nodiscard]] virtual MyClassesStudentCountBatchReadResult
    readMyClassesStudentCounts(
        const std::vector<Domain::ClassId>& classIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
