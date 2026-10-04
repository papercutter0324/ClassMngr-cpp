#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct NativeEnglishTeacherBirthdayDirectoryEntry final
{
    std::u16string name;
    std::u16string position;
    std::u16string birthday;

    friend bool operator==(
        const NativeEnglishTeacherBirthdayDirectoryEntry&,
        const NativeEnglishTeacherBirthdayDirectoryEntry&
        ) = default;
};

using NativeEnglishTeacherBirthdayDirectorySnapshot =
    std::vector<NativeEnglishTeacherBirthdayDirectoryEntry>;

} // namespace ClassMngr::Next::Application
