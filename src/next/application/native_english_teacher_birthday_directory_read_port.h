#pragma once

#include "next/application/native_english_teacher_birthday_directory_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using NativeEnglishTeacherBirthdayDirectoryReadResult =
    Domain::Result<NativeEnglishTeacherBirthdayDirectorySnapshot>;

class NativeEnglishTeacherBirthdayDirectoryReadPort
{
public:
    virtual ~NativeEnglishTeacherBirthdayDirectoryReadPort() = default;

    [[nodiscard]] virtual NativeEnglishTeacherBirthdayDirectoryReadResult
    readNativeEnglishTeacherBirthdayDirectory() const = 0;
};

} // namespace ClassMngr::Next::Application
