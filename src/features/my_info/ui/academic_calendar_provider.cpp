#include "academic_calendar_provider.h"

#include "data/data_service.h"
#include "features/my_info/calendar_settings_keys.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QVariantMap>

namespace
{
const QString AcademicCalendarSettingsKey =
    QStringLiteral("calendar/academicSchedule/v1");

QDate firstMondayInMonth(int year, int month)
{
    const QDate first(year, month, 1);
    if (!first.isValid())
    {
        return {};
    }

    const int offset =
        (Qt::Monday - first.dayOfWeek() + 7) % 7;
    return first.addDays(offset);
}

int qtDayOfWeek(int qmlLocaleDay)
{
    return qmlLocaleDay == 0
        ? Qt::Sunday
        : qmlLocaleDay;
}

int qmlDayOfWeek(Qt::DayOfWeek qtDay)
{
    return qtDay == Qt::Sunday
        ? 0
        : static_cast<int>(qtDay);
}

int defaultFirstDayOfWeek()
{
    return qmlDayOfWeek(QLocale().firstDayOfWeek());
}

int normalizedFirstDayOfWeek(const QVariant& value)
{
    if (!value.isValid())
    {
        return defaultFirstDayOfWeek();
    }

    bool ok = false;
    const int day = value.toInt(&ok);

    if (ok && day >= 0 && day <= 6)
    {
        return day;
    }

    return defaultFirstDayOfWeek();
}
}

AcademicCalendarProvider::AcademicCalendarProvider(
    DataService* dataService,
    QObject* parent
    )
    : QObject(parent)
    , m_dataService(dataService)
{
    reload();
}

int AcademicCalendarProvider::revision() const
{
    return m_revision;
}

int AcademicCalendarProvider::firstDayOfWeek() const
{
    return m_firstDayOfWeek;
}

const AcademicCalendarSchedule& AcademicCalendarProvider::schedule() const
{
    return m_schedule;
}

QString AcademicCalendarProvider::monthTitle(
    int year,
    int zeroBasedMonth
    ) const
{
    const int month = zeroBasedMonth + 1;
    const QDate monday = firstMondayInMonth(year, month);
    const QString monthYear =
        QLocale().toString(
            QDate(year, month, 1),
            QStringLiteral("MMMM yyyy")
            );

    if (!monday.isValid())
    {
        return monthYear;
    }

    const AcademicTermPosition elementary =
        m_schedule.termAt(SchoolLevel::Elementary, monday);
    const AcademicTermPosition middle =
        m_schedule.termAt(SchoolLevel::Middle, monday);

    if (!elementary.valid || !middle.valid)
    {
        return monthYear;
    }

    if (
        elementary.term == middle.term
        && elementary.week == middle.week
        )
    {
        return tr("%1 — %2")
            .arg(monthYear, termWeekText(elementary));
    }

    return tr("%1 — Elem: %2 · MS: %3")
        .arg(
            monthYear,
            termWeekText(elementary),
            termWeekText(middle)
            );
}

QVariantList AcademicCalendarProvider::weekRows(
    int year,
    int zeroBasedMonth,
    int localeFirstDay
    ) const
{
    QVariantList rows;
    const QDate first(year, zeroBasedMonth + 1, 1);

    if (!first.isValid())
    {
        return rows;
    }

    const int rowFirstDay = qtDayOfWeek(localeFirstDay);
    const int firstOffset =
        (first.dayOfWeek() - rowFirstDay + 7) % 7;
    const QDate firstRowStart = first.addDays(-firstOffset);

    for (int row = 0; row < 6; ++row)
    {
        const QDate rowStart = firstRowStart.addDays(row * 7);
        const int mondayOffset =
            (Qt::Monday - rowStart.dayOfWeek() + 7) % 7;
        const QDate monday = rowStart.addDays(mondayOffset);
        const AcademicTermPosition elementary =
            m_schedule.termAt(SchoolLevel::Elementary, monday);
        const AcademicTermPosition middle =
            m_schedule.termAt(SchoolLevel::Middle, monday);

        QVariantMap values;
        values.insert(
            QStringLiteral("elementaryWeek"),
            elementary.valid ? elementary.week : 0
            );
        values.insert(
            QStringLiteral("middleWeek"),
            middle.valid ? middle.week : 0
            );
        values.insert(
            QStringLiteral("elementaryTooltip"),
            tooltipText(SchoolLevel::Elementary, elementary)
            );
        values.insert(
            QStringLiteral("middleTooltip"),
            tooltipText(SchoolLevel::Middle, middle)
            );

        rows.append(values);
    }

    return rows;
}

int AcademicCalendarProvider::termYearForDate(
    const QDateTime& dateTime
    ) const
{
    return termYearForDate(dateTime.date());
}

int AcademicCalendarProvider::termYearForDate(const QDate& date) const
{
    const AcademicTermPosition position =
        m_schedule.termAt(SchoolLevel::Elementary, date);

    return position.valid
        ? position.termYear
        : AcademicCalendarSchedule::FirstTermYear;
}

bool AcademicCalendarProvider::hasCustomYearAfter(int termYear) const
{
    return m_schedule.hasCustomYearAfter(termYear);
}

void AcademicCalendarProvider::saveYearSchedules(
    int termYear,
    const AcademicYearSchedule& elementary,
    const AcademicYearSchedule& middle
    )
{
    m_schedule.setYearSchedules(
        termYear,
        elementary,
        middle
        );
    persist();
    ++m_revision;
    emit revisionChanged();
}

void AcademicCalendarProvider::setFirstDayOfWeek(int firstDayOfWeek)
{
    const int normalized =
        firstDayOfWeek == 1 ? 1 : 0;

    if (m_firstDayOfWeek == normalized)
    {
        return;
    }

    m_firstDayOfWeek = normalized;
    persistFirstDayOfWeek();
    ++m_revision;
    emit firstDayOfWeekChanged();
    emit revisionChanged();
}

void AcademicCalendarProvider::reload()
{
    m_schedule.clear();
    loadOptions();

    if (m_dataService && m_dataService->isOpen())
    {
        const QByteArray json =
            m_dataService
                ->loadSetting(
                    AcademicCalendarSettingsKey,
                    QString()
                    )
                .toString()
                .toUtf8();

        QJsonParseError error;
        const QJsonDocument document =
            QJsonDocument::fromJson(json, &error);

        if (
            error.error == QJsonParseError::NoError
            && document.isObject()
            )
        {
            const bool loaded =
                m_schedule.fromJson(document.object());
            Q_UNUSED(loaded);
        }
    }

    ++m_revision;
    emit firstDayOfWeekChanged();
    emit revisionChanged();
}

void AcademicCalendarProvider::loadOptions()
{
    m_firstDayOfWeek =
        m_dataService && m_dataService->isOpen()
            ? normalizedFirstDayOfWeek(
                m_dataService->loadSetting(
                    CalendarSettingsKeys::FirstDayOfWeek,
                    defaultFirstDayOfWeek()
                    )
                )
            : defaultFirstDayOfWeek();
}

QString AcademicCalendarProvider::termName(AcademicTerm term) const
{
    switch (term)
    {
    case AcademicTerm::Winter:
        return tr("Winter");

    case AcademicTerm::Spring:
        return tr("Spring");

    case AcademicTerm::Summer:
        return tr("Summer");

    case AcademicTerm::Fall:
        return tr("Fall");
    }

    return {};
}

QString AcademicCalendarProvider::termWeekText(
    const AcademicTermPosition& position
    ) const
{
    if (!position.valid)
    {
        return QStringLiteral("—");
    }

    return tr("%1 Wk %2")
        .arg(termName(position.term))
        .arg(position.week);
}

QString AcademicCalendarProvider::tooltipText(
    SchoolLevel level,
    const AcademicTermPosition& position
    ) const
{
    if (!position.valid)
    {
        return tr("No academic term");
    }

    const QString school =
        level == SchoolLevel::Elementary
            ? tr("Elementary")
            : tr("Middle School");
    const QLocale locale;
    const QString start =
        locale.toString(position.weekStart, QLocale::ShortFormat);
    const QString end =
        locale.toString(
            position.weekStart.addDays(6),
            QLocale::ShortFormat
            );

    return tr("%1 — %2, Week %3 (%4–%5)")
        .arg(
            school,
            termName(position.term)
            )
        .arg(position.week)
        .arg(start, end);
}

void AcademicCalendarProvider::persist()
{
    if (!m_dataService || !m_dataService->isOpen())
    {
        return;
    }

    const QString json =
        QString::fromUtf8(
            QJsonDocument(m_schedule.toJson())
                .toJson(QJsonDocument::Compact)
            );
    m_dataService->saveSetting(
        AcademicCalendarSettingsKey,
        json
        );
}

void AcademicCalendarProvider::persistFirstDayOfWeek()
{
    if (!m_dataService || !m_dataService->isOpen())
    {
        return;
    }

    m_dataService->saveSetting(
        CalendarSettingsKeys::FirstDayOfWeek,
        m_firstDayOfWeek
        );
}
