#include "upcoming_birthday_schedule.h"

#include "classmngr/engine/upcoming_birthday_schedule.h"

#include <QByteArray>

#include <chrono>
#include <string>
#include <vector>

namespace
{
using PortableBirthday = classmngr::engine::UpcomingBirthday;
using PortableBirthdayGroup =
    classmngr::engine::UpcomingBirthdayGroup;
using PortableGsTeamMember = classmngr::engine::GsTeamMember;
using PortableNativeEnglishTeacher =
    classmngr::engine::NativeEnglishTeacher;
using PortableSchedule = classmngr::engine::UpcomingBirthdaySchedule;
using PortableTeacher = classmngr::engine::Teacher;
using PortableDate = classmngr::engine::CalendarDate;

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

PortableDate toPortable(const QDate& value)
{
    if (!value.isValid())
    {
        return {};
    }

    return {
        std::chrono::year{value.year()},
        std::chrono::month{static_cast<unsigned>(value.month())},
        std::chrono::day{static_cast<unsigned>(value.day())}
    };
}

QDate fromPortable(const PortableDate& value)
{
    if (!value.ok())
    {
        return {};
    }

    return {
        static_cast<int>(value.year()),
        static_cast<int>(static_cast<unsigned>(value.month())),
        static_cast<int>(static_cast<unsigned>(value.day()))
    };
}

PortableTeacher toPortable(const Teacher& source)
{
    PortableTeacher result;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.preferredRomanization = toUtf8(source.preferredRomanization);
    result.preferredName = toUtf8(source.preferredName);
    result.birthday = toUtf8(source.birthday);
    return result;
}

PortableNativeEnglishTeacher toPortable(
    const NativeEnglishTeacher& source
    )
{
    PortableNativeEnglishTeacher result;
    result.name = toUtf8(source.name);
    result.position = toUtf8(source.position);
    result.birthday = toUtf8(source.birthday);
    return result;
}

PortableGsTeamMember toPortable(const GsTeamMember& source)
{
    PortableGsTeamMember result;
    result.name = toUtf8(source.name);
    result.koreanName = toUtf8(source.koreanName);
    result.position = toUtf8(source.position);
    result.birthday = toUtf8(source.birthday);
    return result;
}

UpcomingBirthdayGroup fromPortable(PortableBirthdayGroup group)
{
    switch (group)
    {
    case PortableBirthdayGroup::NativeEnglishTeacher:
        return UpcomingBirthdayGroup::NativeEnglishTeacher;

    case PortableBirthdayGroup::GsTeam:
        return UpcomingBirthdayGroup::GsTeam;

    case PortableBirthdayGroup::KoreanTeacher:
    default:
        return UpcomingBirthdayGroup::KoreanTeacher;
    }
}

UpcomingBirthday fromPortable(const PortableBirthday& source)
{
    return {
        fromPortable(source.date),
        QString::fromUtf8(
            source.displayName.data(),
            static_cast<qsizetype>(source.displayName.size())
            ),
        QString::fromUtf8(
            source.position.data(),
            static_cast<qsizetype>(source.position.size())
            ),
        fromPortable(source.group)
    };
}
} // namespace

bool UpcomingBirthdaySchedule::isEmpty() const
{
    return today.isEmpty() && thisWeek.isEmpty() && nextWeek.isEmpty();
}

UpcomingBirthdaySchedule UpcomingBirthdaySchedule::build(
    const QList<Teacher>& teachers,
    const QList<NativeEnglishTeacher>& nativeEnglishTeachers,
    const QList<GsTeamMember>& gsTeamMembers,
    const QDate& referenceDate
    )
{
    std::vector<PortableTeacher> portableTeachers;
    portableTeachers.reserve(static_cast<std::size_t>(teachers.size()));
    for (const Teacher& teacher : teachers)
    {
        portableTeachers.push_back(toPortable(teacher));
    }

    std::vector<PortableNativeEnglishTeacher> portableNativeTeachers;
    portableNativeTeachers.reserve(
        static_cast<std::size_t>(nativeEnglishTeachers.size())
        );
    for (const NativeEnglishTeacher& teacher : nativeEnglishTeachers)
    {
        portableNativeTeachers.push_back(toPortable(teacher));
    }

    std::vector<PortableGsTeamMember> portableGsTeamMembers;
    portableGsTeamMembers.reserve(
        static_cast<std::size_t>(gsTeamMembers.size())
        );
    for (const GsTeamMember& member : gsTeamMembers)
    {
        portableGsTeamMembers.push_back(toPortable(member));
    }

    const PortableSchedule portable = PortableSchedule::build(
        portableTeachers,
        portableNativeTeachers,
        portableGsTeamMembers,
        toPortable(referenceDate)
        );

    UpcomingBirthdaySchedule result;
    const auto append = [](const std::vector<PortableBirthday>& source,
                           QList<UpcomingBirthday>* destination)
    {
        destination->reserve(static_cast<qsizetype>(source.size()));
        for (const PortableBirthday& birthday : source)
        {
            destination->append(fromPortable(birthday));
        }
    };
    append(portable.today, &result.today);
    append(portable.thisWeek, &result.thisWeek);
    append(portable.nextWeek, &result.nextWeek);
    return result;
}
