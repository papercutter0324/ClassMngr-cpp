#pragma once

#include "next/application/selected_class_grade_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using SelectedClassGradeReadResult =
    Domain::Result<SelectedClassGradeReadSnapshot>;

class SelectedClassGradeReadPort
{
public:
    virtual ~SelectedClassGradeReadPort() = default;

    [[nodiscard]] virtual SelectedClassGradeReadResult readSelectedClassGrade(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
