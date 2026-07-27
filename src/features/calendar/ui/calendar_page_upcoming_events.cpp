#include "calendar_page.h"

#include "calendar_event_model.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "data/data_service.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/calendar/calendar_event_campus_filter.h"
#include "features/calendar/calendar_settings_keys.h"
#include "ui/shared/widgets/marquee_label.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <array>

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
#include <QTabWidget>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QVBoxLayout>

namespace
{
constexpr int UpcomingEventsNext30Days = 30;
constexpr int UpcomingEventsLimit = 10;
constexpr int UpcomingEventColumnSpacing = 16;
constexpr int UpcomingEventDateColumnMinimumWidth = 72;
constexpr int UpcomingEventTimeColumnMinimumWidth = 104;
constexpr int UpcomingEventTypeColumnMinimumWidth = 82;
constexpr int UpcomingEventColumnTextPadding = 8;
constexpr int UpcomingEventTagMinimumHeight = 28;
constexpr int UpcomingEventRowMinimumHeight = 38;

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

QString calendarEventTypeColorSettingKey(
    const QString& eventType
    )
{
    return QStringLiteral("calendar/eventTypeColor/%1").arg(
        normalizedCalendarEventType(eventType)
        );
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

CampusJsonRepository campusRepository()
{
    return CampusJsonRepository(
        ResourcePaths::Campuses::directory()
        );
}

QString campusDisplayName(
    const CampusInfo& campus
    )
{
    return campus.campusName.trimmed().isEmpty()
        ? campus.id.trimmed()
        : campus.campusName.trimmed();
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
        new UniformWidthTabWidget(
            UniformWidthTabKind::Section,
            QStringLiteral("calendarUpcomingTabBar"),
            parent
            );
    m_upcomingEventsTabs->setObjectName(
        QStringLiteral("calendarUpcomingTabs")
        );
    m_upcomingEventsTabs->setTabAppearance(
        UniformWidthTabAppearance::NavigationPill
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
        &QTabWidget::currentChanged,
        this,
        [this](int)
        {
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

    std::array<QList<CalendarEvent>, UpcomingEventsScopeCount> eventsByScope;
    for (UpcomingEventsScope scope : scopes)
    {
        eventsByScope[scopeIndex(scope)] =
            upcomingEventsForScope(scope);
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

    const QStringList activeTypes =
        activeCalendarEventTypes();

    for (UpcomingEventsScope scope : scopes)
    {
        QList<CalendarEvent> filteredEvents;
        for (const CalendarEvent& event : eventsByScope[scopeIndex(scope)])
        {
            if (
                activeTypes.contains(
                    normalizedCalendarEventType(event.eventType)
                    )
                && calendarEventVisible(event)
                )
            {
                filteredEvents.append(event);
            }
        }

        if (scope == UpcomingEventsScope::Next10Events)
        {
            while (filteredEvents.size() > UpcomingEventsLimit)
            {
                filteredEvents.removeLast();
            }
        }

        for (const CalendarEvent& event : filteredEvents)
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
                        upcomingEventTimeText(event)
                        ) + UpcomingEventColumnTextPadding
                    );
        }
    }

    for (UpcomingEventsScope scope : scopes)
    {
        renderUpcomingEvents(
            scope,
            eventsByScope[scopeIndex(scope)],
            dateColumnWidth,
            timeColumnWidth,
            eventTypeColumnWidth
            );
    }
}
void CalendarPage::renderUpcomingEvents(
    UpcomingEventsScope scope,
    const QList<CalendarEvent>& events,
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

    const QStringList activeTypes =
        activeCalendarEventTypes();

    QList<CalendarEvent> filteredEvents;
    for (const CalendarEvent& event : events)
    {
        if (
            activeTypes.contains(
                normalizedCalendarEventType(event.eventType)
                )
            && calendarEventVisible(event)
            )
        {
            filteredEvents.append(event);
        }
    }

    if (scope == UpcomingEventsScope::Next10Events)
    {
        while (filteredEvents.size() > UpcomingEventsLimit)
        {
            filteredEvents.removeLast();
        }
    }

    if (filteredEvents.isEmpty())
    {
        auto* empty =
            new QLabel(
                tr("No upcoming events."),
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

    for (const CalendarEvent& event : filteredEvents)
    {
        layout->addWidget(
            createUpcomingEventRow(
                event,
                dateColumnWidth,
                timeColumnWidth,
                eventTypeColumnWidth,
                layout->parentWidget()
                )
            );
    }
}
QList<CalendarEvent> CalendarPage::upcomingEventsForScope(
    UpcomingEventsScope scope
    ) const
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
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
        return dataService->loadCalendarEventsInRange(
            firstOfMonth,
            firstOfMonth.addMonths(1).addDays(-1)
            );
    }

    case UpcomingEventsScope::Next30Days:
        return dataService->loadCalendarEventsInRange(
            today,
            today.addDays(UpcomingEventsNext30Days)
            );

    case UpcomingEventsScope::Next10Events:
        return dataService->loadUpcomingCalendarEvents(
            today,
            100
            );
    }

    return {};
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

    auto* dataService =
        openDataService(m_services);

    if (dataService)
    {
        const QColor storedColor(
            dataService
                ->loadSetting(
                    calendarEventTypeColorSettingKey(normalized),
                    QString()
                    )
                .toString()
            );

        if (storedColor.isValid())
        {
            return storedColor;
        }
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

    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    dataService->saveSetting(
        calendarEventTypeColorSettingKey(eventType),
        color.name(QColor::HexRgb)
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

    if (m_upcomingEventsTabs && m_upcomingEventsTabs->tabBar())
    {
        m_upcomingEventsTabs->setFont(
            navigationFont
            );
        m_upcomingEventsTabs->tabBar()->setFont(
            navigationFont
            );
        m_upcomingEventsTabs->updateGeometry();
        m_upcomingEventsTabs->tabBar()->updateGeometry();
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
    const CalendarEvent& event
    ) const
{
    if (!event.startDate.isValid())
    {
        return QStringLiteral("-");
    }

    if (
        !event.endDate.isValid()
        || event.endDate == event.startDate
        )
    {
        return event.startDate.toString(
            QStringLiteral("MMM d")
            );
    }

    const QString startFormat =
        event.startDate.year() == event.endDate.year()
            ? QStringLiteral("MMM d")
            : QStringLiteral("MMM d yyyy");

    return QStringLiteral("%1 - %2")
        .arg(
            event.startDate.toString(startFormat),
            event.endDate.toString(QStringLiteral("MMM d yyyy"))
            );
}
QString CalendarPage::upcomingEventTimeText(
    const CalendarEvent& event
    ) const
{
    if (event.allDay)
    {
        return tr("All day");
    }

    const QString timeStatus =
        normalizedCalendarEventTimeStatus(event.timeStatus);

    if (timeStatus == QStringLiteral("Unknown"))
    {
        return tr("Unknown Time");
    }

    if (timeStatus == QStringLiteral("Unconfirmed"))
    {
        return tr("Unconfirmed Time");
    }

    if (!event.startTime.isValid())
    {
        return QString();
    }

    auto* dataService =
        openDataService(m_services);
    const bool use24h =
        dataService
        && settingToBool(
            dataService->loadSetting(
                QStringLiteral("schedule_use_24h"),
                QStringLiteral("false")
                ),
            false
            );

    const QString format =
        use24h
            ? QStringLiteral("HH:mm")
            : QStringLiteral("h:mm AP");

    if (!event.endTime.isValid())
    {
        return event.startTime.toString(format);
    }

    return QStringLiteral("%1 - %2")
        .arg(
            event.startTime.toString(format),
            event.endTime.toString(format)
            );
}
bool CalendarPage::calendarEventVisible(
    const CalendarEvent& event
    ) const
{
    if (
        hideStartOfTermEvents()
        && isStartOfTermCalendarEvent(event)
        )
    {
        return false;
    }

    return CalendarEventCampusFilter::eventMatchesCampus(
        event,
        currentCampusCodes(),
        allCampusCodes(),
        showAllCalendarCampuses()
        );
}
bool CalendarPage::showAllCalendarCampuses() const
{
    auto* dataService =
        openDataService(m_services);

    return dataService
        && settingToBool(
            dataService->loadSetting(
                CalendarSettingsKeys::ShowEventsAtAllCampuses,
                false
                ),
            false
            );
}
bool CalendarPage::hideStartOfTermEvents() const
{
    auto* dataService =
        openDataService(m_services);

    return dataService
        && settingToBool(
            dataService->loadSetting(
                CalendarSettingsKeys::HideStartOfTermEvents,
                false
                ),
            false
            );
}
QStringList CalendarPage::currentCampusCodes() const
{
    QStringList codes;
    QString currentId;
    QString currentName;

    if (auto* dataService = openDataService(m_services))
    {
        currentName =
            dataService
                ->loadSetting(
                    QStringLiteral("myInfo/campus"),
                    QString()
                    )
                .toString();
        currentId =
            currentName;
    }

    codes.append(currentId);
    codes.append(currentName);

    const QList<CampusInfo> campuses =
        campusRepository().loadCampuses();

    for (const CampusInfo& campus : campuses)
    {
        if (
            campus.id.compare(currentId, Qt::CaseInsensitive) == 0
            || campusDisplayName(campus).compare(currentName, Qt::CaseInsensitive) == 0
            || campus.campusName.compare(currentName, Qt::CaseInsensitive) == 0
            )
        {
            codes.append(campus.campusCode);
            codes.append(campus.id);
            codes.append(campusDisplayName(campus));
        }
    }

    codes.removeAll(QString());
    codes.removeDuplicates();
    return codes;
}
QStringList CalendarPage::allCampusCodes() const
{
    QStringList codes;
    const QList<CampusInfo> campuses =
        campusRepository().loadCampuses();

    for (const CampusInfo& campus : campuses)
    {
        codes.append(campus.campusCode);
        codes.append(campus.id);
    }

    codes.removeAll(QString());
    codes.removeDuplicates();
    return codes;
}
QWidget* CalendarPage::createUpcomingEventRow(
    const CalendarEvent& event,
    int dateColumnWidth,
    int timeColumnWidth,
    int eventTypeColumnWidth,
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
        event.id
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
        event.id
        );

    auto* time =
        new QLabel(
            upcomingEventTimeText(event),
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
        event.id
        );

    auto* title =
        new MarqueeLabel(row);
    title->setText(event.title);
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
        event.id
        );

    auto* type =
        new QPushButton(
            normalizedCalendarEventType(event.eventType),
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
            normalizedCalendarEventType(event.eventType)
            )
        );
    type->setStyleSheet(
        eventTypeBadgeStyle(
            event.eventType,
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
                event.eventType
                );
        }
        );

    return row;
}
