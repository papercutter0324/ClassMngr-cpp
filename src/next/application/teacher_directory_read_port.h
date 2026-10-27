#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// The directory keeps only the identity and the four strings used for the
// legacy sort and teacher display fallback. It never materializes profiles.
struct TeacherDirectoryEntry final
{
    Domain::TeacherId teacherId;
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;

    friend bool operator==(
        const TeacherDirectoryEntry&,
        const TeacherDirectoryEntry&
        ) = default;
};

struct TeacherDirectoryReadSnapshot final
{
    std::vector<TeacherDirectoryEntry> teachers;

    friend bool operator==(
        const TeacherDirectoryReadSnapshot&,
        const TeacherDirectoryReadSnapshot&
        ) = default;
};

using TeacherDirectoryReadResult = Domain::Result<TeacherDirectoryReadSnapshot>;

class TeacherDirectoryReadPort
{
public:
    virtual ~TeacherDirectoryReadPort() = default;

    [[nodiscard]] virtual TeacherDirectoryReadResult readTeacherDirectory()
        const = 0;
};

} // namespace ClassMngr::Next::Application
