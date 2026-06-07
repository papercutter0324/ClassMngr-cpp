#pragma once

#include <QString>

struct ClassInfo;
struct Teacher;

namespace SidebarNodeNaming
{
QString formatClassDisplayName(
    const ClassInfo& classInfo,
    const Teacher& teacher
    );

QString formatTeacherDisplayName(
    const Teacher& teacher
    );
}