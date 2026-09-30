#pragma once

#include "next/application/native_english_teacher_directory_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using NativeEnglishTeacherDirectoryReadResult =
    Domain::Result<NativeEnglishTeacherDirectorySnapshot>;

class NativeEnglishTeacherDirectoryReadPort
{
public:
    virtual ~NativeEnglishTeacherDirectoryReadPort() = default;

    [[nodiscard]] virtual NativeEnglishTeacherDirectoryReadResult
    readNativeEnglishTeacherDirectory() const = 0;
};

} // namespace ClassMngr::Next::Application
