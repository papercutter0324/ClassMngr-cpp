#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct TeacherDisplayNameFields final
{
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;

    friend bool operator==(
        const TeacherDisplayNameFields&,
        const TeacherDisplayNameFields&
        ) = default;
};

struct TeacherDisplayNameBatchReadSnapshot final
{
    Domain::TeacherId teacherId;
    TeacherDisplayNameFields fields;

    friend bool operator==(
        const TeacherDisplayNameBatchReadSnapshot&,
        const TeacherDisplayNameBatchReadSnapshot&
        ) = default;
};

using TeacherDisplayNameBatchReadResult =
    Domain::Result<std::vector<TeacherDisplayNameBatchReadSnapshot>>;

class TeacherDisplayNameBatchReadPort
{
public:
    virtual ~TeacherDisplayNameBatchReadPort() = default;

    [[nodiscard]] virtual TeacherDisplayNameBatchReadResult
    readTeacherDisplayNames(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
