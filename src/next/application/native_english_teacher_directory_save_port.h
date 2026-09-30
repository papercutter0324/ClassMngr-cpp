#pragma once

#include "next/application/native_english_teacher_directory_save.h"

namespace ClassMngr::Next::Application
{

class NativeEnglishTeacherDirectorySavePort
{
public:
    virtual ~NativeEnglishTeacherDirectorySavePort() = default;

    [[nodiscard]] virtual Domain::Result<void>
    saveNativeEnglishTeacherDirectory(
        const NativeEnglishTeacherDirectorySaveRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
