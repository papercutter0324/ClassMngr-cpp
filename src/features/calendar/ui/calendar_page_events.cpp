#include "calendar_page.h"

#include "academic_calendar_provider.h"
#include "calendar_event_cache.h"
#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
#include "domain/models/calendar_event.h"
#include "next/platform/application_services_calendar_event_port.h"
#include "next/platform/application_services_calendar_event_delete_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "next/platform/application_services_calendar_event_series_create_port.h"
#include "next/platform/application_services_calendar_event_series_edit_port.h"
#include "next/platform/application_services_calendar_event_series_delete_port.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/dialogs/user_prompt_service.h"
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
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>

namespace
{
constexpr int UntitledCardTopMargin = 4;

using CalendarEventSummary =
    ClassMngr::Next::Application::CalendarEventSummary;
using CalendarEventEditDraft =
    ClassMngr::Next::Application::CalendarEventEditDraft;
using CalendarEventSaveRequest =
    ClassMngr::Next::Application::CalendarEventSaveRequest;

QString projectionText(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

CalendarEventEditDraft calendarEventDraftFromProjection(
    const CalendarEventSummary& summary
    )
{
    CalendarEventEditDraft draft;
    draft.id = summary.id;
    draft.repeatSeriesId = summary.repeatSeriesId;
    draft.title = summary.title;
    draft.startDate = summary.startDate;
    draft.endDate = summary.endDate;
    draft.startTime = summary.startTime;
    draft.endTime = summary.endTime;
    draft.allDay = summary.allDay;
    draft.eventType = summary.eventType;
    draft.timeStatus = summary.timeStatus;
    return draft;
}

bool calendarServiceIsAvailable(
    ApplicationServices* services
    )
{
    if (!services)
    {
        return false;
    }

    const ClassMngr::Next::Platform::ApplicationServicesCalendarEventPort
        calendarEventPort(*services);
    return calendarEventPort.isAvailable();
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

CalendarEventSaveRequest saveRequestFromDraft(
    const CalendarEventEditDraft& draft
    )
{
    CalendarEventSaveRequest request;
    request.id = draft.id;
    request.title = draft.title;
    request.startDate = draft.startDate;
    request.endDate = draft.endDate;
    request.allDay = draft.allDay;
    request.eventType = draft.eventType;
    request.timeStatus = draft.timeStatus;

    if (!draft.allDay)
    {
        request.startTime = draft.startTime;
        request.endTime = draft.endTime;
    }

    return request;
}

QList<CalendarEventEditDraft> repeatedCalendarEventDrafts(
    const CalendarEventEditDraft& draft,
    CalendarEventRepeatFrequency frequency,
    const QDate& untilDate
    )
{
    const QDate startDate = QDate::fromString(
        projectionText(draft.startDate),
        Qt::ISODate
        );
    const QDate endDate = QDate::fromString(
        projectionText(draft.endDate),
        Qt::ISODate
        );

    QList<CalendarEventEditDraft> events;

    if (
        !startDate.isValid()
        || !endDate.isValid()
        || !untilDate.isValid()
        || untilDate < startDate
        )
    {
        events.append(draft);
        return events;
    }

    const int durationDays =
        startDate.daysTo(
            endDate
            );

    for (
        QDate occurrenceDate = startDate;
        occurrenceDate.isValid() && occurrenceDate <= untilDate;
        occurrenceDate = nextRepeatDate(occurrenceDate, frequency)
        )
    {
        CalendarEventEditDraft occurrence = draft;
        occurrence.id.reset();
        occurrence.startDate = occurrenceDate.toString(
            Qt::ISODate
            ).toStdString();
        occurrence.endDate = occurrenceDate.addDays(durationDays).toString(
            Qt::ISODate
            ).toStdString();

        events.append(occurrence);
    }

    return events;
}

bool isRepeatSeriesEvent(
    const CalendarEventEditDraft& draft
    )
{
    return draft.repeatSeriesId.has_value();
}

QString newRepeatSeriesId()
{
    return QUuid::createUuid().toString(
        QUuid::WithoutBraces
        );
}

}

void CalendarPage::handleCalendarDayActivated(
    int year,
    int month,
    int day
    )
{
    CalendarEventEditDraft draft;
    const QDate date(year, month, day);
    draft.startDate = date.toString(Qt::ISODate).toStdString();
    draft.endDate = draft.startDate;
    draft.startTime = "09:00";
    draft.endTime = "10:00";

    openCalendarDialog(
        draft,
        false
        );
}
void CalendarPage::handleCalendarEventActivated(
    int eventId
    )
{
    if (!m_services || eventId <= 0)
    {
        return;
    }

    ClassMngr::Next::Platform::ApplicationServicesCalendarEventPort port(
        *m_services
        );
    const auto projectedEvent = port.projectionById(eventId);
    if (!projectedEvent)
    {
        return;
    }

    const CalendarEventEditDraft draft =
        calendarEventDraftFromProjection(projectedEvent.value());
    openCalendarDialog(
        draft,
        true
        );
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

            if (currentUpcomingEventsScope()
                == UpcomingEventsScope::Next10Events)
            {
                ensureNextTenEvents();
            }
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
                ? m_services->settingsService()
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

    if (currentUpcomingEventsScope()
        == UpcomingEventsScope::Next10Events)
    {
        ensureNextTenEvents();
    }
}

void CalendarPage::refreshCalendarData()
{
    if (!m_calendarCache)
    {
        return;
    }

    const QString databasePath =
        m_services
            ? m_services->currentDatabasePath()
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

    m_loadedMonths.insert(visibleMonth);
    updateCalendarCacheRetention();

    m_calendarCache->requestRange(
        visibleMonth,
        visibleMonthEnd,
        CalendarEventCache::Priority::Foreground
        );

    refreshUpcomingEvents();
}

void CalendarPage::ensureUpcomingEventsForScope(
    UpcomingEventsScope scope
    )
{
    if (!m_calendarCache || scope == UpcomingEventsScope::CurrentMonth)
    {
        return;
    }

    const QString databasePath =
        m_services
            ? m_services->currentDatabasePath()
            : QString();

    if (databasePath.isEmpty())
    {
        return;
    }

    const QDate today = QDate::currentDate();

    if (scope == UpcomingEventsScope::Next30Days)
    {
        const CalendarEventCache::DateRange range{
            today,
            today.addDays(30)
        };

        if (!m_onDemandRetainedRanges.contains(range))
        {
            m_onDemandRetainedRanges.append(range);
        }

        updateCalendarCacheRetention();
        m_calendarCache->requestRange(
            range.startDate,
            range.endDate,
            CalendarEventCache::Priority::Foreground
            );
        return;
    }

    if (!m_nextTenSearchEnd.isValid())
    {
        m_nextTenSearchEnd =
            QDate(today.year(), today.month(), 1)
                .addMonths(1)
                .addDays(-1);
    }

    const CalendarEventCache::DateRange range{
        today,
        m_nextTenSearchEnd
    };

    if (!m_onDemandRetainedRanges.contains(range))
    {
        m_onDemandRetainedRanges.append(range);
    }

    updateCalendarCacheRetention();
    m_calendarCache->requestRange(
        range.startDate,
        range.endDate,
        CalendarEventCache::Priority::Foreground
        );
    ensureNextTenEvents();
}

void CalendarPage::updateCalendarCacheRetention()
{
    if (!m_calendarCache)
    {
        return;
    }

    QList<CalendarEventCache::DateRange> retainedRanges;
    QList<QDate> loadedMonths = m_loadedMonths.values();
    std::sort(loadedMonths.begin(), loadedMonths.end());

    for (const QDate& month : loadedMonths)
    {
        retainedRanges.append(
            {
                month,
                month.addMonths(1).addDays(-1)
            }
            );
    }

    for (const CalendarEventCache::DateRange& range :
         m_onDemandRetainedRanges)
    {
        if (!retainedRanges.contains(range))
        {
            retainedRanges.append(range);
        }
    }

    m_calendarCache->setRetainedRanges(retainedRanges);
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
    m_loadedMonths.clear();
    m_onDemandRetainedRanges.clear();
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
    const std::vector<
        ClassMngr::Next::Application::CalendarEventSummary
        > visibleEvents =
        filterUpcomingEvents(
            m_calendarCache->eventProjectionInRange(
                today,
                m_nextTenSearchEnd
                ).events(),
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

    const CalendarEventCache::DateRange range{
        QDate::currentDate(),
        m_nextTenSearchEnd
    };

    if (!m_onDemandRetainedRanges.contains(range))
    {
        m_onDemandRetainedRanges.append(range);
    }

    updateCalendarCacheRetention();
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
    const CalendarEventEditDraft& draft,
    bool existingEvent
    )
{
    if (!calendarServiceIsAvailable(m_services))
    {
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleDisplayPreferencesPort
        displayPreferencesPort(*m_services);
    const auto displayPreferences = displayPreferencesPort.load();
    CalendarEventDialog dialog(
        draft,
        existingEvent,
        displayPreferences
            ? displayPreferences.value().use24HourTime
            : false,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const bool repeatSeriesEvent =
        existingEvent
        && isRepeatSeriesEvent(draft);
    const bool thisAndFollowing =
        repeatSeriesEvent
        && dialog.seriesEditScope()
            == CalendarEventSeriesEditScope::ThisAndFollowingEvents;

    if (dialog.deleteRequested())
    {
        QString deleteError;
        if (thisAndFollowing)
        {
            const ClassMngr::Next::Application::
                CalendarEventSeriesDeleteRequest request{
                    *draft.repeatSeriesId,
                    draft.startDate
                };
            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventSeriesDeletePort deletePort(
                    *m_services
                    );
            const auto typedDeleted =
                deletePort.deleteRepeatSeriesFromDate(request);
            if (!typedDeleted)
            {
                deleteError = projectionText(typedDeleted.error().message);
            }
        }
        else
        {
            if (!draft.id)
            {
                return;
            }

            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventDeletePort deletePort(
                    *m_services
                    );
            const auto typedDeleted = deletePort.deleteEvent(*draft.id);
            if (!typedDeleted)
            {
                deleteError = projectionText(typedDeleted.error().message);
            }
        }

        if (!deleteError.isNull())
        {
            DialogServices::showWarning(
                this,
                tr("Delete Calendar Event"),
                deleteError
                );
            return;
        }
    }
    else
    {
        CalendarEventEditDraft savedDraft =
            dialog.eventData();

        if (thisAndFollowing)
        {
            const std::string repeatSeriesId = savedDraft.repeatSeriesId
                ? *savedDraft.repeatSeriesId
                : *draft.repeatSeriesId;
            ClassMngr::Next::Application::CalendarEventSeriesEditRequest
                request{
                    repeatSeriesId,
                    draft.startDate,
                    savedDraft.startDate,
                    savedDraft.endDate,
                    savedDraft.title,
                    savedDraft.allDay
                        ? std::nullopt
                        : savedDraft.startTime,
                    savedDraft.allDay
                        ? std::nullopt
                        : savedDraft.endTime,
                    savedDraft.allDay,
                    savedDraft.eventType,
                    savedDraft.timeStatus
                };

            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventSeriesEditPort editPort(
                    *m_services
                    );
            const auto typedSaved =
                editPort.editRepeatSeriesFromDate(request);
            if (!typedSaved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Save Calendar Event"),
                    projectionText(typedSaved.error().message)
                    );
                return;
            }
        }
        else if (!repeatSeriesEvent && !dialog.repeatEnabled())
        {
            CalendarEventSaveRequest request =
                saveRequestFromDraft(savedDraft);

            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventSavePort savePort(
                    *m_services
                    );
            const auto typedSaved = savePort.saveEvent(request);
            if (!typedSaved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Save Calendar Event"),
                    projectionText(typedSaved.error().message)
                    );
                return;
            }
        }
        else if (repeatSeriesEvent)
        {
            CalendarEventSaveRequest request =
                saveRequestFromDraft(savedDraft);

            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventSavePort savePort(
                    *m_services
                    );
            const auto typedSaved = savePort.saveEvent(request);
            if (!typedSaved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Save Calendar Event"),
                    projectionText(typedSaved.error().message)
                    );
                return;
            }
        }
        else
        {
            if (dialog.repeatEnabled())
            {
                savedDraft.repeatSeriesId =
                    newRepeatSeriesId().toUtf8().toStdString();
                const QList<CalendarEventEditDraft> eventsToSave =
                    repeatedCalendarEventDrafts(
                        savedDraft,
                        dialog.repeatFrequency(),
                        dialog.repeatUntilDate()
                        );

                ClassMngr::Next::Application::
                    CalendarEventSeriesCreateRequest request;
                request.repeatSeriesId = *savedDraft.repeatSeriesId;
                request.occurrences.reserve(eventsToSave.size());
                for (const CalendarEventEditDraft& occurrence : eventsToSave)
                {
                    request.occurrences.push_back(
                        saveRequestFromDraft(occurrence)
                        );
                }

                ClassMngr::Next::Platform::
                    ApplicationServicesCalendarEventSeriesCreatePort
                    createPort(*m_services);
                const auto typedSaved =
                    createPort.createRepeatSeries(request);
                if (!typedSaved)
                {
                    DialogServices::showWarning(
                        this,
                        tr("Save Calendar Event"),
                        projectionText(typedSaved.error().message)
                        );
                    return;
                }
            }
        }
    }

    invalidateCalendarData();
}
