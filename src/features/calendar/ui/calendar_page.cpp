#include "calendar_page.h"

#include "academic_calendar_provider.h"
#include "calendar_event_model.h"
#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QFrame>
#include <QColorDialog>
#include <QLabel>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

CalendarPage::CalendarPage(
    ApplicationServices* services,
    QWidget* parent,
    EventTypeColorPicker eventTypeColorPicker
    )
    : BasePage(parent)
    , m_services(services)
    , m_eventTypeColorPicker(
          eventTypeColorPicker
              ? std::move(eventTypeColorPicker)
              : [this](const QColor& initial, const QString& title)
                {
                    return QColorDialog::getColor(initial, this, title);
                }
          )
{
    setProperty("role", UiRoles::MyInfo);
    buildUi();
}

void CalendarPage::buildUi()
{
    contentLayout()->setContentsMargins(0, 0, 0, 0);

    m_scrollArea =
        new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollContent =
        new QWidget(m_scrollArea);
    m_scrollContentLayout =
        new QVBoxLayout(m_scrollContent);
    m_scrollContentLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    m_scrollContentLayout->setSpacing(
        UiConstants::Pages::Spacing
        );
    m_scrollContentLayout->setAlignment(Qt::AlignTop);

    m_pageHeader =
        new QWidget(m_scrollContent);
    auto* headerLayout =
        new QVBoxLayout(m_pageHeader);
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel =
        new QLabel(
            tr("Calendar"),
            m_scrollContent
            );
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("View and manage your monthly events and upcoming dates."),
            m_scrollContent
            );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    m_scrollContentLayout->addWidget(m_pageHeader);
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_scrollContentLayout->addStretch();
    buildCalendarContent();

    m_scrollArea->setWidget(m_scrollContent);
    contentLayout()->addWidget(m_scrollArea);
}

void CalendarPage::refresh()
{
    BasePage::refresh();

    buildCalendarContent();
    updateCalendarCampusFilter();

    refreshCalendarData();

    if (m_academicCalendarProvider)
    {
        m_academicCalendarProvider->reload();
    }

    refreshUpcomingEvents();
}

void CalendarPage::releaseFeatureResources()
{
    if (!m_calendarContent)
    {
        return;
    }

    m_upcomingEventsScope = currentUpcomingEventsScope();
    QWidget* const content = m_calendarContent;
    m_calendarContent = nullptr;
    m_calendarView = nullptr;
    m_upcomingEventsTabs = nullptr;
    m_upcomingEventsHeading = nullptr;
    m_upcomingEventLayouts.fill(nullptr);
    m_eventTypeFilterButtons.clear();
    // Destroy QML bindings before invalidation notifies the retained model.
    delete content;

    m_nextTenSearchEnd = {};
    m_nextTenSearchComplete = false;
    m_nextTenLookupPending = false;
    m_loadedMonths.clear();
    m_onDemandRetainedRanges.clear();
    m_calendarCache->setRetainedRanges({});
    m_calendarCache->invalidate();
    markStale();
}

void CalendarPage::clearDatabaseState()
{
    m_eventTypeFilterStates.clear();

    updateCalendarCampusFilter();

    invalidateCalendarData();

    if (m_academicCalendarProvider)
    {
        m_academicCalendarProvider->reload();
    }

    syncCalendarEventTypeColors();
    refreshUpcomingEvents();
}

void CalendarPage::retranslateUi()
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(
            tr("Calendar")
            );
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            tr("View and manage your monthly events and upcoming dates.")
            );
    }

    if (m_upcomingEventsHeading)
    {
        m_upcomingEventsHeading->setText(
            tr("Upcoming Events")
            );
    }

    if (m_upcomingEventsTabs && m_upcomingEventsTabs->count() >= 3)
    {
        m_upcomingEventsTabs->setTabText(0, tr("Current Month"));
        m_upcomingEventsTabs->setTabText(1, tr("Next 30 Days"));
        m_upcomingEventsTabs->setTabText(2, tr("Next 10 Events"));
    }

    if (m_calendarView && m_calendarView->engine())
    {
        m_calendarView->engine()->retranslate();
    }

    if (m_academicCalendarProvider)
    {
        m_academicCalendarProvider->reload();
    }

    refreshUpcomingEvents();
}

void CalendarPage::setPageHeaderVisible(bool visible)
{
    if (m_pageHeader)
    {
        m_pageHeader->setVisible(visible);
    }
}

void CalendarPage::scrollToTop()
{
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (auto* scrollBar =
                    m_scrollArea
                        ? m_scrollArea->verticalScrollBar()
                        : nullptr)
            {
                scrollBar->setValue(
                    scrollBar->minimum()
                    );
            }
        }
        );
}

AcademicCalendarProvider* CalendarPage::academicCalendarProvider() const
{
    return m_academicCalendarProvider;
}

CalendarPageRuntimeMetrics CalendarPage::runtimeMetrics() const
{
    CalendarPageRuntimeMetrics metrics;

    if (m_calendarCache)
    {
        metrics.cacheEventCount = m_calendarCache->eventCount();
        metrics.cacheDateBucketCount = m_calendarCache->dateBucketCount();
        metrics.cacheLoadedRangeCount = m_calendarCache->loadedRangeCount();
        metrics.cacheRetainedRangeCount = m_calendarCache->retainedRangeCount();
        metrics.cacheLoading = m_calendarCache->isLoading();
    }

    metrics.nextTenSearchEnd = m_nextTenSearchEnd;
    metrics.nextTenSearchComplete = m_nextTenSearchComplete;
    metrics.nextTenLookupPending = m_nextTenLookupPending;
    metrics.loadedMonthCount = m_loadedMonths.size();
    metrics.onDemandRetainedRangeCount = m_onDemandRetainedRanges.size();
    metrics.modelRevision = m_calendarModel
        ? m_calendarModel->revision()
        : 0;
    metrics.pageWidgetCount = findChildren<QWidget*>().size();
    metrics.calendarViewObjectCount = m_calendarView
        ? m_calendarView->findChildren<QObject*>().size()
        : 0;

    return metrics;
}

void CalendarPage::calendarPreferencesChanged(bool eventsChanged)
{
    updateCalendarCampusFilter();

    if (eventsChanged)
    {
        invalidateCalendarData();
    }

    markStale();

    if (isVisible())
    {
        activate();
    }
}

QLabel* CalendarPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(
            text,
            parent
            );

    label->setObjectName("sectionTitle");
    label->setAlignment(Qt::AlignCenter);
    label->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SectionTitleFontSize,
            QFont::DemiBold
            )
        );

    return label;
}
