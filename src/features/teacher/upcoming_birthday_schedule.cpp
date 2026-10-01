#include "upcoming_birthday_schedule.h"

#include "next/application/upcoming_birthday_schedule_use_case.h"

#include <QByteArray>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace
{

using ClassMngr::Next::Application::UpcomingBirthdayCandidate;
using ClassMngr::Next::Application::UpcomingBirthdayDate;
using ClassMngr::Next::Application::UpcomingBirthdayOccurrence;
using ClassMngr::Next::Application::UpcomingBirthdayStaffGroup;
using ClassMngr::Next::Application::UpcomingBirthdayScheduleUseCase;

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return bytes.toStdString();
}

std::u16string utf16(const QString& value)
{
    return value.toStdU16String();
}

UpcomingBirthdayGroup featureGroup(
    const UpcomingBirthdayStaffGroup group
    )
{
    switch (group)
    {
    case UpcomingBirthdayStaffGroup::KoreanTeacher:
        return UpcomingBirthdayGroup::KoreanTeacher;
    case UpcomingBirthdayStaffGroup::NativeEnglishTeacher:
        return UpcomingBirthdayGroup::NativeEnglishTeacher;
    case UpcomingBirthdayStaffGroup::GsTeam:
        return UpcomingBirthdayGroup::GsTeam;
    }

    return UpcomingBirthdayGroup::KoreanTeacher;
}

UpcomingBirthday convertOccurrence(
    const UpcomingBirthdayOccurrence& occurrence
    )
{
    return {
        QDate(
            occurrence.date.year,
            occurrence.date.month,
            occurrence.date.day
            ),
        QString::fromUtf16(
            occurrence.displayName.data(),
            static_cast<qsizetype>(occurrence.displayName.size())
            ),
        QString::fromUtf16(
            occurrence.position.data(),
            static_cast<qsizetype>(occurrence.position.size())
            ),
        featureGroup(occurrence.group)
    };
}

void appendOccurrences(
    QList<UpcomingBirthday>* destination,
    const std::vector<UpcomingBirthdayOccurrence>& source
    )
{
    if (!destination)
    {
        return;
    }

    for (const UpcomingBirthdayOccurrence& occurrence : source)
    {
        destination->append(convertOccurrence(occurrence));
    }
}

bool birthdayLessThan(
    const UpcomingBirthday& left,
    const UpcomingBirthday& right
    )
{
    if (left.date != right.date)
    {
        return left.date < right.date;
    }

    const int nameComparison = QString::localeAwareCompare(
        left.displayName,
        right.displayName
        );
    if (nameComparison != 0)
    {
        return nameComparison < 0;
    }

    if (left.group != right.group)
    {
        return static_cast<int>(left.group) < static_cast<int>(right.group);
    }

    return QString::localeAwareCompare(left.position, right.position) < 0;
}

void sortBirthdays(QList<UpcomingBirthday>* birthdays)
{
    if (birthdays)
    {
        std::sort(birthdays->begin(), birthdays->end(), birthdayLessThan);
    }
}

}

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
    std::vector<UpcomingBirthdayCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(
        teachers.size() + nativeEnglishTeachers.size() + gsTeamMembers.size()
        ));

    for (const Teacher& teacher : teachers)
    {
        candidates.push_back({
            .birthdayMonthDay = utf8(teacher.birthday.trimmed()),
            .displayName = utf16(teacher.preferredDisplayName().trimmed()),
            .group = UpcomingBirthdayStaffGroup::KoreanTeacher
        });
    }

    for (const NativeEnglishTeacher& teacher : nativeEnglishTeachers)
    {
        candidates.push_back({
            .birthdayMonthDay = utf8(teacher.birthday.trimmed()),
            .displayName = utf16(teacher.name.trimmed()),
            .position = utf16(teacher.position.trimmed()),
            .group = UpcomingBirthdayStaffGroup::NativeEnglishTeacher
        });
    }

    for (const GsTeamMember& member : gsTeamMembers)
    {
        const QString displayName = member.name.trimmed().isEmpty()
            ? member.koreanName.trimmed()
            : member.name.trimmed();
        candidates.push_back({
            .birthdayMonthDay = utf8(member.birthday.trimmed()),
            .displayName = utf16(displayName),
            .position = utf16(member.position.trimmed()),
            .group = UpcomingBirthdayStaffGroup::GsTeam
        });
    }

    const UpcomingBirthdayDate applicationReferenceDate{
        referenceDate.year(),
        referenceDate.month(),
        referenceDate.day()
    };
    const ClassMngr::Next::Application::UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
            candidates,
            applicationReferenceDate
            );

    UpcomingBirthdaySchedule result;
    appendOccurrences(&result.today, schedule.today);
    appendOccurrences(&result.thisWeek, schedule.thisWeek);
    appendOccurrences(&result.nextWeek, schedule.nextWeek);

    sortBirthdays(&result.today);
    sortBirthdays(&result.thisWeek);
    sortBirthdays(&result.nextWeek);
    return result;
}
