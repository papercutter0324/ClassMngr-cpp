#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassCoTeacherTeacherChoice final
{
    Domain::TeacherId teacherId;
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string roomNumber;
    std::u16string internetType;
    std::u16string wifiName;
    std::u16string wifiPassword;
    std::u16string projectionType;
    std::u16string zoomId;
    std::u16string zoomPassword;

    friend bool operator==(
        const ClassCoTeacherTeacherChoice&,
        const ClassCoTeacherTeacherChoice&
        ) = default;
};

struct ClassCoTeacherTeacherChoicesSnapshot final
{
    std::vector<ClassCoTeacherTeacherChoice> teachers;

    friend bool operator==(
        const ClassCoTeacherTeacherChoicesSnapshot&,
        const ClassCoTeacherTeacherChoicesSnapshot&
        ) = default;
};

using ClassCoTeacherTeacherChoicesResult =
    Domain::Result<ClassCoTeacherTeacherChoicesSnapshot>;

} // namespace ClassMngr::Next::Application
