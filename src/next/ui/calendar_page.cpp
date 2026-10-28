#include "next/ui/calendar_page.h"

#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "next/application/calendar_event_edit_draft.h"
#include "next/application/calendar_event_repeat_occurrence_plan.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"
#include "next/ui/calendar_event_dialog.h"
#include "next/ui/calendar_event_import_service.h"
#include "next/ui/calendar_preferences_dialog.h"

#include "data/database/workspace_store.h"

#include <QApplication>
#include <QCoreApplication>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFontInfo>
#include <QFrame>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMessageBox>
#include <QQuickItem>
#include <QQuickWidget>
#include <QPushButton>
#include <QPixmap>
#include <QUuid>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QQmlContext>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int EventIdRole = Qt::UserRole + 1;
constexpr int UpcomingLimit = 10;
constexpr int UpcomingSearchDays = 366;

constexpr std::array<Domain::CalendarEventType, 6> EventTypes{
    Domain::CalendarEventType::Vacation,
    Domain::CalendarEventType::Holiday,
    Domain::CalendarEventType::Workshop,
    Domain::CalendarEventType::CM,
    Domain::CalendarEventType::Meeting,
    Domain::CalendarEventType::Other
};

QString calendarPageText(const char* source)
{
    return QCoreApplication::translate("CalendarPage", source);
}

QString text(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString calendarEventTypeText(const std::string& value)
{
    for (const Domain::CalendarEventType type : EventTypes)
    {
        const std::string_view name = Domain::calendarEventTypeName(type);
        if (value == name)
        {
            return QCoreApplication::translate("CalendarEventDialog", name.data());
        }
    }
    return text(value);
}

QString typeLabel(const int index)
{
    switch (EventTypes.at(static_cast<std::size_t>(index)))
    {
    case Domain::CalendarEventType::Vacation: return calendarEventTypeText("Vacation");
    case Domain::CalendarEventType::Holiday: return calendarEventTypeText("Holiday");
    case Domain::CalendarEventType::Workshop: return calendarEventTypeText("Workshop");
    case Domain::CalendarEventType::CM: return calendarEventTypeText("CM");
    case Domain::CalendarEventType::Meeting: return calendarEventTypeText("Meeting");
    case Domain::CalendarEventType::Other: return calendarEventTypeText("Other");
    }
    return calendarPageText("Other");
}

QString defaultColor(const int index)
{
    static const std::array<QString, 6> colors{
        QStringLiteral("#4b6f91"),
        QStringLiteral("#7a5f9e"),
        QStringLiteral("#5f7f52"),
        QStringLiteral("#9a6b3f"),
        QStringLiteral("#8a4f5d"),
        QStringLiteral("#66727a")
    };
    return colors.at(static_cast<std::size_t>(index));
}

QString errorText(const Domain::OperationError& error)
{
    return text(error.message).isEmpty()
        ? calendarPageText("The calendar operation could not be completed.")
        : text(error.message);
}

QDate isoDate(const std::string& value)
{
    return QDate::fromString(text(value), Qt::ISODate);
}

std::optional<Domain::CalendarEventId> eventIdFromString(const QString& value)
{
    return Domain::CalendarEventId::fromString(value.toUtf8().toStdString());
}

QString timingText(const Application::CalendarEventSummary& event)
{
    if (event.allDay)
    {
        return QCoreApplication::translate("CalendarEventDialog", "All day");
    }
    if (event.startTime && event.endTime)
    {
        return text(*event.startTime) + QStringLiteral("–") + text(*event.endTime);
    }
    switch (event.timeStatus)
    {
    case Domain::CalendarEventTimeStatus::Unknown:
        return QCoreApplication::translate("CalendarEventDialog", "Time unknown");
    case Domain::CalendarEventTimeStatus::Unconfirmed:
        return QCoreApplication::translate("CalendarEventDialog", "Time unconfirmed");
    case Domain::CalendarEventTimeStatus::Timed:
        return {};
    }
    return {};
}

} // namespace

CalendarPage::CalendarPage(
    const PageContext& context,
    CalendarPageActions actions,
    QWidget* parent
    )
    : Page(context, parent),
      m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextCalendarPage"));
    buildShell();
    m_calendarImportService = new NextCalendarEventImportService(
        [this] { return activeWorkspacePath(); },
        [this] { return calendarImportCampusCodes(); },
        m_actions.calendarImportSourceUrl,
        m_actions.calendarImportTemporaryFileTemplate,
        m_actions.calendarImportNetworkAccessManagerFactory,
        this);
    connect(m_calendarImportService,
        &NextCalendarEventImportService::importingChanged,
        this,
        [this](const bool importing)
        {
            if (importing && m_status)
            {
                m_status->setText(QCoreApplication::translate(
                    "CalendarPreferencesDialog", "Importing events..."));
                m_status->show();
            }
        });
    connect(m_calendarImportService,
        &NextCalendarEventImportService::importFinished,
        this,
        [this](const int importedCount, const int skippedCount)
        {
            if (m_status)
            {
                m_status->setText(QCoreApplication::translate(
                    "CalendarPreferencesDialog",
                    "Imported %1 event(s). Skipped %2 existing or ignored item(s).")
                    .arg(importedCount)
                    .arg(skippedCount));
                m_status->show();
            }
            if (importedCount > 0 && m_cache)
            {
                m_cache->invalidate();
                updateRetentionAndQueries();
            }
        });
    connect(m_calendarImportService,
        &NextCalendarEventImportService::importFailed,
        this,
        [this](const QString& message)
        {
            if (m_status)
            {
                m_status->setText(QCoreApplication::translate(
                    "CalendarPreferencesDialog", "Calendar import failed: %1")
                    .arg(message));
                m_status->show();
            }
        });
    connect(m_calendarImportService,
        &NextCalendarEventImportService::importCanceled,
        this,
        [this]
        {
            if (m_status)
            {
                m_status->setText(QCoreApplication::translate(
                    "CalendarPreferencesDialog", "Calendar import was canceled."));
                m_status->show();
            }
        });
}

void CalendarPage::buildShell()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(20, 16, 20, 16);
    m_rootLayout->setSpacing(12);

    auto* heading = new QWidget(this);
    auto* headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(calendarPageText("Calendar"), heading);
    title->setObjectName(QStringLiteral("calendarPageTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    auto* subtitle = new QLabel(
        calendarPageText("View and manage monthly events and upcoming dates."), heading);
    subtitle->setObjectName(QStringLiteral("calendarPageSubtitle"));
    subtitle->setWordWrap(true);
    headingLayout->addWidget(title);
    headingLayout->addWidget(subtitle);

    auto* toolbar = new QHBoxLayout;
    toolbar->addStretch(1);
    auto* newEvent = new QPushButton(calendarPageText("New Event"), heading);
    newEvent->setObjectName(QStringLiteral("calendarNewEvent"));
    auto* preferences = new QPushButton(calendarPageText("Preferences"), heading);
    preferences->setObjectName(QStringLiteral("calendarPreferences"));
    toolbar->addWidget(newEvent);
    toolbar->addWidget(preferences);
    headingLayout->addLayout(toolbar);
    m_rootLayout->addWidget(heading);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("calendarPageStatus"));
    m_status->setWordWrap(true);
    m_status->hide();
    m_rootLayout->addWidget(m_status);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("calendarPageLazyContent"));
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->addWidget(m_content, 1);

    connect(newEvent, &QPushButton::clicked, this, [this]
    {
        const QDate date = m_visibleMonth.isValid()
            ? qMax(QDate::currentDate(), m_visibleMonth)
            : QDate::currentDate();
        Application::CalendarEventEditDraft draft;
        draft.startDate = date.toString(Qt::ISODate).toStdString();
        draft.endDate = draft.startDate;
        draft.startTime = std::string("09:00");
        draft.endTime = std::string("10:00");
        openEventDialog(std::move(draft), false);
    });
    connect(preferences, &QPushButton::clicked,
        this, &CalendarPage::openCalendarPreferences);
}

PageOperationResult<void> CalendarPage::onActivating()
{
    ensureCalendarContent();
    reloadWorkspaceState();
    updateRetentionAndQueries();
    refreshUpcoming();
    return {};
}

void CalendarPage::onSuspending() noexcept
{
    if (m_calendarImportService)
    {
        m_calendarImportService->cancel();
    }
    releaseCalendarContent();
}

void CalendarPage::ensureCalendarContent()
{
    if (m_contentActive)
    {
        return;
    }
    m_contentActive = true;

    m_calendarCard = new QWidget(m_content);
    m_calendarCard->setObjectName(QStringLiteral("nextCalendarContentCard"));
    auto* cardLayout = new QVBoxLayout(m_calendarCard);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(12);

    m_cache = new CalendarEventCache(this);
    m_model = new CalendarEventModel(m_cache, this);
    connect(m_cache, &CalendarEventCache::cacheChanged, this,
        &CalendarPage::refreshUpcoming);

    m_visibleMonth = QDate::currentDate();
    if (m_visibleMonth.year() < 2026)
    {
        m_visibleMonth = QDate(2026, 1, 1);
    }
    m_visibleMonth = QDate(m_visibleMonth.year(), m_visibleMonth.month(), 1);

    m_workspaceStore = new WorkspaceStore();
    m_workspacePath = activeWorkspacePath();
    if (!m_workspacePath.isEmpty())
    {
        const RepositoryStatus opened = m_workspaceStore->openExisting(m_workspacePath);
        if (!opened)
        {
            const QString diagnostic = toLegacyDiagnostic(opened.error());
            m_status->setText(diagnostic.isEmpty()
                ? calendarPageText("The workspace calendar could not be opened.")
                : diagnostic);
            m_status->show();
        }
    }
    m_academicProvider = new AcademicCalendarProvider(
        std::make_unique<Platform::ApplicationServicesAcademicCalendarSchedulePreferencesPort>(
            *m_workspaceStore),
        std::make_unique<Platform::ApplicationServicesCalendarFirstDayOfWeekPreferencesPort>(
            *m_workspaceStore),
        this);

    m_calendarView = new QQuickWidget(m_calendarCard);
    m_calendarView->setObjectName(QStringLiteral("nextCalendarMonthGrid"));
    m_calendarView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_calendarView->setMinimumHeight(500);
    m_calendarView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_calendarView->rootContext()->setContextProperty(
        QStringLiteral("calendarEventProvider"), m_model);
    m_calendarView->rootContext()->setContextProperty(
        QStringLiteral("academicCalendarProvider"), m_academicProvider);
    m_calendarView->setInitialProperties({
        {QStringLiteral("shownDate"), QDateTime(m_visibleMonth, QTime(0, 0))}
    });
    m_calendarView->setSource(QUrl(QStringLiteral(
        "qrc:/qt/qml/ClassMngr/Calendar/EventCalendar.qml")));
    ++m_calendarViewConstructionCount;
    cardLayout->addWidget(m_calendarView, 1);

    auto* upcomingHeader = new QHBoxLayout;
    auto* upcomingTitle = new QLabel(calendarPageText("Upcoming Events"), m_calendarCard);
    upcomingTitle->setObjectName(QStringLiteral("calendarUpcomingTitle"));
    upcomingHeader->addWidget(upcomingTitle);
    upcomingHeader->addStretch(1);
    m_upcomingScope = new QComboBox(m_calendarCard);
    m_upcomingScope->setObjectName(QStringLiteral("calendarUpcomingScope"));
    m_upcomingScope->addItem(calendarPageText("This month"), static_cast<int>(UpcomingScope::CurrentMonth));
    m_upcomingScope->addItem(calendarPageText("Next 30 days"), static_cast<int>(UpcomingScope::Next30Days));
    m_upcomingScope->addItem(calendarPageText("Next 10 events"), static_cast<int>(UpcomingScope::Next10Events));
    upcomingHeader->addWidget(m_upcomingScope);
    cardLayout->addLayout(upcomingHeader);

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(calendarPageText("Event types"), m_calendarCard));
    for (int index = 0; index < static_cast<int>(m_eventTypeFilters.size()); ++index)
    {
        auto* filter = new QPushButton(typeLabel(index), m_calendarCard);
        filter->setObjectName(QStringLiteral("calendarEventTypeFilter%1").arg(index));
        filter->setCheckable(true);
        filter->setChecked(true);
        m_eventTypeFilters[static_cast<std::size_t>(index)] = filter;
        filterRow->addWidget(filter);
        connect(filter, &QPushButton::toggled,
            this, &CalendarPage::updateEventTypeFilter);
    }
    filterRow->addStretch(1);
    cardLayout->addLayout(filterRow);

    m_upcomingList = new QListWidget(m_calendarCard);
    m_upcomingList->setObjectName(QStringLiteral("calendarUpcomingEvents"));
    m_upcomingList->setMinimumHeight(150);
    m_upcomingList->setMaximumHeight(280);
    cardLayout->addWidget(m_upcomingList);
    connect(m_upcomingScope, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this]
        {
            updateRetentionAndQueries();
            refreshUpcoming();
        });
    connect(m_upcomingList, &QListWidget::itemDoubleClicked,
        this, &CalendarPage::handleUpcomingItemActivated);

    m_contentLayout->addWidget(m_calendarCard);

    if (QObject* const root = m_calendarView->rootObject())
    {
        connect(root, SIGNAL(dayActivated(int,int,int)),
            this, SLOT(handleCalendarDayActivated(int,int,int)));
        connect(root, SIGNAL(eventActivated(QString)),
            this, SLOT(handleCalendarEventActivated(QString)));
        connect(root, SIGNAL(displayedMonthChanged(int,int)),
            this, SLOT(handleCalendarDisplayedMonthChanged(int,int)));
        applyCalendarPalette();
        renderCalendarColors();
    }
    else
    {
        const QString detail = m_calendarView->errors().isEmpty()
            ? calendarPageText("The calendar grid could not be loaded.")
            : m_calendarView->errors().front().toString();
        m_status->setText(detail);
        m_status->show();
    }

    updateCampusFilter();
    updateEventTypeFilter();
    if (m_calendarView->rootObject())
    {
        handleCalendarDisplayedMonthChanged(m_visibleMonth.year(), m_visibleMonth.month());
    }
}

void CalendarPage::releaseCalendarContent() noexcept
{
    if (!m_contentActive)
    {
        return;
    }
    m_contentActive = false;
    if (m_calendarView)
    {
        delete m_calendarView;
        m_calendarView = nullptr;
    }
    m_upcomingList = nullptr;
    m_upcomingScope = nullptr;
    QWidget* const card = m_calendarCard;
    m_calendarCard = nullptr;
    if (m_model)
    {
        delete m_model;
        m_model = nullptr;
    }
    if (m_cache)
    {
        delete m_cache;
        m_cache = nullptr;
    }
    if (m_academicProvider)
    {
        delete m_academicProvider;
        m_academicProvider = nullptr;
    }
    if (m_workspaceStore)
    {
        m_workspaceStore->close();
        delete m_workspaceStore;
        m_workspaceStore = nullptr;
    }
    if (card)
    {
        m_contentLayout->removeWidget(card);
        delete card;
    }
    else if (m_contentLayout && m_contentLayout->count() > 0)
    {
        QLayoutItem* const item = m_contentLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
    m_workspacePath.clear();
    m_currentCampusCodes.clear();
    m_allCampusCodes.clear();
    m_upcomingResultCount = 0;
    for (QPushButton*& filter : m_eventTypeFilters)
    {
        filter = nullptr;
    }
    m_status->clear();
    m_status->hide();
}

void CalendarPage::reloadWorkspaceState()
{
    if (!m_contentActive)
    {
        return;
    }
    const QString path = activeWorkspacePath();
    if (path != m_workspacePath)
    {
        if (m_calendarImportService)
        {
            m_calendarImportService->cancel();
        }
        if (m_workspaceStore)
        {
            m_workspaceStore->close();
            if (!path.isEmpty())
            {
                const RepositoryStatus opened = m_workspaceStore->openExisting(path);
                if (!opened)
                {
                    m_status->setText(calendarPageText("The workspace calendar could not be opened."));
                    m_status->show();
                }
            }
        }
        m_workspacePath = path;
        if (m_cache)
        {
            m_cache->setDatabasePath(path);
        }
        if (m_academicProvider)
        {
            m_academicProvider->reload();
        }
    }
    updateCampusFilter();
    renderCalendarColors();
}

void CalendarPage::updateRetentionAndQueries()
{
    if (!m_cache || !m_visibleMonth.isValid())
    {
        return;
    }
    const CalendarEventCache::DateRange visible = visibleGridRange();
    const CalendarEventCache::DateRange upcoming = upcomingRange();
    m_cache->setDatabasePath(m_workspacePath);
    m_cache->setRetainedRanges({visible, upcoming});
    if (!m_workspacePath.isEmpty())
    {
        m_cache->requestRange(visible.startDate, visible.endDate);
        m_cache->requestRange(upcoming.startDate, upcoming.endDate);
    }
}

void CalendarPage::updateCampusFilter()
{
    if (!m_model)
    {
        return;
    }
    m_currentCampusCodes.clear();
    m_allCampusCodes.clear();
    m_displayPreferences = {};

    if (m_actions.readDisplayPreferences && !m_workspacePath.isEmpty())
    {
        const auto preferences = m_actions.readDisplayPreferences(m_workspacePath);
        if (preferences)
        {
            m_displayPreferences = preferences.value();
        }
    }

    QString currentCampus;
    if (m_actions.readCurrentCampus && !m_workspacePath.isEmpty())
    {
        const auto result = m_actions.readCurrentCampus(m_workspacePath);
        if (result)
        {
            currentCampus = text(result.value().value()).trimmed();
            if (!currentCampus.isEmpty())
            {
                m_currentCampusCodes.append(currentCampus);
            }
        }
    }

    if (m_actions.readCampusDirectory)
    {
        const auto campuses = m_actions.readCampusDirectory();
        for (const Application::CalendarPageCampusMetadata& campus : campuses)
        {
            const QString id = text(campus.id.value());
            const QString name = text(campus.campusName);
            const QString code = campus.campusCode
                ? text(campus.campusCode->value())
                : QString();
            for (const QString& candidate : {id, name, code})
            {
                if (!candidate.isEmpty() && !m_allCampusCodes.contains(candidate))
                {
                    m_allCampusCodes.append(candidate);
                }
            }
            if (!currentCampus.isEmpty()
                && (id.compare(currentCampus, Qt::CaseInsensitive) == 0
                    || name.compare(currentCampus, Qt::CaseInsensitive) == 0
                    || code.compare(currentCampus, Qt::CaseInsensitive) == 0))
            {
                for (const QString& candidate : {id, name, code})
                {
                    if (!candidate.isEmpty()
                        && !m_currentCampusCodes.contains(candidate))
                    {
                        m_currentCampusCodes.append(candidate);
                    }
                }
            }
        }
    }

    m_model->setCampusFilter(
        m_currentCampusCodes,
        m_allCampusCodes,
        m_displayPreferences.showEventsAtAllCampuses,
        m_displayPreferences.hideStartOfTermEvents);
    refreshUpcoming();
}

void CalendarPage::updateEventTypeFilter()
{
    renderEventTypeFilterStyles();
    refreshUpcoming();
}

void CalendarPage::refreshUpcoming()
{
    if (!m_cache || !m_model || !m_upcomingList)
    {
        return;
    }
    const CalendarEventCache::DateRange range = upcomingRange();
    m_upcomingResultCount = 0;
    m_upcomingList->clear();

    if (!m_workspacePath.isEmpty()
        && !m_cache->isRangeLoaded(range.startDate, range.endDate))
    {
        const QString error = m_cache->rangeError(range.startDate, range.endDate);
        if (!error.isEmpty())
        {
            m_status->setText(error);
            m_status->show();
        }
        else
        {
            m_status->setText(calendarPageText("Loading upcoming events…"));
            m_status->show();
        }
        return;
    }

    const Application::CalendarEventProjection projection =
        m_model->visibleEventsInRange(range.startDate, range.endDate);
    QStringList activeTypes;
    for (int index = 0; index < static_cast<int>(m_eventTypeFilters.size()); ++index)
    {
        const QPushButton* const filter = m_eventTypeFilters.at(
            static_cast<std::size_t>(index));
        if (!filter || !filter->isChecked())
        {
            continue;
        }
        const std::string_view name = Domain::calendarEventTypeName(
            EventTypes.at(static_cast<std::size_t>(index)));
        activeTypes.append(QString::fromUtf8(
            name.data(), static_cast<qsizetype>(name.size())));
    }
    for (const Application::CalendarEventSummary& event : projection.events())
    {
        if (!activeTypes.contains(text(event.eventType.value())))
        {
            continue;
        }
        if (m_upcomingResultCount >= UpcomingLimit)
        {
            break;
        }
        const QDate startDate = isoDate(event.startDate);
        const QDate endDate = isoDate(event.endDate);
        if (!startDate.isValid() || !endDate.isValid() || endDate < range.startDate)
        {
            continue;
        }
        const QString dateText = QLocale().toString(
            startDate, QLocale::ShortFormat);
        const QString timing = timingText(event);
        QString label = dateText;
        if (!timing.isEmpty())
        {
            label += QStringLiteral(" · ") + timing;
        }
        label += QStringLiteral("  ") + text(event.title)
            + QStringLiteral("  ·  ")
            + calendarEventTypeText(event.eventType.value());
        auto* item = new QListWidgetItem(label, m_upcomingList);
        item->setData(EventIdRole, text(event.id.value()));
        int colorIndex = static_cast<int>(EventTypes.size()) - 1;
        for (int index = 0; index < static_cast<int>(EventTypes.size()); ++index)
        {
            if (event.eventType.value()
                == Domain::calendarEventTypeName(
                    EventTypes.at(static_cast<std::size_t>(index))))
            {
                colorIndex = index;
                break;
            }
        }
        QColor eventColor(QString::fromStdString(
            m_eventColors.at(static_cast<std::size_t>(colorIndex))));
        if (!eventColor.isValid())
        {
            eventColor = QColor(defaultColor(colorIndex));
        }
        QPixmap colorCue(12, 12);
        colorCue.fill(eventColor);
        item->setIcon(QIcon(colorCue));
        item->setToolTip(calendarEventTypeText(event.eventType.value()));
        ++m_upcomingResultCount;
    }

    if (m_upcomingResultCount == 0)
    {
        m_status->setText(calendarPageText("There are no upcoming events in this range."));
        m_status->show();
    }
    else
    {
        m_status->clear();
        m_status->hide();
    }
}

void CalendarPage::renderCalendarColors()
{
    if (!m_calendarView)
    {
        return;
    }
    QObject* const root = m_calendarView->rootObject();
    QVariantMap colors;
    QVariantMap textColors;
    for (int index = 0; index < static_cast<int>(EventTypes.size()); ++index)
    {
        const auto type = EventTypes.at(static_cast<std::size_t>(index));
        QString value = defaultColor(index);
        if (m_actions.readEventTypeColor && !m_workspacePath.isEmpty())
        {
            const auto saved = m_actions.readEventTypeColor(m_workspacePath, type);
            if (saved && QColor(text(saved.value())).isValid())
            {
                value = text(saved.value());
            }
        }
        m_eventColors[static_cast<std::size_t>(index)] = value.toStdString();
        const std::string_view name = Domain::calendarEventTypeName(type);
        const QString key = QString::fromUtf8(
            name.data(), static_cast<qsizetype>(name.size()));
        const QColor color(value);
        colors.insert(key, color);
        textColors.insert(key, color.lightness() < 128
            ? QColor(Qt::white) : QColor(Qt::black));
    }
    if (root)
    {
        root->setProperty("eventTypeColors", colors);
        root->setProperty("eventTypeTextColors", textColors);
    }
    renderEventTypeFilterStyles();
}

void CalendarPage::renderEventTypeFilterStyles()
{
    const QPalette palette = QApplication::palette();
    for (int index = 0; index < static_cast<int>(m_eventTypeFilters.size()); ++index)
    {
        QPushButton* const filter = m_eventTypeFilters.at(
            static_cast<std::size_t>(index));
        if (!filter)
        {
            continue;
        }
        QColor color(QString::fromStdString(
            m_eventColors.at(static_cast<std::size_t>(index))));
        if (!color.isValid())
        {
            color = QColor(defaultColor(index));
        }
        const bool checked = filter->isChecked();
        const QString foreground = checked
            ? (color.lightness() < 128 ? QStringLiteral("#ffffff")
                                       : QStringLiteral("#111111"))
            : palette.color(QPalette::Text).name(QColor::HexRgb);
        const QString background = checked
            ? color.name(QColor::HexRgb) : QStringLiteral("transparent");
        const QString border = checked
            ? color.name(QColor::HexRgb) : QStringLiteral("#a8b2b8");
        filter->setFixedHeight(30);
        filter->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
            "border-radius: 6px; padding: 4px 10px; }"
            "QPushButton:hover { border-color: %4; }")
            .arg(background, foreground, border, color.name(QColor::HexRgb)));
    }
}

void CalendarPage::applyCalendarPalette()
{
    if (!m_calendarView || !m_calendarView->rootObject())
    {
        return;
    }
    QObject* const root = m_calendarView->rootObject();
    const QPalette palette = QApplication::palette();
    const bool dark = palette.color(QPalette::Window).lightness() < 128;
    root->setProperty("toolbarColor", dark ? QColor(QStringLiteral("#353b43"))
                                            : QColor(QStringLiteral("#536f8a")));
    root->setProperty("toolbarTextColor", dark ? QColor(QStringLiteral("#f5f4f0"))
                                                : QColor(QStringLiteral("#fbfaf7")));
    root->setProperty("textColor", palette.color(QPalette::Text));
    root->setProperty("mutedTextColor", dark ? QColor(QStringLiteral("#bac1c8"))
                                              : QColor(QStringLiteral("#66727a")));
    root->setProperty("inactiveTextColor", dark ? QColor(QStringLiteral("#747c84"))
                                                  : QColor(QStringLiteral("#a5aaa9")));
    root->setProperty("calendarBackground", palette.color(QPalette::Base));
    root->setProperty("headerBackground", palette.color(QPalette::AlternateBase));
    root->setProperty("cellBackground", palette.color(QPalette::Base));
    root->setProperty("weekendBackground", dark ? QColor(QStringLiteral("#30363d"))
                                                  : QColor(QStringLiteral("#e8edf1")));
    root->setProperty("gridLineColor", palette.color(QPalette::Mid));
    root->setProperty("accentColor", palette.color(QPalette::Highlight));
    root->setProperty("accentTextColor", palette.color(QPalette::HighlightedText));
    const int pixelSize = QFontInfo(m_calendarView->font()).pixelSize();
    if (pixelSize > 0)
    {
        root->setProperty("baseFontPixelSize", pixelSize);
    }
}

void CalendarPage::openCalendarPreferences()
{
    if (!m_contentActive)
    {
        return;
    }
    CalendarPreferencesActions actions;
    actions.readDisplayPreferences = m_actions.readDisplayPreferences;
    actions.saveDisplayPreferences = m_actions.saveDisplayPreferences;
    actions.deleteAllEvents = m_actions.deleteAllEvents;
    actions.readEventTypeColor = m_actions.readEventTypeColor;
    actions.saveEventTypeColor = m_actions.saveEventTypeColor;
    actions.calendarImportService = m_calendarImportService;
    actions.preferencesChanged = [this](const bool eventsChanged)
    {
        if (eventsChanged && m_cache)
        {
            m_cache->invalidate();
            updateRetentionAndQueries();
        }
        updateCampusFilter();
        renderCalendarColors();
    };
    CalendarPreferencesDialog dialog(
        m_workspacePath, m_academicProvider, std::move(actions), this);
    static_cast<void>(dialog.exec());
}

void CalendarPage::openEventDialog(
    Application::CalendarEventEditDraft draft,
    const bool existingEvent
    )
{
    if (m_workspacePath.isEmpty())
    {
        m_status->setText(calendarPageText("Open a workspace to edit calendar events."));
        m_status->show();
        return;
    }

    Application::CalendarEventEditDraft currentDraft = std::move(draft);
    const std::string seriesStartDate = currentDraft.startDate;
    CalendarEventSeriesEditScope currentSeriesEditScope =
        CalendarEventSeriesEditScope::ThisEventOnly;
    while (true)
    {
        CalendarEventDialog dialog(
            currentDraft,
            existingEvent,
            this,
            currentSeriesEditScope);
        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }
        const Application::CalendarEventEditDraft edited = dialog.eventData();
        currentSeriesEditScope = dialog.seriesEditScope();
        bool success = false;
        QString error;

        if (dialog.operation() == CalendarEventDialogOperation::Delete)
        {
            const auto answer = QMessageBox::question(
                this,
                calendarPageText("Delete Calendar Event?"),
                calendarPageText("Delete this event? This action cannot be undone."),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes)
            {
                currentDraft = edited;
                continue;
            }

            if (dialog.seriesEditScope()
                    == CalendarEventSeriesEditScope::ThisAndFollowingEvents
                && currentDraft.repeatSeriesId)
            {
                const Application::CalendarEventSeriesDeleteInput input{
                    currentDraft.repeatSeriesId,
                    seriesStartDate
                };
                const auto result = m_actions.deleteRepeatSeries
                    ? m_actions.deleteRepeatSeries(m_workspacePath, input)
                    : Application::CalendarEventSeriesDeleteResult::failure({
                          .code = Domain::ErrorCode::NotFound,
                          .message = "Calendar repeat-series deletion is unavailable.",
                          .recoverable = true
                      });
                success = result.hasValue();
                if (!result)
                {
                    error = errorText(result.error());
                }
            }
            else if (currentDraft.id)
            {
                const auto result = m_actions.deleteEvent
                    ? m_actions.deleteEvent(m_workspacePath, *currentDraft.id)
                    : Application::CalendarEventDeleteResult::failure({
                          .code = Domain::ErrorCode::NotFound,
                          .message = "Calendar event deletion is unavailable.",
                          .recoverable = true
                      });
                success = result.hasValue();
                if (!result)
                {
                    error = errorText(result.error());
                }
            }
        }
        else
        {
            const Application::CalendarEventSaveInput saveInput{
                .id = edited.id,
                .title = edited.title,
                .startDate = edited.startDate,
                .endDate = edited.endDate,
                .startTime = edited.startTime,
                .endTime = edited.endTime,
                .allDay = edited.allDay,
                .eventType = edited.eventType,
                .timeStatus = edited.timeStatus
            };
            if (edited.repeatSeriesId
                && dialog.seriesEditScope()
                    == CalendarEventSeriesEditScope::ThisAndFollowingEvents)
            {
                const Application::CalendarEventSeriesEditInput input{
                    edited.repeatSeriesId,
                    seriesStartDate,
                    edited.startDate,
                    edited.endDate,
                    edited.title,
                    edited.startTime,
                    edited.endTime,
                    edited.allDay,
                    edited.eventType,
                    edited.timeStatus
                };
                const auto result = m_actions.editRepeatSeries
                    ? m_actions.editRepeatSeries(m_workspacePath, input)
                    : Application::CalendarEventSeriesEditResult::failure({
                          .code = Domain::ErrorCode::NotFound,
                          .message = "Calendar repeat-series editing is unavailable.",
                          .recoverable = true
                      });
                success = result.hasValue();
                if (!result)
                {
                    error = errorText(result.error());
                }
            }
            else if (dialog.repeatEnabled())
            {
                const std::optional<Domain::CalendarEventSeriesId> seriesId =
                    Domain::CalendarEventSeriesId::fromString(
                        QUuid::createUuid().toString(QUuid::WithoutBraces)
                            .toStdString());
                const Application::CalendarEventSeriesCreateInput input{
                    .seed = saveInput,
                    .repeatSeriesId = seriesId,
                    .frequency = dialog.repeatFrequency(),
                    .untilDate = dialog.repeatUntilDate().toStdString()
                };
                const auto result = m_actions.createRepeatSeries
                    ? m_actions.createRepeatSeries(m_workspacePath, input)
                    : Application::CalendarEventSeriesCreateResult::failure({
                          .code = Domain::ErrorCode::NotFound,
                          .message = "Calendar repeat-series creation is unavailable.",
                          .recoverable = true
                      });
                success = result.hasValue();
                if (!result)
                {
                    error = errorText(result.error());
                }
            }
            else
            {
                const auto result = m_actions.saveEvent
                    ? m_actions.saveEvent(m_workspacePath, saveInput)
                    : Application::CalendarEventSaveResult::failure({
                          .code = Domain::ErrorCode::NotFound,
                          .message = "Calendar event saving is unavailable.",
                          .recoverable = true
                      });
                success = result.hasValue();
                if (!result)
                {
                    error = errorText(result.error());
                }
            }
        }

        if (success)
        {
            invalidateAfterMutation();
            return;
        }

        QMessageBox::warning(this, calendarPageText("Calendar Event Not Saved"),
            error.isEmpty() ? calendarPageText("The event could not be saved. Your edits are still available.")
                            : error);
        currentDraft = edited;
    }
}

void CalendarPage::invalidateAfterMutation()
{
    if (m_cache)
    {
        m_cache->invalidate();
        updateRetentionAndQueries();
    }
    updateCampusFilter();
    refreshUpcoming();
}

CalendarEventCache::DateRange CalendarPage::visibleGridRange() const
{
    if (!m_visibleMonth.isValid())
    {
        return {};
    }
    const int firstDay = m_academicProvider
        ? m_academicProvider->firstDayOfWeek()
        : QLocale().firstDayOfWeek() % 7;
    const QDate firstOfMonth(m_visibleMonth.year(), m_visibleMonth.month(), 1);
    const int offset = (firstOfMonth.dayOfWeek() % 7 - firstDay + 7) % 7;
    const QDate start = firstOfMonth.addDays(-offset);
    const int rowCount = ((offset + firstOfMonth.daysInMonth() + 6) / 7);
    return {start, start.addDays(rowCount * 7 - 1)};
}

CalendarEventCache::DateRange CalendarPage::upcomingRange() const
{
    const auto scope = static_cast<UpcomingScope>(
        m_upcomingScope ? m_upcomingScope->currentData().toInt()
                        : static_cast<int>(UpcomingScope::CurrentMonth));
    if (scope == UpcomingScope::CurrentMonth && m_visibleMonth.isValid())
    {
        const QDate first(m_visibleMonth.year(), m_visibleMonth.month(), 1);
        return {first, first.addMonths(1).addDays(-1)};
    }
    const QDate start = QDate::currentDate();
    if (scope == UpcomingScope::Next10Events)
    {
        return {start, start.addDays(UpcomingSearchDays - 1)};
    }
    return {start, start.addDays(29)};
}

QString CalendarPage::activeWorkspacePath() const
{
    return m_actions.activeWorkspacePath
        ? m_actions.activeWorkspacePath()
        : QString();
}

QStringList CalendarPage::calendarImportCampusCodes() const
{
    QStringList codes;
    if (!m_actions.readCampusDirectory)
    {
        return codes;
    }
    for (const Application::CalendarPageCampusMetadata& campus :
         m_actions.readCampusDirectory())
    {
        if (!campus.campusCode.has_value())
        {
            continue;
        }
        const std::string& value = campus.campusCode->value();
        const QString code = QString::fromUtf8(
            value.data(), static_cast<qsizetype>(value.size())).trimmed();
        if (!code.isEmpty() && !codes.contains(code, Qt::CaseInsensitive))
        {
            codes.append(code);
        }
    }
    return codes;
}

Application::CalendarEventEditDraft CalendarPage::draftFromSummary(
    const Application::CalendarEventSummary& summary
    ) const
{
    Application::CalendarEventEditDraft draft;
    draft.id = summary.id;
    draft.repeatSeriesId = summary.repeatSeriesId;
    draft.title = summary.title;
    draft.startDate = summary.startDate;
    draft.endDate = summary.endDate;
    draft.startTime = summary.startTime;
    draft.endTime = summary.endTime;
    draft.allDay = summary.allDay;
    draft.eventType = summary.eventType.value();
    draft.timeStatus = summary.timeStatus;
    return draft;
}

void CalendarPage::handleCalendarDayActivated(
    const int year,
    const int month,
    const int day
    )
{
    const QDate selected(year, month, day);
    if (!selected.isValid())
    {
        return;
    }
    Application::CalendarEventEditDraft draft;
    draft.startDate = selected.toString(Qt::ISODate).toStdString();
    draft.endDate = draft.startDate;
    draft.startTime = std::string("09:00");
    draft.endTime = std::string("10:00");
    openEventDialog(std::move(draft), false);
}

void CalendarPage::handleCalendarEventActivated(const QString& value)
{
    const auto eventId = eventIdFromString(value);
    if (!eventId || !m_actions.readEvent)
    {
        return;
    }
    const auto result = m_actions.readEvent(m_workspacePath, *eventId);
    if (!result)
    {
        QMessageBox::warning(this, calendarPageText("Calendar Event"), errorText(result.error()));
        return;
    }
    openEventDialog(draftFromSummary(result.value()), true);
}

void CalendarPage::handleCalendarDisplayedMonthChanged(
    const int year,
    const int month
    )
{
    const QDate shown(year, month, 1);
    if (!shown.isValid())
    {
        return;
    }
    m_visibleMonth = shown;
    updateRetentionAndQueries();
    refreshUpcoming();
}

void CalendarPage::handleUpcomingItemActivated(QListWidgetItem* item)
{
    if (item)
    {
        handleCalendarEventActivated(item->data(EventIdRole).toString());
    }
}

CalendarPageRuntimeMetrics CalendarPage::runtimeMetrics() const
{
    const CalendarEventCache::DateRange visible = visibleGridRange();
    const CalendarEventCache::DateRange upcoming = upcomingRange();
    CalendarPageRuntimeMetrics metrics;
    if (m_cache)
    {
        metrics.cacheEventCount = m_cache->eventCount();
        metrics.cacheDateBucketCount = m_cache->dateBucketCount();
        metrics.cacheLoadedRangeCount = m_cache->loadedRangeCount();
        metrics.cacheRetainedRangeCount = m_cache->retainedRangeCount();
        metrics.cacheLoading = m_cache->isLoading();
    }
    if (visible.startDate.isValid() && visible.endDate.isValid())
    {
        metrics.visibleGridDayCount = static_cast<int>(
            visible.startDate.daysTo(visible.endDate) + 1);
    }
    if (upcoming.startDate.isValid() && upcoming.endDate.isValid())
    {
        metrics.upcomingRangeDayCount = static_cast<int>(
            upcoming.startDate.daysTo(upcoming.endDate) + 1);
    }
    metrics.upcomingResultCount = m_upcomingResultCount;
    metrics.calendarViewCount = m_calendarView ? 1 : 0;
    metrics.calendarRootObjectCount = m_calendarView
        && m_calendarView->rootObject() ? 1 : 0;
    metrics.calendarViewConstructionCount = m_calendarViewConstructionCount;
    return metrics;
}

CalendarEventModel* CalendarPage::eventModel() const noexcept
{
    return m_model;
}

CalendarEventCache* CalendarPage::eventCache() const noexcept
{
    return m_cache;
}

QQuickWidget* CalendarPage::calendarView() const noexcept
{
    return m_calendarView;
}

QListWidget* CalendarPage::upcomingList() const noexcept
{
    return m_upcomingList;
}

void CalendarPage::changeEvent(QEvent* event)
{
    Page::changeEvent(event);
    if (event && (event->type() == QEvent::PaletteChange
                  || event->type() == QEvent::ApplicationPaletteChange
                  || event->type() == QEvent::FontChange))
    {
        applyCalendarPalette();
    }
}

} // namespace ClassMngr::Next::Ui
