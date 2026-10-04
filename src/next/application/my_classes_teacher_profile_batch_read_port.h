#pragma once

#include "next/application/my_classes_teacher_profile_batch_read_snapshot.h"

namespace ClassMngr::Next::Application
{

using MyClassesTeacherProfileBatchReadResult =
    Domain::Result<MyClassesTeacherProfileBatchReadSnapshot>;

class MyClassesTeacherProfileBatchReadPort
{
public:
    virtual ~MyClassesTeacherProfileBatchReadPort() = default;

    [[nodiscard]] virtual MyClassesTeacherProfileBatchReadResult
    readMyClassesTeacherProfiles(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
