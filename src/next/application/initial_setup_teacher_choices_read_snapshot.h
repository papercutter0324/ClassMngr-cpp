#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct InitialSetupTeacherChoice final
{
    Domain::TeacherId teacherId;
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;

    friend bool operator==(
        const InitialSetupTeacherChoice&,
        const InitialSetupTeacherChoice&
        ) = default;
};

struct InitialSetupTeacherChoicesSnapshot final
{
    std::vector<InitialSetupTeacherChoice> teachers;

    friend bool operator==(
        const InitialSetupTeacherChoicesSnapshot&,
        const InitialSetupTeacherChoicesSnapshot&
        ) = default;
};

using InitialSetupTeacherChoicesResult =
    Domain::Result<InitialSetupTeacherChoicesSnapshot>;

} // namespace ClassMngr::Next::Application
