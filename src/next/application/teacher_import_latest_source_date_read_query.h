#pragma once

#include "next/application/teacher_import_latest_source_date_read_port.h"

namespace ClassMngr::Next::Application
{

class TeacherImportLatestSourceDateReadQuery final
{
public:
    explicit TeacherImportLatestSourceDateReadQuery(
        const TeacherImportLatestSourceDateReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] TeacherImportLatestSourceDateReadResult execute() const
    {
        return m_port.readLatestTeacherImportSourceDate();
    }

private:
    const TeacherImportLatestSourceDateReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
