#pragma once

#include "domain/models/calendar_event.h"
#include "ui/shared/pages/basepage.h"

#include <array>

#include <QColor>
#include <QDate>
#include <QHash>
#include <QList>
#include <QStringList>

class AcademicCalendarProvider;
class ApplicationServices;
class CalendarEventCache;
class CalendarEventModel;
class QEvent;
class QFont;
class QLabel;
class QPushButton;
class QQuickWidget;
class QScrollArea;
class QShowEvent;
class NavigationTabWidget;
class QVBoxLayout;
class QWidget;

enum class UpcomingEventsScope
{
    CurrentMonth = 0,
    Next30Days,
    Next10Events
};

constexpr int UpcomingEventsScopeCount = 3;

class CalendarPage : public BasePage
{
    Q_OBJECT

public:
    explicit CalendarPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;
    void scrollToTop();
    [[nodiscard]] AcademicCalendarProvider* academicCalendarProvider() const;
    void calendarPreferencesChanged(bool eventsChanged);

protected:
    void showEvent(
        QShowEvent* event
        ) override;

    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

private slots:
    void handleCalendarDayActivated(
        int year,
        int month,
        int day
        );
    void handleCalendarEventActivated(
        int eventId
        );
    void handleCalendarDisplayedMonthChanged(
        int year,
        int month
        );

private:
    static constexpr int UpcomingEventsLimit = 10;

    struct CalendarEventDisplayOptions
    {
        QStringList activeTypes;
        QStringList currentCampusCodes;
        QStringList allCampusCodes;
        bool showAllCampuses = false;
        bool hideStartOfTermEvents = false;
        bool use24HourTime = false;
    };

    void buildUi();
    void buildCalendarContent();
    void buildUpcomingEventsPanel(
        QVBoxLayout* cardLayout,
        QWidget* parent
        );
    QWidget* createUpcomingEventsPage(
        QVBoxLayout** pageLayout,
        QWidget* parent
        );
    QWidget* createEventTypeFilterRow(
        QWidget* parent
        );
    void openCalendarDialog(
        const CalendarEvent& event,
        bool existingEvent
        );
    void refreshCalendarData();
    void invalidateCalendarData();
    void ensureNextTenEvents();
    void handleNextEventMonthFound(
        const QDate& firstEventDate
        );
    void refreshUpcomingEvents();
    void updateCalendarCampusFilter();
    void renderUpcomingEvents(
        UpcomingEventsScope scope,
        const QList<CalendarEvent>& events,
        bool loading,
        bool use24HourTime,
        int dateColumnWidth,
        int timeColumnWidth,
        int eventTypeColumnWidth
        );
    QList<CalendarEvent> upcomingEventsForScope(
        UpcomingEventsScope scope
        ) const;
    bool upcomingEventsLoading(
        UpcomingEventsScope scope,
        const QList<CalendarEvent>& events
        ) const;
    CalendarEventDisplayOptions calendarEventDisplayOptions() const;
    QList<CalendarEvent> filterUpcomingEvents(
        const QList<CalendarEvent>& events,
        const CalendarEventDisplayOptions& options
        ) const;
    QStringList activeCalendarEventTypes() const;
    QColor calendarEventTypeColor(
        const QString& eventType
        ) const;
    void saveCalendarEventTypeColor(
        const QString& eventType,
        const QColor& color
        );
    void chooseCalendarEventTypeColor(
        const QString& eventType
        );
    QString eventTypeBadgeStyle(
        const QString& eventType,
        const QFont& font
        ) const;
    QString eventTypeFilterButtonStyle(
        const QString& eventType,
        bool checked,
        const QFont& font
        ) const;
    void syncEventTypeFilterButtons();
    void syncCalendarEventTypeColors();
    void syncCalendarFontSize();
    QString upcomingEventDateText(
        const CalendarEvent& event
        ) const;
    QString upcomingEventTimeText(
        const CalendarEvent& event,
        bool use24HourTime
        ) const;
    bool calendarEventVisible(
        const CalendarEvent& event,
        const CalendarEventDisplayOptions& options
        ) const;
    QWidget* createUpcomingEventRow(
        const CalendarEvent& event,
        int dateColumnWidth,
        int timeColumnWidth,
        int eventTypeColumnWidth,
        bool use24HourTime,
        QWidget* parent
        );
    QLabel* createTopLevelHeading(
        const QString& text,
        QWidget* parent
        ) const;

private:
    ApplicationServices* m_services = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_upcomingEventsHeading = nullptr;
    CalendarEventCache* m_calendarCache = nullptr;
    CalendarEventModel* m_calendarModel = nullptr;
    AcademicCalendarProvider* m_academicCalendarProvider = nullptr;
    QQuickWidget* m_calendarView = nullptr;
    NavigationTabWidget* m_upcomingEventsTabs = nullptr;
    std::array<QVBoxLayout*, UpcomingEventsScopeCount> m_upcomingEventLayouts{};
    QList<QPushButton*> m_eventTypeFilterButtons;
    QHash<QString, bool> m_eventTypeFilterStates;
    QDate m_calendarVisibleMonth;
    QDate m_nextTenSearchEnd;
    bool m_nextTenSearchComplete = false;
    bool m_nextTenLookupPending = false;
};
