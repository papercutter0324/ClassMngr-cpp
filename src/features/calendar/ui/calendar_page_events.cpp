#include "calendar_page.h"

#include "calendar_settings_dialog.h"
#include "academic_calendar_provider.h"
#include "calendar_event_cache.h"
#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

#include <QDate>
#include <QDialog>
#include <QFontInfo>
#include <QFrame>
#include <QLabel>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSizePolicy>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int UntitledCardTopMargin = 4;

DataService* openDataService(
    ApplicationServices* services
    )
{
    auto* dataService =
        services
            ? services->dataService()
            : nullptr;

    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
}

bool settingToBool(
    const QVariant& value,
    bool defaultValue
    )
{
    if (!value.isValid())
    {
        return defaultValue;
    }

    const QString text =
        value.toString().trimmed().toLower();

    if (text == QStringLiteral("true") || text == QStringLiteral("1"))
    {
        return true;
    }

    if (text == QStringLiteral("false") || text == QStringLiteral("0"))
    {
        return false;
    }

    return value.toBool();
}

QDate nextRepeatDate(
    const QDate& date,
    CalendarEventRepeatFrequency frequency
    )
{
    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
        return date.addDays(1);

    case CalendarEventRepeatFrequency::Monthly:
        return date.addMonths(1);

    case CalendarEventRepeatFrequency::Weekly:
        return date.addDays(7);
    }

    return date.addDays(7);
}

QList<CalendarEvent> repeatedCalendarEvents(
    const CalendarEvent& event,
    CalendarEventRepeatFrequency frequency,
    const QDate& untilDate
    )
{
    QList<CalendarEvent> events;

    if (
        !event.startDate.isValid()
        || !event.endDate.isValid()
        || !untilDate.isValid()
        || untilDate < event.startDate
        )
    {
        events.append(event);
        return events;
    }

    const int durationDays =
        event.startDate.daysTo(
            event.endDate
            );

    for (
        QDate occurrenceDate = event.startDate;
        occurrenceDate.isValid() && occurrenceDate <= untilDate;
        occurrenceDate = nextRepeatDate(occurrenceDate, frequency)
        )
    {
        CalendarEvent occurrence =
            event;
        occurrence.id =
            -1;
        occurrence.startDate =
            occurrenceDate;
        occurrence.endDate =
            occurrenceDate.addDays(durationDays);

        events.append(occurrence);
    }

    return events;
}

bool isRepeatSeriesEvent(
    const CalendarEvent& event
    )
{
    return !event.repeatSeriesId.trimmed().isEmpty();
}

QString newRepeatSeriesId()
{
    return QUuid::createUuid().toString(
        QUuid::WithoutBraces
        );
}

void saveRepeatSeriesFromDate(
    DataService* dataService,
    const CalendarEvent& originalEvent,
    const CalendarEvent& editedEvent
    )
{
    if (
        !dataService
        || !isRepeatSeriesEvent(originalEvent)
        || !originalEvent.startDate.isValid()
        || !editedEvent.startDate.isValid()
        || !editedEvent.endDate.isValid()
        )
    {
        return;
    }

    const QString repeatSeriesId =
        originalEvent.repeatSeriesId.trimmed();
    const int startDateOffset =
        originalEvent.startDate.daysTo(
            editedEvent.startDate
            );
    const int durationDays =
        editedEvent.startDate.daysTo(
            editedEvent.endDate
            );
    const QList<CalendarEvent> seriesEvents =
        dataService->loadCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId,
            originalEvent.startDate
            );

    for (const CalendarEvent& seriesEvent : seriesEvents)
    {
        CalendarEvent updatedEvent =
            seriesEvent;

        updatedEvent.title =
            editedEvent.title;
        updatedEvent.eventType =
            editedEvent.eventType;
        updatedEvent.timeStatus =
            editedEvent.timeStatus;
        updatedEvent.allDay =
            editedEvent.allDay;
        updatedEvent.startTime =
            editedEvent.startTime;
        updatedEvent.endTime =
            editedEvent.endTime;
        updatedEvent.repeatSeriesId =
            repeatSeriesId;
        updatedEvent.startDate =
            seriesEvent.startDate.addDays(
                startDateOffset
                );
        updatedEvent.endDate =
            updatedEvent.startDate.addDays(
                durationDays
                );

        dataService->saveCalendarEvent(
            updatedEvent
            );
    }
}
}

void CalendarPage::handleCalendarDayActivated(
    int year,
    int month,
    int day
    )
{
    CalendarEvent event;

    event.startDate =
        QDate(
            year,
            month,
            day
            );
    event.endDate =
        event.startDate;
    event.startTime =
        QTime(9, 0);
    event.endTime =
        QTime(10, 0);

    openCalendarDialog(
        event,
        false
        );
}
void CalendarPage::handleCalendarEventActivated(
    int eventId
    )
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService || eventId <= 0)
    {
        return;
    }

    const CalendarEvent event =
        dataService->getCalendarEvent(
            eventId
            );

    if (event.id <= 0)
    {
        return;
    }

    openCalendarDialog(
        event,
        true
        );
}
void CalendarPage::handleCalendarConfigureRequested(
    int year,
    int month
    )
{
    if (!m_academicCalendarProvider)
    {
        return;
    }

    const QDate firstOfMonth(year, month, 1);
    if (!firstOfMonth.isValid())
    {
        return;
    }

    const QDate firstMonday =
        firstOfMonth.addDays(
            (Qt::Monday - firstOfMonth.dayOfWeek() + 7) % 7
            );
    const int termYear =
        m_academicCalendarProvider->termYearForDate(firstMonday);

    CalendarSettingsDialog dialog(
        m_academicCalendarProvider,
        openDataService(m_services),
        termYear,
        this
        );

    connect(
        &dialog,
        &CalendarSettingsDialog::calendarEventsImported,
        this,
        [this]()
        {
            invalidateCalendarData();
        }
        );

    if (dialog.exec() == QDialog::Accepted)
    {
        updateCalendarCampusFilter();
    }
}
void CalendarPage::handleCalendarDisplayedMonthChanged(
    int year,
    int month
    )
{
    const QDate firstOfMonth(
        year,
        month,
        1
        );

    if (!firstOfMonth.isValid())
    {
        return;
    }

    m_calendarVisibleMonth =
        firstOfMonth;

    refreshCalendarData();
    refreshUpcomingEvents();
}
void CalendarPage::buildCalendarContent()
{
    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setAlignment(Qt::AlignTop);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UntitledCardTopMargin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    m_calendarCache =
        new CalendarEventCache(this);
    connect(
        m_calendarCache,
        &CalendarEventCache::cacheChanged,
        this,
        [this]()
        {
            refreshUpcomingEvents();
            ensureNextTenEvents();
        }
        );
    connect(
        m_calendarCache,
        &CalendarEventCache::loadingChanged,
        this,
        &CalendarPage::refreshUpcomingEvents
        );
    connect(
        m_calendarCache,
        &CalendarEventCache::nextEventMonthFound,
        this,
        &CalendarPage::handleNextEventMonthFound
        );

    m_calendarModel =
        new CalendarEventModel(
            m_calendarCache,
            this
            );

    m_academicCalendarProvider =
        new AcademicCalendarProvider(
            m_services
                ? m_services->dataService()
                : nullptr,
            this
            );

    const QDate today =
        QDate::currentDate();
    m_calendarVisibleMonth =
        QDate(
            qMax(today.year(), 2026),
            today.year() < 2026 ? 1 : today.month(),
            1
            );

    m_calendarView =
        new QQuickWidget(card);
    m_calendarView->installEventFilter(this);
    m_calendarView->setResizeMode(
        QQuickWidget::SizeRootObjectToView
        );
    m_calendarView->setMinimumHeight(840);
    m_calendarView->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );
    m_calendarView
        ->rootContext()
        ->setContextProperty(
            QStringLiteral("calendarEventProvider"),
            m_calendarModel
            );
    m_calendarView
        ->rootContext()
        ->setContextProperty(
            QStringLiteral("academicCalendarProvider"),
            m_academicCalendarProvider
            );
    m_calendarView->setSource(
        QUrl(
            QStringLiteral(
                "qrc:/qt/qml/ClassMngr/Calendar/EventCalendar.qml"
                )
            )
        );

    if (auto* root = m_calendarView->rootObject())
    {
        syncCalendarFontSize();
        syncCalendarEventTypeColors();

        connect(
            root,
            SIGNAL(dayActivated(int,int,int)),
            this,
            SLOT(handleCalendarDayActivated(int,int,int))
            );
        connect(
            root,
            SIGNAL(eventActivated(int)),
            this,
            SLOT(handleCalendarEventActivated(int))
            );
        connect(
            root,
            SIGNAL(configureRequested(int,int)),
            this,
            SLOT(handleCalendarConfigureRequested(int,int))
            );
        connect(
            root,
            SIGNAL(displayedMonthChanged(int,int)),
            this,
            SLOT(handleCalendarDisplayedMonthChanged(int,int))
            );
    }

    cardLayout->addWidget(
        m_calendarView
        );

    buildUpcomingEventsPanel(
        cardLayout,
        card
        );

    updateCalendarCampusFilter();
    refreshCalendarData();

    m_scrollContentLayout->addWidget(
        card
        );
}
void CalendarPage::updateCalendarCampusFilter()
{
    if (!m_calendarModel)
    {
        return;
    }

    const CalendarEventDisplayOptions options =
        calendarEventDisplayOptions();

    m_calendarModel->setCampusFilter(
        options.currentCampusCodes,
        options.allCampusCodes,
        options.showAllCampuses,
        options.hideStartOfTermEvents
        );
    refreshUpcomingEvents();
    ensureNextTenEvents();
}

void CalendarPage::refreshCalendarData()
{
    if (!m_calendarCache)
    {
        return;
    }

    auto* dataService =
        openDataService(m_services);
    const QString databasePath =
        dataService
            ? dataService->currentDatabasePath()
            : QString();

    m_calendarCache->setDatabasePath(databasePath);

    if (databasePath.isEmpty())
    {
        refreshUpcomingEvents();
        return;
    }

    const QDate today =
        QDate::currentDate();
    const QDate visibleMonth =
        m_calendarVisibleMonth.isValid()
            ? m_calendarVisibleMonth
            : QDate(today.year(), today.month(), 1);
    const QDate visibleMonthEnd =
        visibleMonth.addMonths(1).addDays(-1);

    m_calendarCache->requestRange(
        visibleMonth,
        visibleMonthEnd,
        CalendarEventCache::Priority::Foreground
        );
    m_calendarCache->requestRange(
        today,
        today.addDays(30),
        CalendarEventCache::Priority::Foreground
        );

    const QDate prefetchStart =
        visibleMonth.addMonths(1);
    const QDate prefetchEnd =
        visibleMonth.addMonths(5).addDays(-1);

    m_calendarCache->requestRange(
        prefetchStart,
        prefetchEnd,
        CalendarEventCache::Priority::Background
        );

    if (!m_nextTenSearchEnd.isValid())
    {
        const QDate currentMonth(
            today.year(),
            today.month(),
            1
            );

        m_nextTenSearchEnd =
            currentMonth.addMonths(5).addDays(-1);

        if (visibleMonth != currentMonth)
        {
            m_calendarCache->requestRange(
                currentMonth,
                m_nextTenSearchEnd,
                CalendarEventCache::Priority::Background
                );
        }
    }

    refreshUpcomingEvents();
    ensureNextTenEvents();
}

void CalendarPage::invalidateCalendarData()
{
    if (!m_calendarCache)
    {
        return;
    }

    m_nextTenSearchEnd = {};
    m_nextTenSearchComplete = false;
    m_nextTenLookupPending = false;
    m_calendarCache->invalidate();
    refreshCalendarData();
}

void CalendarPage::ensureNextTenEvents()
{
    if (
        !m_calendarCache
        || !m_nextTenSearchEnd.isValid()
        || m_nextTenSearchComplete
        || m_nextTenLookupPending
        )
    {
        return;
    }

    const CalendarEventDisplayOptions options =
        calendarEventDisplayOptions();

    if (options.activeTypes.isEmpty())
    {
        return;
    }

    const QDate today =
        QDate::currentDate();
    const QList<CalendarEvent> visibleEvents =
        filterUpcomingEvents(
            m_calendarCache->eventsInRange(
                today,
                m_nextTenSearchEnd
                ),
            options
            );

    if (
        visibleEvents.size() >= UpcomingEventsLimit
        || !m_calendarCache->isRangeLoaded(
            today,
            m_nextTenSearchEnd
            )
        )
    {
        return;
    }

    m_nextTenLookupPending = true;
    m_calendarCache->requestNextEventMonth(
        m_nextTenSearchEnd.addDays(1),
        CalendarEventCache::Priority::Background
        );
}

void CalendarPage::handleNextEventMonthFound(
    const QDate& firstEventDate
    )
{
    m_nextTenLookupPending = false;

    if (!firstEventDate.isValid())
    {
        m_nextTenSearchComplete = true;
        refreshUpcomingEvents();
        return;
    }

    const QDate firstOfMonth(
        firstEventDate.year(),
        firstEventDate.month(),
        1
        );

    m_nextTenSearchEnd =
        firstOfMonth.addMonths(1).addDays(-1);
    m_calendarCache->requestRange(
        firstOfMonth,
        m_nextTenSearchEnd,
        CalendarEventCache::Priority::Background
        );
}
void CalendarPage::syncCalendarFontSize()
{
    if (!m_calendarView)
    {
        return;
    }

    auto* root =
        m_calendarView->rootObject();

    if (!root)
    {
        return;
    }

    const int pixelSize =
        qMax(
            1,
            QFontInfo(
                m_calendarView->font()
                ).pixelSize()
            );

    root->setProperty(
        "baseFontPixelSize",
        pixelSize
        );
}
void CalendarPage::openCalendarDialog(
    const CalendarEvent& event,
    bool existingEvent
    )
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    CalendarEventDialog dialog(
        event,
        existingEvent,
        settingToBool(
            dataService->loadSetting(
                QStringLiteral("schedule_use_24h"),
                QStringLiteral("false")
                ),
            false
            ),
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const bool repeatSeriesEvent =
        existingEvent
        && isRepeatSeriesEvent(event);
    const bool thisAndFollowing =
        repeatSeriesEvent
        && dialog.seriesEditScope()
            == CalendarEventSeriesEditScope::ThisAndFollowingEvents;

    if (dialog.deleteRequested())
    {
        if (thisAndFollowing)
        {
            dataService->deleteCalendarEventsForRepeatSeriesFromDate(
                event.repeatSeriesId,
                event.startDate
                );
        }
        else
        {
            dataService->deleteCalendarEvent(
                event.id
                );
        }
    }
    else
    {
        CalendarEvent savedEvent =
            dialog.eventData();

        if (thisAndFollowing)
        {
            saveRepeatSeriesFromDate(
                dataService,
                event,
                savedEvent
                );
        }
        else
        {
            if (repeatSeriesEvent)
            {
                savedEvent.repeatSeriesId.clear();
            }
            else if (dialog.repeatEnabled())
            {
                savedEvent.repeatSeriesId =
                    newRepeatSeriesId();
            }

            const QList<CalendarEvent> eventsToSave =
                dialog.repeatEnabled()
                    ? repeatedCalendarEvents(
                        savedEvent,
                        dialog.repeatFrequency(),
                        dialog.repeatUntilDate()
                        )
                    : QList<CalendarEvent>{savedEvent};

            for (const CalendarEvent& eventToSave : eventsToSave)
            {
                dataService->saveCalendarEvent(
                    eventToSave
                    );
            }
        }
    }

    invalidateCalendarData();
}
