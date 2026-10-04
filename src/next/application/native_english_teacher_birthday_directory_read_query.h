#pragma once

#include "next/application/native_english_teacher_birthday_directory_read_port.h"

namespace ClassMngr::Next::Application
{

class NativeEnglishTeacherBirthdayDirectoryReadQuery final
{
public:
    explicit NativeEnglishTeacherBirthdayDirectoryReadQuery(
        const NativeEnglishTeacherBirthdayDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] NativeEnglishTeacherBirthdayDirectoryReadResult execute() const
    {
        return m_port.readNativeEnglishTeacherBirthdayDirectory();
    }

private:
    const NativeEnglishTeacherBirthdayDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
