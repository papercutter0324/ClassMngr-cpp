#include "upcoming_birthday_schedule.h"

#include <algorithm>

namespace
{
QDate parseBirthday(const QString& birthday)
{
    return QDate::fromString(
        QStringLiteral("2000-%1").arg(birthday.trimmed()),
        QStringLiteral("yyyy-MM-dd")
        );
}

QDate occurrenceForYear(const QDate& birthday, int year)
{
    QDate occurrence(year, birthday.month(), birthday.day());

    if (!occurrence.isValid()
        && birthday.month() == 2
        && birthday.day() == 29)
    {
        occurrence = QDate(year, 2, 28);
    }

    return occurrence;
}

QDate nextOccurrenceInRange(
    const QDate& birthday,
    const QDate& rangeStart,
    const QDate& rangeEnd
    )
{
    for (int year = rangeStart.year(); year <= rangeEnd.year(); ++year)
    {
        const QDate occurrence = occurrenceForYear(birthday, year);
        if (occurrence >= rangeStart && occurrence <= rangeEnd)
        {
            return occurrence;
        }
    }

    return {};
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

void appendBirthday(
    UpcomingBirthdaySchedule* schedule,
    const QString& birthdayValue,
    const QString& displayNameValue,
    const QString& position,
    UpcomingBirthdayGroup group,
    const QDate& referenceDate,
    const QDate& thisWeekEnd,
    const QDate& nextWeekEnd
    )
{
    if (!schedule)
    {
        return;
    }

    const QDate birthday = parseBirthday(birthdayValue);
    const QString displayName = displayNameValue.trimmed();
    if (!birthday.isValid() || displayName.isEmpty())
    {
        return;
    }

    const QDate occurrence = nextOccurrenceInRange(
        birthday,
        referenceDate,
        nextWeekEnd
        );
    if (!occurrence.isValid())
    {
        return;
    }

    const UpcomingBirthday entry{
        occurrence,
        displayName,
        position.trimmed(),
        group
    };

    if (occurrence == referenceDate)
    {
        schedule->today.append(entry);
    }
    else if (occurrence <= thisWeekEnd)
    {
        schedule->thisWeek.append(entry);
    }
    else
    {
        schedule->nextWeek.append(entry);
    }
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
    UpcomingBirthdaySchedule result;

    if (!referenceDate.isValid())
    {
        return result;
    }

    const QDate thisWeekEnd = referenceDate.addDays(
        7 - referenceDate.dayOfWeek()
        );
    const QDate nextWeekEnd = thisWeekEnd.addDays(7);

    for (const Teacher& teacher : teachers)
    {
        appendBirthday(
            &result,
            teacher.birthday,
            teacher.preferredDisplayName(),
            {},
            UpcomingBirthdayGroup::KoreanTeacher,
            referenceDate,
            thisWeekEnd,
            nextWeekEnd
            );
    }

    for (const NativeEnglishTeacher& teacher : nativeEnglishTeachers)
    {
        appendBirthday(
            &result,
            teacher.birthday,
            teacher.name,
            teacher.position,
            UpcomingBirthdayGroup::NativeEnglishTeacher,
            referenceDate,
            thisWeekEnd,
            nextWeekEnd
            );
    }

    for (const GsTeamMember& member : gsTeamMembers)
    {
        const QString displayName = member.name.trimmed().isEmpty()
            ? member.koreanName
            : member.name;
        appendBirthday(
            &result,
            member.birthday,
            displayName,
            member.position,
            UpcomingBirthdayGroup::GsTeam,
            referenceDate,
            thisWeekEnd,
            nextWeekEnd
            );
    }

    sortBirthdays(&result.today);
    sortBirthdays(&result.thisWeek);
    sortBirthdays(&result.nextWeek);
    return result;
}
