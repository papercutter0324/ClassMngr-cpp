#pragma once

#include <QString>

class Teacher;
class ClassInfoRecord;

namespace SidebarNodeNaming
{
QString formatClassDisplayName(
    const ClassInfoRecord& classInfo
    );

QString formatTeacherDisplayName(
    const Teacher& teacher
    );
}