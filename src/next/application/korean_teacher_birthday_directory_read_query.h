#pragma once

#include "next/application/korean_teacher_birthday_directory_read_port.h"

namespace ClassMngr::Next::Application
{

class KoreanTeacherBirthdayDirectoryReadQuery final
{
public:
    explicit KoreanTeacherBirthdayDirectoryReadQuery(
        const KoreanTeacherBirthdayDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] KoreanTeacherBirthdayDirectoryReadResult execute() const
    {
        return m_port.readKoreanTeacherBirthdayDirectory();
    }

private:
    const KoreanTeacherBirthdayDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
