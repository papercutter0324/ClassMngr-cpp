#include "calendar_page.h"

#include "app/services/feature_services.h"
#include "academic_calendar_provider.h"
#include "calendar_event_cache.h"
#include "core/memory_usage_diagnostics.h"
#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
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

namespace
{
constexpr int UntitledCardTopMargin = 4;

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
    auto* calendarService =
        openCalendarService(m_services);

    if (!calendarService || eventId <= 0)
    {
        return;
    }

    const Result<CalendarEvent> event =
        calendarService->event(
            eventId
            );

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

    if (retainedRanges != m_lastRecordedRetentionRanges)
    {
        m_lastRecordedRetentionRanges = retainedRanges;
        const int eventCount = m_calendarCache->eventCount();
        const int dateBucketCount = m_calendarCache->dateBucketCount();
        emit calendarRetentionChanged(
            retainedRanges.size(),
            eventCount,
            dateBucketCount
            );
        MemoryUsageDiagnostics::recordEvent(
            QStringLiteral("calendar-retention-changed"),
            QStringLiteral("ranges=%1, events=%2, dateBuckets=%3")
                .arg(retainedRanges.size())
                .arg(eventCount)
                .arg(dateBucketCount)
            );
    }
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

    CalendarEventDialog dialog(
        event,
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
            deleted = calendarService->deleteRepeatSeriesFromDate(
                event.repeatSeriesId,
                event.startDate
                );
        }
        else
        {
            deleted = calendarService->deleteEvent(
                event.id
                );
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
        CalendarEvent savedEvent =
            dialog.eventData();

        if (thisAndFollowing)
        {
            const Status saved = calendarService->updateRepeatSeriesFromDate(
                event,
                savedEvent
                );
            if (!saved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Save Calendar Event"),
                    saved.error()
                    );
                return;
            }
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

            const Result<QList<int>> saved =
                dialog.repeatEnabled()
                    ? calendarService->createRepeatSeries(
                        savedEvent,
                        dialog.repeatFrequency(),
                        dialog.repeatUntilDate()
                        )
                    : calendarService->saveEvents({savedEvent});
            if (!saved)
            {
                DialogServices::showWarning(
                    this,
                    tr("Save Calendar Event"),
                    saved.error()
                    );
                return;
            }
        }
    }

    invalidateCalendarData();
}
