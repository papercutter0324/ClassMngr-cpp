#include "calendar_page.h"

#include "app/services/feature_services.h"
#include "academic_calendar_provider.h"
#include "calendar_event_cache.h"
#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
#include "next/platform/application_services_calendar_event_port.h"
#include "next/platform/application_services_calendar_event_delete_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
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
#include <QVariant>
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

std::optional<CalendarEvent> legacyEventFromProjection(
    const CalendarEventSummary& summary
    )
{
    bool validId = false;
    const int id = projectionText(summary.id.value()).toInt(&validId);
    const QDate startDate = QDate::fromString(
        projectionText(summary.startDate),
        Qt::ISODate
        );
    const QDate endDate = QDate::fromString(
        projectionText(summary.endDate),
        Qt::ISODate
        );
    if (!validId
        || id <= 0
        || !startDate.isValid()
        || !endDate.isValid()
        || endDate < startDate
        || summary.startTime.has_value() != summary.endTime.has_value()
        || (summary.allDay
            && (summary.startTime.has_value() || summary.endTime.has_value())))
    {
        return std::nullopt;
    }

    CalendarEvent event;
    event.id = id;
    event.title = projectionText(summary.title);
    event.eventType = projectionText(summary.eventType);
    event.timeStatus = projectionText(summary.timeStatus);
    event.repeatSeriesId = summary.repeatSeriesId
        ? projectionText(*summary.repeatSeriesId)
        : QString();
    event.allDay = summary.allDay;
    event.startDate = startDate;
    event.endDate = endDate;

    if (!summary.allDay && summary.startTime && summary.endTime)
    {
        event.startTime = QTime::fromString(
            projectionText(*summary.startTime),
            QStringLiteral("HH:mm")
            );
        event.endTime = QTime::fromString(
            projectionText(*summary.endTime),
            QStringLiteral("HH:mm")
            );
        if (!event.startTime.isValid() || !event.endTime.isValid())
        {
            return std::nullopt;
        }
    }

    return event;
}

CalendarEventEditDraft editDraftFromLegacyEvent(
    const CalendarEvent& event
    )
{
    CalendarEventEditDraft draft;

    if (event.id > 0)
    {
        draft.id =
            ClassMngr::Next::Domain::CalendarEventId::fromString(
                std::to_string(event.id)
                );
    }

    if (!event.repeatSeriesId.trimmed().isEmpty())
    {
        draft.repeatSeriesId =
            event.repeatSeriesId.toUtf8().toStdString();
    }

    draft.title = event.title.toUtf8().toStdString();
    draft.startDate = event.startDate.toString(Qt::ISODate).toStdString();
    draft.endDate = event.endDate.toString(Qt::ISODate).toStdString();
    draft.allDay = event.allDay;
    draft.eventType = event.eventType.toUtf8().toStdString();
    draft.timeStatus = event.timeStatus.toUtf8().toStdString();

    if (!event.allDay
        && event.startTime.isValid()
        && event.endTime.isValid())
    {
        draft.startTime = event.startTime.toString(
            QStringLiteral("HH:mm")
            ).toStdString();
        draft.endTime = event.endTime.toString(
            QStringLiteral("HH:mm")
            ).toStdString();
    }

    return draft;
}

CalendarService* openCalendarService(
    ApplicationServices* services
    )
{
    auto* calendarService =
        services
            ? services->calendarService()
            : nullptr;

    return calendarService && calendarService->isAvailable()
        ? calendarService
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

    const auto event = legacyEventFromProjection(projectedEvent.value());
    if (!event || event->id <= 0)
    {
        return;
    }

    openCalendarDialog(
        *event,
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
    const CalendarEvent& event,
    bool existingEvent
    )
{
    auto* calendarService =
        openCalendarService(m_services);
    auto* settingsService =
        m_services
            ? m_services->settingsService()
            : nullptr;

    if (!calendarService)
    {
        return;
    }

    const CalendarEventEditDraft editDraft =
        editDraftFromLegacyEvent(event);
    CalendarEventDialog dialog(
        editDraft,
        existingEvent,
        settingToBool(
            settingsService
                ? settingsService->loadOrDefault(
                QStringLiteral("schedule_use_24h"),
                QStringLiteral("false")
                )
                : QVariant(QStringLiteral("false")),
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
        Status deleted;
        if (thisAndFollowing)
        {
            const ClassMngr::Next::Application::
                CalendarEventSeriesDeleteRequest request{
                    event.repeatSeriesId.toUtf8().toStdString(),
                    event.startDate.toString(Qt::ISODate).toStdString()
                };
            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventSeriesDeletePort deletePort(
                    *m_services
                    );
            const auto typedDeleted =
                deletePort.deleteRepeatSeriesFromDate(request);
            if (!typedDeleted)
            {
                deleted = std::unexpected(
                    projectionText(typedDeleted.error().message)
                    );
            }
        }
        else
        {
            const auto typedEventId =
                ClassMngr::Next::Domain::CalendarEventId::fromString(
                    std::to_string(event.id)
                    );
            ClassMngr::Next::Platform::
                ApplicationServicesCalendarEventDeletePort deletePort(
                    *m_services
                    );
            const auto typedDeleted = deletePort.deleteEvent(*typedEventId);
            if (!typedDeleted)
            {
                deleted = std::unexpected(
                    projectionText(typedDeleted.error().message)
                    );
            }
        }

        if (!deleted)
        {
            DialogServices::showWarning(
                this,
                tr("Delete Calendar Event"),
                deleted.error()
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
                : event.repeatSeriesId.toUtf8().toStdString();
            ClassMngr::Next::Application::CalendarEventSeriesEditRequest
                request{
                    repeatSeriesId,
                    event.startDate.toString(Qt::ISODate).toStdString(),
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
