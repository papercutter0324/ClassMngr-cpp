#pragma once

#include "next/application/korean_teacher_birthday_directory_read_snapshot.h"

namespace ClassMngr::Next::Application
{

class KoreanTeacherBirthdayDirectoryReadPort
{
public:
    virtual ~KoreanTeacherBirthdayDirectoryReadPort() = default;

    [[nodiscard]] virtual KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const = 0;
};

} // namespace ClassMngr::Next::Application
