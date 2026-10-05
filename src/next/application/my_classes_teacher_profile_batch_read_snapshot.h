#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct MyClassesTeacherProfileFields final
{
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;
    std::u16string roomNumber;
    std::u16string wifiName;
    std::u16string wifiPassword;
    std::u16string internetType;
    std::u16string zoomId;
    std::u16string zoomPassword;
    std::u16string projectionType;
    std::u16string notes;

    friend bool operator==(
        const MyClassesTeacherProfileFields&,
        const MyClassesTeacherProfileFields&
        ) = default;
};

struct MyClassesTeacherProfileBatchReadEntry final
{
    Domain::TeacherId teacherId;
    Domain::Result<MyClassesTeacherProfileFields> profile;
};

using MyClassesTeacherProfileBatchReadSnapshot =
    std::vector<MyClassesTeacherProfileBatchReadEntry>;

} // namespace ClassMngr::Next::Application
