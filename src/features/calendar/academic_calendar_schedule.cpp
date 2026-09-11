#include "academic_calendar_schedule.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QMap>

#include <algorithm>
#include <chrono>
#include <map>

namespace
{
using PortableCalendarDate = classmngr::engine::CalendarDate;
using PortableSchedule = classmngr::engine::AcademicYearSchedule;
using PortableScheduleMap =
    classmngr::engine::AcademicCalendarSchedule::ScheduleMap;

PortableCalendarDate toPortableDate(const QDate& date)
{
    if (!date.isValid())
    {
        return {};
    }

    return {
        std::chrono::year{date.year()},
        std::chrono::month{static_cast<unsigned>(date.month())},
        std::chrono::day{static_cast<unsigned>(date.day())}
    };
}

QDate toQtDate(const PortableCalendarDate& date)
{
    if (!date.ok())
    {
        return {};
    }

    return QDate(
        static_cast<int>(date.year()),
        static_cast<int>(static_cast<unsigned>(date.month())),
        static_cast<int>(static_cast<unsigned>(date.day()))
        );
}

classmngr::engine::SchoolLevel toPortableSchoolLevel(SchoolLevel level)
{
    return level == SchoolLevel::Elementary
        ? classmngr::engine::SchoolLevel::Elementary
        : classmngr::engine::SchoolLevel::Middle;
}

classmngr::engine::AcademicTerm toPortableTerm(AcademicTerm term)
{
    return static_cast<classmngr::engine::AcademicTerm>(
        static_cast<int>(term)
        );
}

AcademicTerm toQtTerm(classmngr::engine::AcademicTerm term)
{
    return static_cast<AcademicTerm>(static_cast<int>(term));
}

PortableSchedule toPortableSchedule(const AcademicYearSchedule& schedule)
{
    return {
        schedule.termYear,
        toPortableDate(schedule.winterStart),
        schedule.weeks
    };
}

AcademicYearSchedule toQtSchedule(const PortableSchedule& schedule)
{
    return {
        schedule.termYear,
        toQtDate(schedule.winterStart),
        schedule.weeks
    };
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

    if (weeks.size() != AcademicTermCount)
    {
        return false;
    }

    AcademicYearSchedule parsed;
    parsed.termYear = termYear;
    parsed.winterStart = winterStart;
    for (int index = 0; index < AcademicTermCount; ++index)
    {
        parsed.weeks[index] = weeks.at(index).toInt();
    }

    if (!parsed.isValid())
    {
        return false;
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

PortableScheduleMap toPortableSchedules(
    const QMap<int, AcademicYearSchedule>& schedules
    )
{
    PortableScheduleMap result;
    for (auto iterator = schedules.cbegin(); iterator != schedules.cend(); ++iterator)
    {
        result.insert({iterator.key(), toPortableSchedule(iterator.value())});
    }
    return result;
}

QJsonObject profileToJson(const PortableScheduleMap& schedules)
{
    QJsonObject profile;
    for (const auto& [termYear, schedule] : schedules)
    {
        profile.insert(
            QString::number(termYear),
            scheduleToJson(toQtSchedule(schedule))
            );
    }
    return profile;
}
}

bool AcademicYearSchedule::isValid() const
{
    return toPortableSchedule(*this).isValid();
}

QDate AcademicYearSchedule::termStart(AcademicTerm term) const
{
    return toQtDate(
        toPortableSchedule(*this).termStart(toPortableTerm(term))
        );
}

QDate AcademicYearSchedule::endDate() const
{
    return toQtDate(toPortableSchedule(*this).endDate());
}

QDate AcademicCalendarSchedule::initialWinterStart()
{
    return toQtDate(
        classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
        );
}

std::array<int, AcademicTermCount>
AcademicCalendarSchedule::defaultWeeks(SchoolLevel level)
{
    return classmngr::engine::AcademicCalendarSchedule::defaultWeeks(
        toPortableSchoolLevel(level)
        );
}

AcademicYearSchedule AcademicCalendarSchedule::yearSchedule(
    SchoolLevel level,
    int termYear
    ) const
{
    return toQtSchedule(
        m_engine.yearSchedule(
            toPortableSchoolLevel(level),
            termYear
            )
        );
}

AcademicYearSchedule AcademicCalendarSchedule::defaultYearSchedule(
    SchoolLevel level,
    int termYear
    ) const
{
    return toQtSchedule(
        m_engine.defaultYearSchedule(
            toPortableSchoolLevel(level),
            termYear
            )
        );
}

AcademicTermPosition AcademicCalendarSchedule::termAt(
    SchoolLevel level,
    const QDate& date
    ) const
{
    const classmngr::engine::AcademicTermPosition position =
        m_engine.termAt(
            toPortableSchoolLevel(level),
            toPortableDate(date)
            );

    return {
        position.valid,
        position.termYear,
        toQtTerm(position.term),
        position.week,
        toQtDate(position.weekStart)
    };
}

bool AcademicCalendarSchedule::hasCustomYearAfter(int termYear) const
{
    return m_engine.hasCustomYearAfter(termYear);
}

bool AcademicCalendarSchedule::hasSavedSchedules() const
{
    return m_engine.hasSavedSchedules();
}

void AcademicCalendarSchedule::setYearSchedules(
    int termYear,
    const AcademicYearSchedule& elementary,
    const AcademicYearSchedule& middle
    )
{
    m_engine.setYearSchedules(
        termYear,
        toPortableSchedule(elementary),
        toPortableSchedule(middle)
        );
}

void AcademicCalendarSchedule::clear()
{
    m_engine.clear();
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
                    profileToJson(
                        m_engine.customSchedules(
                            classmngr::engine::SchoolLevel::Elementary
                            )
                        )
                },
                {
                    QStringLiteral("middle"),
                    profileToJson(
                        m_engine.customSchedules(
                            classmngr::engine::SchoolLevel::Middle
                            )
                        )
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

    QMap<int, AcademicYearSchedule> elementary;
    QMap<int, AcademicYearSchedule> middle;
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

    if (!m_engine.replaceSchedules(
            toPortableSchedules(elementary),
            toPortableSchedules(middle)
            ))
    {
        clear();
        return false;
    }

    return true;
}
