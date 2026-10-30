#pragma once

#include "features/calendar/ui/calendar_event_cache.h"
#include "next/application/calendar_event_by_id_query_port.h"
#include "next/application/calendar_event_delete_all_port.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/application/calendar_event_edit_draft.h"
#include "next/application/calendar_event_display_preferences.h"
#include "next/application/calendar_event_projection.h"
#include "next/application/calendar_event_save_port.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/calendar_event_series_create_use_case.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/application/calendar_event_series_edit_port.h"
#include "next/application/calendar_event_type_color_preferences.h"
#include "next/application/calendar_page_campus_directory_query_port.h"
#include "next/application/current_campus_preferences.h"
#include "next/ui/page.h"

#include <QDate>
#include <QStringList>

#include <array>
#include <functional>
#include <vector>

class QComboBox;
class QEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QNetworkAccessManager;
class QObject;
class QPushButton;
class QQuickWidget;
class QWidget;
class QVBoxLayout;
class QShowEvent;
class CalendarEventModel;
class AcademicCalendarProvider;
class WorkspaceStore;

namespace ClassMngr::Next::Ui
{

class NextCalendarEventImportService;

struct CalendarPageActions final
{
    std::function<QString()> activeWorkspacePath;
    std::function<Application::CalendarEventByIdQueryResult(
        const QString&, const Domain::CalendarEventId&)> readEvent;
    std::function<Application::CalendarEventSaveResult(
        const QString&, const Application::CalendarEventSaveInput&)> saveEvent;
    std::function<Application::CalendarEventDeleteResult(
        const QString&, const Domain::CalendarEventId&)> deleteEvent;
    std::function<Application::CalendarEventSeriesCreateResult(
        const QString&, const Application::CalendarEventSeriesCreateInput&)>
        createRepeatSeries;
    std::function<Application::CalendarEventSeriesEditResult(
        const QString&, const Application::CalendarEventSeriesEditInput&)>
        editRepeatSeries;
    std::function<Application::CalendarEventSeriesDeleteResult(
        const QString&, const Application::CalendarEventSeriesDeleteInput&)>
        deleteRepeatSeries;
    std::function<Application::CalendarEventDeleteAllResult(const QString&)>
        deleteAllEvents;
    std::function<Application::CalendarEventDisplayPreferencesResult(
        const QString&)> readDisplayPreferences;
    std::function<Application::CalendarEventDisplayPreferencesSaveResult(
        const QString&,
        const Application::CalendarEventDisplayPreferencesSaveRequest&)>
        saveDisplayPreferences;
    std::function<Domain::Result<Application::CurrentCampusPreferenceValue>(
        const QString&)> readCurrentCampus;
    std::function<std::vector<Application::CalendarPageCampusMetadata>()>
        readCampusDirectory;
    std::function<Domain::Result<std::string>(
        const QString&, Domain::CalendarEventType)> readEventTypeColor;
    std::function<Application::CalendarEventTypeColorPreferencesPort::SaveResult(
        const QString&,
        Domain::CalendarEventType,
        const std::string&)> saveEventTypeColor;
    // Test seams for deterministic transport and staged-file coverage.
    // Production leaves these overrides empty.
    QString calendarImportSourceUrl;
    QString calendarImportTemporaryFileTemplate;
    std::function<QNetworkAccessManager*(QObject*)>
        calendarImportNetworkAccessManagerFactory;
};

struct CalendarPageRuntimeMetrics final
{
    int cacheEventCount = 0;
    int cacheDateBucketCount = 0;
    int cacheLoadedRangeCount = 0;
    int cacheRetainedRangeCount = 0;
    int visibleGridDayCount = 0;
    int upcomingRangeDayCount = 0;
    int upcomingResultCount = 0;
    int upcomingResultLimit = 10;
    int calendarViewCount = 0;
    int calendarRootObjectCount = 0;
    int calendarViewConstructionCount = 0;
    int liveHeavySessionCount = 0;
    int peakLiveHeavySessionCount = 0;
    bool cacheLoading = false;
};

class CalendarPage final : public Page
{
    Q_OBJECT

public:
    explicit CalendarPage(
        const PageContext& context,
        CalendarPageActions actions = {},
        bool embedded = false,
        QWidget* parent = nullptr
        );
    ~CalendarPage() override;

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;
    void refreshForWorkspaceIdentity();
    void clearForClosedWorkspace();
    [[nodiscard]] CalendarPageRuntimeMetrics runtimeMetrics() const;
    [[nodiscard]] CalendarEventModel* eventModel() const noexcept;
    [[nodiscard]] CalendarEventCache* eventCache() const noexcept;
    [[nodiscard]] QQuickWidget* calendarView() const noexcept;
    [[nodiscard]] QListWidget* upcomingList() const noexcept;

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void handleCalendarDayActivated(int year, int month, int day);
    void handleCalendarEventActivated(const QString& eventId);
    void handleCalendarDisplayedMonthChanged(int year, int month);
    void handleUpcomingItemActivated(QListWidgetItem* item);

private:
    enum class UpcomingScope
    {
        CurrentMonth,
        Next30Days,
        Next10Events
    };

    void buildShell();
    void ensureCalendarContent();
    void activateVisibleContent();
    void releaseCalendarContent() noexcept;
    void reloadWorkspaceState();
    void updateRetentionAndQueries();
    void updateCampusFilter();
    void updateEventTypeFilter();
    void refreshUpcoming();
    void renderCalendarColors();
    void renderEventTypeFilterStyles();
    void applyCalendarPalette();
    void openCalendarPreferences();
    void openEventDialog(
        Application::CalendarEventEditDraft draft,
        bool existingEvent
        );
    void invalidateAfterMutation();
    [[nodiscard]] CalendarEventCache::DateRange visibleGridRange() const;
    [[nodiscard]] CalendarEventCache::DateRange upcomingRange() const;
    [[nodiscard]] QString activeWorkspacePath() const;
    [[nodiscard]] QStringList calendarImportCampusCodes() const;
    [[nodiscard]] Application::CalendarEventEditDraft draftFromSummary(
        const Application::CalendarEventSummary& summary
        ) const;

    CalendarPageActions m_actions;
    QVBoxLayout* m_rootLayout = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget* m_calendarCard = nullptr;
    QQuickWidget* m_calendarView = nullptr;
    QListWidget* m_upcomingList = nullptr;
    QComboBox* m_upcomingScope = nullptr;
    QLabel* m_status = nullptr;
    std::array<QPushButton*, 6> m_eventTypeFilters{};
    std::array<std::string, 6> m_eventColors{};
    CalendarEventCache* m_cache = nullptr;
    CalendarEventModel* m_model = nullptr;
    WorkspaceStore* m_workspaceStore = nullptr;
    NextCalendarEventImportService* m_calendarImportService = nullptr;
    AcademicCalendarProvider* m_academicProvider = nullptr;
    QString m_workspacePath;
    QStringList m_currentCampusCodes;
    QStringList m_allCampusCodes;
    Application::CalendarEventDisplayPreferences m_displayPreferences;
    QDate m_visibleMonth;
    int m_upcomingResultCount = 0;
    int m_calendarViewConstructionCount = 0;
    int m_upcomingScopeSelection = 0;
    std::array<bool, 6> m_eventTypeFilterEnabled{true, true, true, true, true, true};
    bool m_embedded = false;
    bool m_pageActive = false;
    bool m_contentActive = false;
};

} // namespace ClassMngr::Next::Ui
