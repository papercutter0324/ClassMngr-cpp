#pragma once

#include "next/domain/native_english_teacher_id.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct NativeEnglishTeacherDirectoryEntry final
{
    Domain::NativeEnglishTeacherId id;
    std::u16string name;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string email;
    std::u16string birthday;
    std::u16string nationality;

    friend bool operator==(
        const NativeEnglishTeacherDirectoryEntry&,
        const NativeEnglishTeacherDirectoryEntry&
        ) = default;
};

using NativeEnglishTeacherDirectorySnapshot =
    std::vector<NativeEnglishTeacherDirectoryEntry>;

} // namespace ClassMngr::Next::Application
