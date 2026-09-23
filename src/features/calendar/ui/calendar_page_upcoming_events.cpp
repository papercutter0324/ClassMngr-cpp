#include "calendar_page.h"

#include "calendar_event_cache.h"
#include "calendar_event_model.h"
#include "core/fontmanager.h"
#include "features/calendar/calendar_event_campus_filter.h"
#include "domain/models/calendar_event.h"
#include "next/platform/application_services_calendar_event_display_preferences_port.h"
#include "next/platform/application_services_calendar_event_type_color_preferences_port.h"
#include "next/platform/application_services_current_campus_preferences_port.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "next/platform/calendar_page_campus_directory_query_adapter.h"
#include "ui/shared/widgets/marquee_label.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <array>
#include <optional>
#include <string>

#include <QColorDialog>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabBar>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QVBoxLayout>

namespace
{
using CalendarEventProjection =
    ClassMngr::Next::Application::CalendarEventProjection;
using CalendarEventSummary =
    ClassMngr::Next::Application::CalendarEventSummary;

constexpr int UpcomingEventsNext30Days = 30;
constexpr int UpcomingEventColumnSpacing = 16;
constexpr int UpcomingEventDateColumnMinimumWidth = 72;
constexpr int UpcomingEventTimeColumnMinimumWidth = 104;
constexpr int UpcomingEventTypeColumnMinimumWidth = 82;
constexpr int UpcomingEventColumnTextPadding = 8;
constexpr int UpcomingEventTagMinimumHeight = 28;
constexpr int UpcomingEventRowMinimumHeight = 38;

QString projectionText(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QDate projectionDate(
    const std::string& value
    )
{
    return QDate::fromString(
        projectionText(value),
        Qt::ISODate
        );
}

QTime projectionTime(
    const std::optional<std::string>& value
    )
{
    return value
        ? QTime::fromString(
            projectionText(*value),
            QStringLiteral("HH:mm")
            )
        : QTime();
}

int legacyId(
    const CalendarEventSummary& event
    )
{
    bool validId = false;
    const int id = projectionText(event.id.value()).toInt(&validId);
    return validId && id > 0
        ? id
        : -1;
}

bool isStartOfTermCalendarEvent(
    const CalendarEventSummary& event
    )
{
    const QString title =
        projectionText(event.title).simplified().toLower();

    return normalizedCalendarEventType(projectionText(event.eventType))
            == QStringLiteral("Other")
        && (
            title == QStringLiteral("new semester")
            || title == QStringLiteral("start of term")
            || title == QStringLiteral("term start")
            || title == QStringLiteral("term starts")
            );
}

int upcomingEventTagVerticalPadding(
    const QFont& font
    )
{
    return qMax(
        6,
        QFontMetrics(font).height() / 3
        );
}

int upcomingEventTagHorizontalPadding(
    const QFont& font
    )
{
    return qMax(
        8,
        QFontMetrics(font).height() / 2
        );
}

int upcomingEventTagHeight(
    const QFont& font
    )
{
    const QFontMetrics metrics(font);

    return qMax(
        UpcomingEventTagMinimumHeight,
        metrics.lineSpacing()
            + (upcomingEventTagVerticalPadding(font) * 2)
            + 4
        );
}

int upcomingEventTagWidth(
    const QString& text,
    const QFont& font
    )
{
    const QFontMetrics metrics(font);

    return metrics.horizontalAdvance(text)
        + (upcomingEventTagHorizontalPadding(font) * 2)
        + 2;
}

int upcomingEventRowHeight(
    const QFont& font
    )
{
    return qMax(
        UpcomingEventRowMinimumHeight,
        upcomingEventTagHeight(font) + 10
        );
}

int scopeIndex(
    UpcomingEventsScope scope
    )
{
    return static_cast<int>(scope);
}

QColor defaultCalendarEventTypeColor(
    const QString& eventType
    )
{
    const QString normalized =
        normalizedCalendarEventType(
            eventType
            );

    QColor color =
        QColor(QStringLiteral("#66727a"));

    if (normalized == QStringLiteral("Vacation"))
    {
        color = QColor(QStringLiteral("#4b6f91"));
    }
    else if (normalized == QStringLiteral("Holiday"))
    {
        color = QColor(QStringLiteral("#7a5f9e"));
    }
    else if (normalized == QStringLiteral("Workshop"))
    {
        color = QColor(QStringLiteral("#5f7f52"));
    }
    else if (normalized == QStringLiteral("CM"))
    {
        color = QColor(QStringLiteral("#9a6b3f"));
    }
    else if (normalized == QStringLiteral("Meeting"))
    {
        color = QColor(QStringLiteral("#8a4f5d"));
    }

    return color;
}

QString readableTextColor(
    const QColor& color
    )
{
    const int brightness =
        (color.red() * 299
         + color.green() * 587
         + color.blue() * 114) / 1000;

    return brightness > 145
        ? QStringLiteral("#27313a")
        : QStringLiteral("#ffffff");
}

QString campusDisplayName(
    const ClassMngr::Next::Application::CalendarPageCampusMetadata& campus
    )
{
    const QString campusName = projectionText(campus.campusName);
    return campusName.trimmed().isEmpty()
        ? projectionText(campus.id).trimmed()
        : campusName.trimmed();
}

}

bool CalendarPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        event
        && watched == m_calendarView
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            )
        )
    {
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                syncCalendarFontSize();
                refreshUpcomingEvents();
            }
            );
    }

    if (
        event
        && watched
        && (
            event->type() == QEvent::Enter
            || event->type() == QEvent::Leave
            )
        )
    {
        auto* title =
            dynamic_cast<MarqueeLabel*>(watched);

        if (!title)
        {
            title =
                dynamic_cast<MarqueeLabel*>(
                    watched
                        ->property("calendarEventTitleLabel")
                        .value<QObject*>()
                    );
        }

        if (title)
        {
            title->setMarqueeActive(
                event->type() == QEvent::Enter
                );
        }
    }

    if (
        event
        && event->type() == QEvent::MouseButtonRelease
        && watched
        )
    {
        const int eventId =
            watched
                ->property("calendarEventId")
                .toInt();

        if (eventId > 0)
        {
            handleCalendarEventActivated(eventId);
            return true;
        }
    }

    return BasePage::eventFilter(
        watched,
        event
        );
}
void CalendarPage::buildUpcomingEventsPanel(
    QVBoxLayout* cardLayout,
    QWidget* parent
    )
{
    m_upcomingEventsHeading =
        createTopLevelHeading(
            tr("Upcoming Events"),
            parent
            );

    cardLayout->addSpacing(24);
    cardLayout->addWidget(m_upcomingEventsHeading);

    for (const QString& eventType : calendarEventTypes())
    {
        m_eventTypeFilterStates.insert(
            eventType,
            true
            );
    }

    m_upcomingEventsTabs =
        new NavigationTabWidget(
            NavigationTabKind::Section,
            QStringLiteral("calendarUpcomingTabBar"),
            parent
            );
    m_upcomingEventsTabs->setObjectName(
        QStringLiteral("calendarUpcomingTabs")
        );
    m_upcomingEventsTabs->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );

    m_upcomingEventsTabs->addTab(
        createUpcomingEventsPage(
            &m_upcomingEventLayouts[scopeIndex(UpcomingEventsScope::CurrentMonth)],
            m_upcomingEventsTabs
            ),
        tr("Current Month")
        );
    m_upcomingEventsTabs->addTab(
        createUpcomingEventsPage(
            &m_upcomingEventLayouts[scopeIndex(UpcomingEventsScope::Next30Days)],
            m_upcomingEventsTabs
            ),
        tr("Next 30 Days")
        );
    m_upcomingEventsTabs->addTab(
        createUpcomingEventsPage(
            &m_upcomingEventLayouts[scopeIndex(UpcomingEventsScope::Next10Events)],
            m_upcomingEventsTabs
            ),
        tr("Next 10 Events")
        );

    connect(
        m_upcomingEventsTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this](int index)
        {
            ensureUpcomingEventsForScope(
                static_cast<UpcomingEventsScope>(index)
                );
            refreshUpcomingEvents();
        }
        );

    cardLayout->addWidget(
        m_upcomingEventsTabs
        );

    syncEventTypeFilterButtons();
    refreshUpcomingEvents();
}
QWidget* CalendarPage::createUpcomingEventsPage(
    QVBoxLayout** pageLayout,
    QWidget* parent
    )
{
    auto* page =
        new QWidget(parent);

    auto* layout =
        new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);

    layout->addWidget(
        createEventTypeFilterRow(page)
        );

    auto* list =
        new QWidget(page);
    auto* listLayout =
        new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(6);
    listLayout->setAlignment(Qt::AlignTop);

    if (pageLayout)
    {
        *pageLayout =
            listLayout;
    }

    layout->addWidget(list);

    return page;
}
QWidget* CalendarPage::createEventTypeFilterRow(
    QWidget* parent
    )
{
    auto* container =
        new QWidget(parent);

    auto* layout =
        new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addStretch(1);

    const QFont tagFont =
        FontManager::getUiFont();
    container->setFixedHeight(
        upcomingEventTagHeight(tagFont)
        );

    for (const QString& eventType : calendarEventTypes())
    {
        // Event-type filters intentionally use their event color rather than
        // the application-wide button colors.
        auto* button =
            new QPushButton(
                eventType,
                container
                );
        button->setCheckable(true);
        button->setProperty(
            "eventType",
            eventType
            );
        button->setFont(tagFont);
        button->setFixedSize(
            upcomingEventTagWidth(eventType, tagFont),
            upcomingEventTagHeight(tagFont)
            );
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Fixed
            );
        button->setCursor(
            Qt::PointingHandCursor
            );

        connect(
            button,
            &QPushButton::toggled,
            this,
            [this, button](bool checked)
            {
                const QString eventType =
                    normalizedCalendarEventType(
                        button->property("eventType").toString()
                        );

                if (m_eventTypeFilterStates.value(eventType, true) == checked)
                {
                    button->setStyleSheet(
                        eventTypeFilterButtonStyle(
                            eventType,
                            checked,
                            button->font()
                            )
                        );
                    return;
                }

                m_eventTypeFilterStates.insert(
                    eventType,
                    checked
                    );
                syncEventTypeFilterButtons();
                refreshUpcomingEvents();
            }
            );

        m_eventTypeFilterButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch(1);

    return container;
}
void CalendarPage::refreshUpcomingEvents()
{
    if (!m_upcomingEventsTabs)
    {
        return;
    }

    syncEventTypeFilterButtons();

    const QList<UpcomingEventsScope> scopes{
        UpcomingEventsScope::CurrentMonth,
        UpcomingEventsScope::Next30Days,
        UpcomingEventsScope::Next10Events
    };

    std::array<
        CalendarEventProjection,
        UpcomingEventsScopeCount
        > eventsByScope;
    std::array<
        std::vector<CalendarEventSummary>,
        UpcomingEventsScopeCount
        > filteredEventsByScope;
    std::array<bool, UpcomingEventsScopeCount> loadingByScope{};
    const CalendarEventDisplayOptions options =
        calendarEventDisplayOptions();

    for (UpcomingEventsScope scope : scopes)
    {
        eventsByScope[scopeIndex(scope)] =
            upcomingEventsForScope(scope);
        filteredEventsByScope[scopeIndex(scope)] =
            filterUpcomingEvents(
                eventsByScope[scopeIndex(scope)].events(),
                options
                );

        if (scope == UpcomingEventsScope::Next10Events)
        {
            while (
                filteredEventsByScope[scopeIndex(scope)].size()
                > UpcomingEventsLimit
                )
            {
                filteredEventsByScope[scopeIndex(scope)].pop_back();
            }
        }

        loadingByScope[scopeIndex(scope)] =
            upcomingEventsLoading(
                scope,
                filteredEventsByScope[scopeIndex(scope)]
                );
    }

    int dateColumnWidth = UpcomingEventDateColumnMinimumWidth;
    int timeColumnWidth = UpcomingEventTimeColumnMinimumWidth;
    int eventTypeColumnWidth = UpcomingEventTypeColumnMinimumWidth;
    const QFont eventTextFont =
        FontManager::getUiFont();
    const QFontMetrics eventTextMetrics(
        eventTextFont
        );

    for (const QString& eventType : calendarEventTypes())
    {
        eventTypeColumnWidth =
            qMax(
                eventTypeColumnWidth,
                upcomingEventTagWidth(
                    normalizedCalendarEventType(eventType),
                    eventTextFont
                    )
                );
    }

    for (UpcomingEventsScope scope : scopes)
    {
        for (
            const CalendarEventSummary& event :
            filteredEventsByScope[scopeIndex(scope)]
            )
        {
            dateColumnWidth =
                qMax(
                    dateColumnWidth,
                    eventTextMetrics.horizontalAdvance(
                        upcomingEventDateText(event)
                        ) + UpcomingEventColumnTextPadding
                    );
            timeColumnWidth =
                qMax(
                    timeColumnWidth,
                    eventTextMetrics.horizontalAdvance(
                        upcomingEventTimeText(
                            event,
                            options.use24HourTime
                            )
                        ) + UpcomingEventColumnTextPadding
                    );
        }
    }

    for (UpcomingEventsScope scope : scopes)
    {
        renderUpcomingEvents(
            scope,
            filteredEventsByScope[scopeIndex(scope)],
            loadingByScope[scopeIndex(scope)],
            options.use24HourTime,
            dateColumnWidth,
            timeColumnWidth,
            eventTypeColumnWidth
            );
    }
}
void CalendarPage::renderUpcomingEvents(
    UpcomingEventsScope scope,
    const std::vector<CalendarEventSummary>& events,
    bool loading,
    bool use24HourTime,
    int dateColumnWidth,
    int timeColumnWidth,
    int eventTypeColumnWidth
    )
{
    QVBoxLayout* layout =
        m_upcomingEventLayouts[scopeIndex(scope)];

    if (!layout)
    {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            delete widget;
        }

        delete item;
    }

    if (events.empty())
    {
        auto* empty =
            new QLabel(
                loading
                    ? tr("Loading events…")
                    : tr("No upcoming events."),
                layout->parentWidget()
                );
        empty->setObjectName(
            QStringLiteral("sectionSubtitle")
            );
        empty->setAlignment(
            Qt::AlignCenter
            );
        layout->addWidget(empty);
        return;
    }

    for (const CalendarEventSummary& event : events)
    {
        layout->addWidget(
            createUpcomingEventRow(
                event,
                dateColumnWidth,
                timeColumnWidth,
                eventTypeColumnWidth,
                use24HourTime,
                layout->parentWidget()
                )
            );
    }

    if (loading)
    {
        auto* loadingLabel =
            new QLabel(
                tr("Loading more events…"),
                layout->parentWidget()
                );
        loadingLabel->setObjectName(
            QStringLiteral("sectionSubtitle")
            );
        loadingLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(loadingLabel);
    }
}
CalendarEventProjection CalendarPage::upcomingEventsForScope(
    UpcomingEventsScope scope
    ) const
{
    if (!m_calendarCache)
    {
        return {};
    }

    const QDate today =
        QDate::currentDate();

    switch (scope)
    {
    case UpcomingEventsScope::CurrentMonth:
    {
        const QDate firstOfMonth =
            m_calendarVisibleMonth.isValid()
                ? m_calendarVisibleMonth
                : QDate(today.year(), today.month(), 1);
        return m_calendarCache->eventProjectionInRange(
            firstOfMonth,
            firstOfMonth.addMonths(1).addDays(-1)
            );
    }

    case UpcomingEventsScope::Next30Days:
        return m_calendarCache->eventProjectionInRange(
            today,
            today.addDays(UpcomingEventsNext30Days)
            );

    case UpcomingEventsScope::Next10Events:
        return m_nextTenSearchEnd.isValid()
            ? m_calendarCache->eventProjectionInRange(
            today,
            m_nextTenSearchEnd
            )
            : CalendarEventProjection();
    }

    return {};
}

UpcomingEventsScope CalendarPage::currentUpcomingEventsScope() const
{
    if (!m_upcomingEventsTabs)
    {
        return UpcomingEventsScope::CurrentMonth;
    }

    switch (m_upcomingEventsTabs->currentIndex())
    {
    case static_cast<int>(UpcomingEventsScope::Next30Days):
        return UpcomingEventsScope::Next30Days;

    case static_cast<int>(UpcomingEventsScope::Next10Events):
        return UpcomingEventsScope::Next10Events;

    case static_cast<int>(UpcomingEventsScope::CurrentMonth):
    default:
        return UpcomingEventsScope::CurrentMonth;
    }
}

bool CalendarPage::upcomingEventsLoading(
    UpcomingEventsScope scope,
    const std::vector<CalendarEventSummary>& events
    ) const
{
    if (!m_calendarCache)
    {
        return false;
    }

    const QDate today =
        QDate::currentDate();

    switch (scope)
    {
    case UpcomingEventsScope::CurrentMonth:
    {
        const QDate firstOfMonth =
            m_calendarVisibleMonth.isValid()
                ? m_calendarVisibleMonth
                : QDate(today.year(), today.month(), 1);
        return !m_calendarCache->isRangeLoaded(
            firstOfMonth,
            firstOfMonth.addMonths(1).addDays(-1)
            );
    }

    case UpcomingEventsScope::Next30Days:
        return !m_calendarCache->isRangeLoaded(
            today,
            today.addDays(UpcomingEventsNext30Days)
            );

    case UpcomingEventsScope::Next10Events:
        return !activeCalendarEventTypes().isEmpty()
            && events.size() < UpcomingEventsLimit
            && !m_nextTenSearchComplete;
    }

    return false;
}

CalendarPage::CalendarEventDisplayOptions
CalendarPage::calendarEventDisplayOptions() const
{
    CalendarEventDisplayOptions options;
    options.activeTypes =
        activeCalendarEventTypes();

    ClassMngr::Next::Platform::
        ApplicationServicesCurrentCampusPreferencesPort
        currentCampusPreferencesPort(m_services);

    if (currentCampusPreferencesPort.isAvailable())
    {
        ClassMngr::Next::Platform::
            ApplicationServicesCalendarEventDisplayPreferencesPort
            eventDisplayPreferencesPort(*m_services);
        const auto eventDisplayPreferences = eventDisplayPreferencesPort.load();
        if (eventDisplayPreferences)
        {
            options.showAllCampuses =
                eventDisplayPreferences.value().showEventsAtAllCampuses;
            options.hideStartOfTermEvents =
                eventDisplayPreferences.value().hideStartOfTermEvents;
        }
        ClassMngr::Next::Platform::
            ApplicationServicesScheduleDisplayPreferencesPort
            displayPreferencesPort(*m_services);
        const auto displayPreferences = displayPreferencesPort.load();
        options.use24HourTime =
            displayPreferences
                ? displayPreferences.value().use24HourTime
                : false;

        const QString currentName =
            projectionText(currentCampusPreferencesPort.read());
        ClassMngr::Next::Platform::
            CalendarPageCampusDirectoryQueryAdapter campusDirectoryQueryAdapter;
        const auto campuses = campusDirectoryQueryAdapter.loadCampuses();

        options.currentCampusCodes.append(currentName);

        for (const auto& campus : campuses)
        {
            if (campus.campusCode)
            {
                options.allCampusCodes.append(
                    projectionText(*campus.campusCode)
                    );
            }
            options.allCampusCodes.append(projectionText(campus.id));

            if (
                projectionText(campus.id)
                    .compare(currentName, Qt::CaseInsensitive) == 0
                || campusDisplayName(campus).compare(currentName, Qt::CaseInsensitive) == 0
                || projectionText(campus.campusName)
                    .compare(currentName, Qt::CaseInsensitive) == 0
                )
            {
                if (campus.campusCode)
                {
                    options.currentCampusCodes.append(
                        projectionText(*campus.campusCode)
                        );
                }
                options.currentCampusCodes.append(projectionText(campus.id));
                options.currentCampusCodes.append(campusDisplayName(campus));
            }
        }
    }

    options.currentCampusCodes.removeAll(QString());
    options.currentCampusCodes.removeDuplicates();
    options.allCampusCodes.removeAll(QString());
    options.allCampusCodes.removeDuplicates();
    return options;
}

std::vector<CalendarEventSummary> CalendarPage::filterUpcomingEvents(
    const std::vector<CalendarEventSummary>& events,
    const CalendarEventDisplayOptions& options
    ) const
{
    std::vector<CalendarEventSummary> filteredEvents;
    filteredEvents.reserve(events.size());

    for (const CalendarEventSummary& event : events)
    {
        if (
            options.activeTypes.contains(
                normalizedCalendarEventType(
                    projectionText(event.eventType)
                    )
                )
            && calendarEventVisible(event, options)
            )
        {
            filteredEvents.push_back(event);
        }
    }

    return filteredEvents;
}
QStringList CalendarPage::activeCalendarEventTypes() const
{
    QStringList activeTypes;

    for (const QString& eventType : calendarEventTypes())
    {
        if (m_eventTypeFilterStates.value(eventType, true))
        {
            activeTypes.append(
                eventType
                );
        }
    }

    return activeTypes;
}
QColor CalendarPage::calendarEventTypeColor(
    const QString& eventType
    ) const
{
    const QString normalized =
        normalizedCalendarEventType(eventType);

    ClassMngr::Next::Platform::
        ApplicationServicesCalendarEventTypeColorPreferencesPort
        colorPreferencesPort(m_services);
    const std::string storedColorText =
        colorPreferencesPort.read(
            normalized.toUtf8().toStdString()
            );
    const QColor storedColor(
        QString::fromUtf8(
            storedColorText.data(),
            static_cast<qsizetype>(storedColorText.size())
            )
        );

    if (storedColor.isValid())
    {
        return storedColor;
    }

    return defaultCalendarEventTypeColor(normalized);
}
void CalendarPage::saveCalendarEventTypeColor(
    const QString& eventType,
    const QColor& color
    )
{
    if (!color.isValid())
    {
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesCalendarEventTypeColorPreferencesPort
        colorPreferencesPort(m_services);
    colorPreferencesPort.write(
        normalizedCalendarEventType(eventType).toUtf8().toStdString(),
        color.name(QColor::HexRgb).toUtf8().toStdString()
        );
}
void CalendarPage::chooseCalendarEventTypeColor(
    const QString& eventType
    )
{
    const QString normalized =
        normalizedCalendarEventType(eventType);

    const QColor selected =
        QColorDialog::getColor(
            calendarEventTypeColor(normalized),
            this,
            tr("Choose %1 Color").arg(normalized)
            );

    if (!selected.isValid())
    {
        return;
    }

    saveCalendarEventTypeColor(
        normalized,
        selected
        );
    syncEventTypeFilterButtons();
    syncCalendarEventTypeColors();
    refreshUpcomingEvents();
}
QString CalendarPage::eventTypeBadgeStyle(
    const QString& eventType,
    const QFont& font
    ) const
{
    const QColor color =
        calendarEventTypeColor(eventType);
    const int horizontalPadding =
        upcomingEventTagHorizontalPadding(font);
    const int verticalPadding =
        upcomingEventTagVerticalPadding(font);

    return QStringLiteral(
        "QPushButton {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid transparent;"
        " border-radius: 4px;"
        " padding: %3px %4px;"
        "}"
        "QPushButton:hover {"
        " border: 1px solid rgba(255, 255, 255, 160);"
        "}"
        ).arg(
            color.name(QColor::HexRgb),
            readableTextColor(color),
            QString::number(verticalPadding),
            QString::number(horizontalPadding)
            );
}
QString CalendarPage::eventTypeFilterButtonStyle(
    const QString& eventType,
    bool checked,
    const QFont& font
    ) const
{
    const QColor color =
        calendarEventTypeColor(eventType);
    const QString textColor =
        checked
            ? readableTextColor(color)
            : QStringLiteral("#66727a");
    const int horizontalPadding =
        upcomingEventTagHorizontalPadding(font);
    const int verticalPadding =
        upcomingEventTagVerticalPadding(font);

    return QStringLiteral(
        "QPushButton {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid %3;"
        " border-radius: 6px;"
        " padding: %4px %5px;"
        "}"
        "QPushButton:hover {"
        " border-color: %6;"
        "}"
        ).arg(
            checked ? color.name(QColor::HexRgb) : QStringLiteral("transparent"),
            textColor,
            checked ? color.name(QColor::HexRgb) : QStringLiteral("#a8b2b8"),
            QString::number(verticalPadding),
            QString::number(horizontalPadding),
            color.name(QColor::HexRgb)
            );
}
void CalendarPage::syncEventTypeFilterButtons()
{
    const QFont navigationFont =
        FontManager::getUiFont();

    for (QPushButton* button : m_eventTypeFilterButtons)
    {
        if (!button)
        {
            continue;
        }

        const QString eventType =
            normalizedCalendarEventType(
                button->property("eventType").toString()
                );
        const bool checked =
            m_eventTypeFilterStates.value(
                eventType,
                true
                );
        const QSignalBlocker blocker(button);
        button->setFont(navigationFont);
        button->setFixedSize(
            upcomingEventTagWidth(eventType, navigationFont),
            upcomingEventTagHeight(navigationFont)
            );
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Fixed
            );
        button->setChecked(checked);
        button->setStyleSheet(
            eventTypeFilterButtonStyle(
                eventType,
                checked,
                navigationFont
                )
            );
    }

    if (m_upcomingEventsTabs && m_upcomingEventsTabs->tabStrip())
    {
        m_upcomingEventsTabs->setFont(
            navigationFont
            );
        m_upcomingEventsTabs->tabStrip()->setFont(
            navigationFont
            );
        m_upcomingEventsTabs->updateGeometry();
        m_upcomingEventsTabs->tabStrip()->updateGeometry();
    }
}
void CalendarPage::syncCalendarEventTypeColors()
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

    QVariantMap colors;
    QVariantMap textColors;

    for (const QString& eventType : calendarEventTypes())
    {
        const QString normalized =
            normalizedCalendarEventType(eventType);
        const QColor color =
            calendarEventTypeColor(normalized);

        colors.insert(
            normalized,
            color.name(QColor::HexRgb)
            );
        textColors.insert(
            normalized,
            readableTextColor(color)
            );
    }

    root->setProperty(
        "eventTypeColors",
        colors
        );
    root->setProperty(
        "eventTypeTextColors",
        textColors
        );
}
QString CalendarPage::upcomingEventDateText(
    const CalendarEventSummary& event
    ) const
{
    const QDate startDate = projectionDate(event.startDate);
    const QDate endDate = projectionDate(event.endDate);

    if (!startDate.isValid())
    {
        return QStringLiteral("-");
    }

    if (
        !endDate.isValid()
        || endDate == startDate
        )
    {
        return startDate.toString(
            QStringLiteral("MMM d")
            );
    }

    const QString startFormat =
        startDate.year() == endDate.year()
            ? QStringLiteral("MMM d")
            : QStringLiteral("MMM d yyyy");

    return QStringLiteral("%1 - %2")
        .arg(
            startDate.toString(startFormat),
            endDate.toString(QStringLiteral("MMM d yyyy"))
            );
}
QString CalendarPage::upcomingEventTimeText(
    const CalendarEventSummary& event,
    bool use24HourTime
    ) const
{
    if (event.allDay)
    {
        return tr("All day");
    }

    const QString timeStatus =
        normalizedCalendarEventTimeStatus(
            projectionText(event.timeStatus)
            );

    if (timeStatus == QStringLiteral("Unknown"))
    {
        return tr("Unknown Time");
    }

    if (timeStatus == QStringLiteral("Unconfirmed"))
    {
        return tr("Unconfirmed Time");
    }

    if (!event.startTime.has_value())
    {
        return QString();
    }

    const QTime startTime = projectionTime(event.startTime);
    if (!startTime.isValid())
    {
        return QString();
    }

    const QString format =
        use24HourTime
            ? QStringLiteral("HH:mm")
            : QStringLiteral("h:mm AP");

    if (!event.endTime.has_value())
    {
        return startTime.toString(format);
    }

    const QTime endTime = projectionTime(event.endTime);
    if (!endTime.isValid())
    {
        return QString();
    }

    return QStringLiteral("%1 - %2")
        .arg(
            startTime.toString(format),
            endTime.toString(format)
            );
}
bool CalendarPage::calendarEventVisible(
    const CalendarEventSummary& event,
    const CalendarEventDisplayOptions& options
    ) const
{
    if (
        options.hideStartOfTermEvents
        && isStartOfTermCalendarEvent(event)
        )
    {
        return false;
    }

    return CalendarEventCampusFilter::eventMatchesCampus(
        event,
        options.currentCampusCodes,
        options.allCampusCodes,
        options.showAllCampuses
        );
}
QWidget* CalendarPage::createUpcomingEventRow(
    const CalendarEventSummary& event,
    int dateColumnWidth,
    int timeColumnWidth,
    int eventTypeColumnWidth,
    bool use24HourTime,
    QWidget* parent
    )
{
    const QFont eventFont =
        FontManager::getUiFont();
    const int tagHeight =
        upcomingEventTagHeight(eventFont);
    const int rowHeight =
        upcomingEventRowHeight(eventFont);

    auto* row =
        new QFrame(parent);
    row->setObjectName(
        QStringLiteral("upcomingCalendarEventRow")
        );
    row->setCursor(
        Qt::PointingHandCursor
        );
    row->setMouseTracking(true);
    row->setFixedHeight(
        rowHeight
        );
    row->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );
    row->setStyleSheet(
        QStringLiteral(
            "QFrame#upcomingCalendarEventRow {"
            " background: transparent;"
            " border: 1px solid transparent;"
            " border-radius: 6px;"
            "}"
            "QFrame#upcomingCalendarEventRow:hover {"
            " background-color: rgba(83, 111, 138, 35);"
            " border-color: rgba(83, 111, 138, 120);"
            "}"
            )
        );
    row->setProperty(
        "calendarEventId",
        legacyId(event)
        );
    row->installEventFilter(this);

    auto* layout =
        new QHBoxLayout(row);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(UpcomingEventColumnSpacing);

    auto* date =
        new QLabel(
            upcomingEventDateText(event),
            row
            );
    date->setFont(eventFont);
    date->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Preferred
        );
    date->setFixedWidth(dateColumnWidth);
    date->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    date->setCursor(
        Qt::PointingHandCursor
        );
    date->setProperty(
        "calendarEventId",
        legacyId(event)
        );

    auto* time =
        new QLabel(
            upcomingEventTimeText(
                event,
                use24HourTime
                ),
            row
            );
    time->setFont(eventFont);
    time->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Preferred
        );
    time->setFixedWidth(timeColumnWidth);
    time->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    time->setCursor(
        Qt::PointingHandCursor
        );
    time->setProperty(
        "calendarEventId",
        legacyId(event)
        );

    auto* title =
        new MarqueeLabel(row);
    title->setText(projectionText(event.title));
    title->setFont(eventFont);
    title->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );
    title->setMinimumWidth(0);
    title->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    title->setTextInteractionFlags(
        Qt::NoTextInteraction
        );
    title->setCursor(
        Qt::PointingHandCursor
        );
    title->setProperty(
        "calendarEventId",
        legacyId(event)
        );

    auto* type =
        new QPushButton(
            normalizedCalendarEventType(
                projectionText(event.eventType)
                ),
            row
            );
    type->setFont(eventFont);
    type->setFixedSize(
        eventTypeColumnWidth,
        tagHeight
        );
    type->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
    type->setCursor(
        Qt::PointingHandCursor
        );
    type->setToolTip(
        tr("Choose %1 color").arg(
            normalizedCalendarEventType(
                projectionText(event.eventType)
                )
            )
        );
    type->setStyleSheet(
        eventTypeBadgeStyle(
            projectionText(event.eventType),
            eventFont
            )
        );

    row->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    date->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    time->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    title->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    type->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );

    date->installEventFilter(this);
    time->installEventFilter(this);
    title->installEventFilter(this);
    type->installEventFilter(this);

    layout->addWidget(date);
    layout->addWidget(time);
    layout->addWidget(title, 1);
    layout->addWidget(type);

    connect(
        type,
        &QPushButton::clicked,
        this,
        [this, event]()
        {
            chooseCalendarEventTypeColor(
                projectionText(event.eventType)
                );
        }
        );

    return row;
}
