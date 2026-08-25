#include "academic_calendar_schedule.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace
{
constexpr int DaysPerWeek = 7;
constexpr int MaximumTermWeeks = 53;

int termIndex(AcademicTerm term)
{
    return static_cast<int>(term);
}

QJsonObject scheduleToJson(const AcademicYearSchedule& schedule)
{
    QJsonArray weeks;
    for (const int count : schedule.weeks)
    {
        weeks.append(count);
    }

    return {
        {
            QStringLiteral("winterStart"),
            schedule.winterStart.toString(Qt::ISODate)
        },
        {
            QStringLiteral("weeks"),
            weeks
        }
    };
}

bool scheduleFromJson(
    int termYear,
    const QJsonValue& value,
    AcademicYearSchedule* schedule
    )
{
    if (!schedule || !value.isObject())
    {
        return false;
    }

    const QJsonObject object = value.toObject();
    const QDate winterStart =
        QDate::fromString(
            object.value(QStringLiteral("winterStart")).toString(),
            Qt::ISODate
            );
    const QJsonArray weeks =
        object.value(QStringLiteral("weeks")).toArray();

    if (
        termYear < AcademicCalendarSchedule::FirstTermYear
        || !winterStart.isValid()
        || winterStart.dayOfWeek() != Qt::Monday
        || weeks.size() != AcademicTermCount
        )
    {
        return false;
    }

    AcademicYearSchedule parsed;
    parsed.termYear = termYear;
    parsed.winterStart = winterStart;

    for (int index = 0; index < AcademicTermCount; ++index)
    {
        const int count = weeks.at(index).toInt();
        if (count < 1 || count > MaximumTermWeeks)
        {
            return false;
        }

        parsed.weeks[index] = count;
    }

    *schedule = parsed;
    return true;
}

bool loadProfile(
    const QJsonObject& profiles,
    const QString& profileName,
    QMap<int, AcademicYearSchedule>* destination
    )
{
    if (!destination)
    {
        return false;
    }

    const QJsonValue profileValue = profiles.value(profileName);
    if (!profileValue.isObject())
    {
        return false;
    }

    QMap<int, AcademicYearSchedule> parsed;
    const QJsonObject profile = profileValue.toObject();

    for (auto iterator = profile.begin(); iterator != profile.end(); ++iterator)
    {
        bool yearOk = false;
        const int termYear = iterator.key().toInt(&yearOk);
        AcademicYearSchedule schedule;

        if (
            !yearOk
            || !scheduleFromJson(termYear, iterator.value(), &schedule)
            )
        {
            return false;
        }

        parsed.insert(termYear, schedule);
    }

    *destination = parsed;
    return true;
}

QJsonObject profileToJson(
    const QMap<int, AcademicYearSchedule>& schedules
    )
{
    QJsonObject profile;

    for (auto iterator = schedules.cbegin(); iterator != schedules.cend(); ++iterator)
    {
        profile.insert(
            QString::number(iterator.key()),
            scheduleToJson(iterator.value())
            );
    }

    return profile;
}
}

bool AcademicYearSchedule::isValid() const
{
    if (
        termYear < AcademicCalendarSchedule::FirstTermYear
        || !winterStart.isValid()
        || winterStart.dayOfWeek() != Qt::Monday
        )
    {
        return false;
    }

    return std::ranges::all_of(
        weeks,
        [](int count)
        {
            return count >= 1 && count <= MaximumTermWeeks;
        }
        );
}

QDate AcademicYearSchedule::termStart(AcademicTerm term) const
{
    if (!isValid())
    {
        return {};
    }

    QDate start = winterStart;
    for (int index = 0; index < termIndex(term); ++index)
    {
        start = start.addDays(weeks[index] * DaysPerWeek);
    }

    return start;
}

QDate AcademicYearSchedule::endDate() const
{
    if (!isValid())
    {
        return {};
    }

    int totalWeeks = 0;
    for (const int count : weeks)
    {
        totalWeeks += count;
    }

    return winterStart.addDays(totalWeeks * DaysPerWeek);
}

QDate AcademicCalendarSchedule::initialWinterStart()
{
    return QDate(2025, 12, 29);
}

std::array<int, AcademicTermCount>
AcademicCalendarSchedule::defaultWeeks(SchoolLevel level)
{
    if (level == SchoolLevel::Elementary)
    {
        return {11, 19, 11, 11};
    }

    return {11, 19, 4, 18};
}

AcademicYearSchedule AcademicCalendarSchedule::yearSchedule(
    SchoolLevel level,
    int termYear
    ) const
{
    if (termYear < FirstTermYear)
    {
        return {};
    }

    const ScheduleMap& customSchedules = schedules(level);
    AcademicYearSchedule current{
        FirstTermYear,
        initialWinterStart(),
        defaultWeeks(level)
    };

    for (int year = FirstTermYear; year <= termYear; ++year)
    {
        if (year > FirstTermYear)
        {
            current = {
                year,
                current.endDate(),
                defaultWeeks(level)
            };
        }

        const auto custom = customSchedules.constFind(year);
        if (custom != customSchedules.cend())
        {
            current = custom.value();
        }
    }

    return current;
}

AcademicYearSchedule AcademicCalendarSchedule::defaultYearSchedule(
    SchoolLevel level,
    int termYear
    ) const
{
    if (termYear < FirstTermYear)
    {
        return {};
    }

    const QDate winterStart =
        termYear == FirstTermYear
            ? initialWinterStart()
            : yearSchedule(level, termYear - 1).endDate();

    return {
        termYear,
        winterStart,
        defaultWeeks(level)
    };
}

AcademicTermPosition AcademicCalendarSchedule::termAt(
    SchoolLevel level,
    const QDate& date
    ) const
{
    if (!date.isValid() || date < initialWinterStart())
    {
        return {};
    }

    AcademicYearSchedule schedule = yearSchedule(level, FirstTermYear);
    int termYear = FirstTermYear;

    while (date >= schedule.endDate())
    {
        ++termYear;
        schedule = yearSchedule(level, termYear);
    }

    for (int index = AcademicTermCount - 1; index >= 0; --index)
    {
        const AcademicTerm term = static_cast<AcademicTerm>(index);
        const QDate start = schedule.termStart(term);

        if (date >= start)
        {
            const int week = start.daysTo(date) / DaysPerWeek + 1;
            return {
                true,
                termYear,
                term,
                week,
                start.addDays((week - 1) * DaysPerWeek)
            };
        }
    }

    return {};
}

bool AcademicCalendarSchedule::hasCustomYearAfter(int termYear) const
{
    const auto hasLater =
        [termYear](const ScheduleMap& schedules)
        {
            const auto iterator = schedules.upperBound(termYear);
            return iterator != schedules.cend();
        };

    return hasLater(m_elementarySchedules)
        || hasLater(m_middleSchedules);
}

bool AcademicCalendarSchedule::hasSavedSchedules() const
{
    return !m_elementarySchedules.isEmpty()
        && !m_middleSchedules.isEmpty();
}

void AcademicCalendarSchedule::setYearSchedules(
    int termYear,
    const AcademicYearSchedule& elementary,
    const AcademicYearSchedule& middle
    )
{
    if (
        termYear < FirstTermYear
        || !elementary.isValid()
        || !middle.isValid()
        || elementary.termYear != termYear
        || middle.termYear != termYear
        )
    {
        return;
    }

    AcademicYearSchedule previousElementary;
    AcademicYearSchedule previousMiddle;

    if (termYear > FirstTermYear)
    {
        previousElementary =
            yearSchedule(SchoolLevel::Elementary, termYear - 1);
        previousMiddle =
            yearSchedule(SchoolLevel::Middle, termYear - 1);

        auto alignPreviousFall =
            [](AcademicYearSchedule* previous, const QDate& nextWinterStart)
            {
                if (!previous)
                {
                    return false;
                }

                const QDate fallStart =
                    previous->termStart(AcademicTerm::Fall);
                const int days = fallStart.daysTo(nextWinterStart);
                const int weeks = days / DaysPerWeek;

                if (
                    days <= 0
                    || days % DaysPerWeek != 0
                    || weeks > MaximumTermWeeks
                    )
                {
                    return false;
                }

                previous->weeks[termIndex(AcademicTerm::Fall)] = weeks;
                return true;
            };

        if (
            !alignPreviousFall(
                &previousElementary,
                elementary.winterStart
                )
            || !alignPreviousFall(
                &previousMiddle,
                middle.winterStart
                )
            )
        {
            return;
        }
    }

    while (!m_elementarySchedules.isEmpty()
           && m_elementarySchedules.lastKey() > termYear)
    {
        m_elementarySchedules.remove(m_elementarySchedules.lastKey());
    }

    while (!m_middleSchedules.isEmpty()
           && m_middleSchedules.lastKey() > termYear)
    {
        m_middleSchedules.remove(m_middleSchedules.lastKey());
    }

    if (termYear > FirstTermYear)
    {
        m_elementarySchedules.insert(
            termYear - 1,
            previousElementary
            );
        m_middleSchedules.insert(
            termYear - 1,
            previousMiddle
            );
    }

    m_elementarySchedules.insert(termYear, elementary);
    m_middleSchedules.insert(termYear, middle);
}

void AcademicCalendarSchedule::clear()
{
    m_elementarySchedules.clear();
    m_middleSchedules.clear();
}

QJsonObject AcademicCalendarSchedule::toJson() const
{
    return {
        {
            QStringLiteral("version"),
            1
        },
        {
            QStringLiteral("profiles"),
            QJsonObject{
                {
                    QStringLiteral("elementary"),
                    profileToJson(m_elementarySchedules)
                },
                {
                    QStringLiteral("middle"),
                    profileToJson(m_middleSchedules)
                }
            }
        }
    };
}

bool AcademicCalendarSchedule::fromJson(const QJsonObject& root)
{
    if (root.value(QStringLiteral("version")).toInt() != 1)
    {
        clear();
        return false;
    }

    const QJsonValue profilesValue =
        root.value(QStringLiteral("profiles"));
    if (!profilesValue.isObject())
    {
        clear();
        return false;
    }

    ScheduleMap elementary;
    ScheduleMap middle;
    const QJsonObject profiles = profilesValue.toObject();

    if (
        !loadProfile(
            profiles,
            QStringLiteral("elementary"),
            &elementary
            )
        || !loadProfile(
            profiles,
            QStringLiteral("middle"),
            &middle
            )
        )
    {
        clear();
        return false;
    }

    m_elementarySchedules = elementary;
    m_middleSchedules = middle;
    return true;
}

const AcademicCalendarSchedule::ScheduleMap&
AcademicCalendarSchedule::schedules(SchoolLevel level) const
{
    return level == SchoolLevel::Elementary
        ? m_elementarySchedules
        : m_middleSchedules;
}

AcademicCalendarSchedule::ScheduleMap&
AcademicCalendarSchedule::schedules(SchoolLevel level)
{
    return level == SchoolLevel::Elementary
        ? m_elementarySchedules
        : m_middleSchedules;
}
