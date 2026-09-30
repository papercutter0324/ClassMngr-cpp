#pragma once

#include "next/application/native_english_teacher_directory_read_port.h"

namespace ClassMngr::Next::Application
{

class NativeEnglishTeacherDirectoryReadQuery final
{
public:
    explicit NativeEnglishTeacherDirectoryReadQuery(
        const NativeEnglishTeacherDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] NativeEnglishTeacherDirectoryReadResult execute() const
    {
        return m_port.readNativeEnglishTeacherDirectory();
    }

private:
    const NativeEnglishTeacherDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
