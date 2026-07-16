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
class CalendarEventModel;
class QEvent;
class QFont;
class QLabel;
class QPushButton;
class QQuickWidget;
class QScrollArea;
class QShowEvent;
class QTabWidget;
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
    void retranslateUi() override;
    void scrollToTop();

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
    void handleCalendarConfigureRequested(
        int year,
        int month
        );
    void handleCalendarDisplayedMonthChanged(
        int year,
        int month
        );

private:
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
    void refreshUpcomingEvents();
    void updateCalendarCampusFilter();
    void renderUpcomingEvents(
        UpcomingEventsScope scope,
        const QList<CalendarEvent>& events,
        int dateColumnWidth,
        int timeColumnWidth,
        int eventTypeColumnWidth
        );
    QList<CalendarEvent> upcomingEventsForScope(
        UpcomingEventsScope scope
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
        const CalendarEvent& event
        ) const;
    bool calendarEventVisible(
        const CalendarEvent& event
        ) const;
    bool showAllCalendarCampuses() const;
    bool hideStartOfTermEvents() const;
    QStringList currentCampusCodes() const;
    QStringList allCampusCodes() const;
    QWidget* createUpcomingEventRow(
        const CalendarEvent& event,
        int dateColumnWidth,
        int timeColumnWidth,
        int eventTypeColumnWidth,
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
    CalendarEventModel* m_calendarModel = nullptr;
    AcademicCalendarProvider* m_academicCalendarProvider = nullptr;
    QQuickWidget* m_calendarView = nullptr;
    QTabWidget* m_upcomingEventsTabs = nullptr;
    std::array<QVBoxLayout*, UpcomingEventsScopeCount> m_upcomingEventLayouts{};
    QList<QPushButton*> m_eventTypeFilterButtons;
    QHash<QString, bool> m_eventTypeFilterStates;
    QDate m_calendarVisibleMonth;
};
