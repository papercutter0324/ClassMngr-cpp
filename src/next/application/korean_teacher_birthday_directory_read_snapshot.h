#pragma once

#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// The Korean teacher fields needed to build birthday schedule candidates.
// Values remain raw so the feature schedule keeps responsibility for trimming
// and display-name fallback behavior.
struct KoreanTeacherBirthdayDirectoryEntry final
{
    std::u16string birthday;
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;

    friend bool operator==(
        const KoreanTeacherBirthdayDirectoryEntry&,
        const KoreanTeacherBirthdayDirectoryEntry&
        ) = default;
};

using KoreanTeacherBirthdayDirectorySnapshot =
    std::vector<KoreanTeacherBirthdayDirectoryEntry>;

using KoreanTeacherBirthdayDirectoryReadResult =
    Domain::Result<KoreanTeacherBirthdayDirectorySnapshot>;

} // namespace ClassMngr::Next::Application
