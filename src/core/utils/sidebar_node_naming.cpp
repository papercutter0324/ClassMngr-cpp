#include "sidebar_node_naming.h"

#include "classmngr/engine/class_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"

#include <string>

namespace
{
using PortableClassInfo = classmngr::engine::ClassInfo;
using PortableClassTime = classmngr::engine::ClassTime;
using PortableTeacher = classmngr::engine::Teacher;

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {encoded.constData(), static_cast<std::size_t>(encoded.size())};
}

PortableClassTime toPortable(const ClassTime& source)
{
    return {
        toUtf8(source.day),
        toUtf8(source.startTime),
        toUtf8(source.endTime)
    };
}

PortableClassInfo toPortable(const ClassInfo& source)
{
    PortableClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.classTimes.reserve(source.classTimes.size());
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back(toPortable(time));
    }
    return result;
}

PortableTeacher toPortable(const Teacher& source)
{
    PortableTeacher result;
    result.id = source.id;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.preferredRomanization = toUtf8(source.preferredRomanization);
    result.preferredName = toUtf8(source.preferredName);
    return result;
}
} // namespace

QString SidebarNodeNaming::formatClassDisplayName(
    const ClassInfo& classInfo,
    const Teacher& teacher
    )
{
    const std::string value = classmngr::engine::ClassNamingService::
        classDisplayName(toPortable(classInfo), toPortable(teacher));
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString SidebarNodeNaming::formatTeacherDisplayName(
    const Teacher& teacher
    )
{
    const std::string value = classmngr::engine::ClassNamingService::
        teacherDisplayName(toPortable(teacher));
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

bool SidebarNodeNaming::teacherDisplayLessThan(
    const Teacher& left,
    const Teacher& right
    )
{
    return classmngr::engine::ClassNamingService::teacherDisplayLessThan(
        toPortable(left),
        toPortable(right)
        );
}
