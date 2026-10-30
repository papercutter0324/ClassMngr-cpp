#pragma once

#include "next/application/schedule_page_read.h"
#include "next/application/sub_prep_calendar_event_intervals_query.h"
#include "next/application/sub_prep_class_details_query.h"
#include "next/application/sub_prep_class_information_state.h"
#include "next/application/sub_prep_preferences.h"
#include "next/application/sub_prep_personal_zoom_preferences.h"
#include "next/application/sub_prep_print_source_query.h"
#include "next/application/sub_prep_roster_output_source_query.h"
#include "next/application/sub_prep_schedule_summary_query.h"
#include "next/application/sub_prep_weekday.h"
#include "next/application/current_campus_preferences.h"
#include "next/application/personal_display_name_preferences.h"
#include "next/application/sub_prep_campus_directory_query_port.h"
#include "next/ui/page.h"

#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QTabBar;
class QGroupBox;
class QEvent;

class SubPrepClassInformationListModel;

namespace ClassMngr::Next::Ui
{

struct SubPrepPageActions final
{
    std::function<QString()> workspaceIdentity;
    std::function<Application::SchedulePageReadResult()> readSchedulePage;
    std::function<Application::SubPrepScheduleSummaryQueryResult(
        const Application::SubPrepScheduleScopeRequest&)> readSummaries;
    std::function<Application::SubPrepClassDetailsQueryResult(
        const Domain::ClassId&)> readSelectedDetails;
    std::function<Application::SubPrepPreferencesResult()> readPreferences;
    std::function<Application::SubPrepPreferencesSaveResult(
        const Application::SubPrepPreferences&)> savePreferences;
    std::function<Application::SubPrepPersonalZoomPreferencesResult()>
        readPersonalZoomPreferences;
    std::function<Domain::Result<Application::CurrentCampusPreferenceValue>()>
        readCurrentCampus;
    std::function<std::vector<Application::SubPrepCampusMetadata>()>
        readCampuses;
    std::function<Application::PersonalDisplayNamePreferencesReadResult()>
        readFolderUserName;
    std::function<Application::PersonalDisplayNamePreferencesSaveResult(
        const std::string&)> saveFolderUserName;
    std::function<Application::SubPrepCalendarEventIntervalsQueryResult(
        const Application::SubPrepCalendarEventIntervalsRequest&)>
        readCalendarIntervals;
    std::function<Application::SubPrepPrintSourceQueryResult(
        const Application::SubPrepPrintSourceRequest&)> readPrintSource;
    std::function<std::unique_ptr<
        Application::SubPrepRosterOutputSourceReadPort>()>
        createRosterOutputSourceReadPort;
};

struct SubPrepPageVisualMetrics final
{
    int summaryModelCount = 0;
    int summaryRowCount = 0;
    int selectedDetailPanelCount = 0;
    int textEditorCount = 0;
    int summaryReadCount = 0;
    int selectedDetailReadCount = 0;
    int scheduleReadCount = 0;
    int visibleSummaryCount = 0;
    bool selectedDetailsRetained = false;
};

// Next-shell Sub Prep route. Navigation owns only one compact summary model
// and one reusable selected-class detail panel.
class SubPrepPage final : public Page
{
public:
    explicit SubPrepPage(
        const PageContext& context,
        SubPrepPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;
    [[nodiscard]] bool reload(QString* error = nullptr);
    [[nodiscard]] bool selectClassForTesting(const Domain::ClassId& classId);
    [[nodiscard]] SubPrepClassInformationListModel* summaryModel() const noexcept;
    [[nodiscard]] QListView* classList() const noexcept;
    [[nodiscard]] QTabBar* gradeTabs() const noexcept;
    [[nodiscard]] const std::optional<Domain::ClassId>& selectedClassId()
        const noexcept;
    [[nodiscard]] const std::optional<Application::SubPrepClassDetails>&
        selectedDetails() const noexcept;
    [[nodiscard]] SubPrepPageVisualMetrics visualMetrics() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    void clearPageData();
    void clearSelectedDetail();
    void setStatus(const QString& message, bool error = false);
    void handleGradeChanged(int index);
    void handleSelectionChanged();
    void loadSelectedDetails(const Domain::ClassId& classId);
    [[nodiscard]] std::vector<Application::SubPrepWeekday>
        selectedWeekdays() const;
    void refreshSummaryScope();
    void refreshSummaryScopeWithSchedule(
        Application::SchedulePageReadSnapshot schedule
        );
    void renderSelectedDetails(
        const Application::SubPrepClassDetails& details
        );
    void savePreferences();
    void generateSubPrep();

    SubPrepPageActions m_actions;
    SubPrepClassInformationListModel* m_model = nullptr;
    QComboBox* m_mode = nullptr;
    QCheckBox* m_dayChecks[7]{};
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_savePreferencesButton = nullptr;
    QTabBar* m_gradeTabs = nullptr;
    QListView* m_classList = nullptr;
    QGroupBox* m_detailPanel = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_classTitle = nullptr;
    QLabel* m_teacherName = nullptr;
    QLabel* m_studentCount = nullptr;
    QLabel* m_schedule = nullptr;
    QLabel* m_room = nullptr;
    QLabel* m_wifiName = nullptr;
    QLabel* m_wifiPassword = nullptr;
    QLabel* m_internet = nullptr;
    QLabel* m_zoomId = nullptr;
    QLabel* m_zoomPassword = nullptr;
    QLabel* m_projection = nullptr;
    QPlainTextEdit* m_classNotes = nullptr;
    QPlainTextEdit* m_teacherNotes = nullptr;
    QPlainTextEdit* m_classMaterials = nullptr;
    QPlainTextEdit* m_gradingInstructions = nullptr;
    QPlainTextEdit* m_specialInstructions = nullptr;
    QPlainTextEdit* m_subComments = nullptr;
    std::optional<QString> m_workspaceIdentity;
    std::optional<Domain::ClassId> m_selectedClassId;
    std::optional<Application::SubPrepClassDetails> m_selectedDetails;
    Application::SubPrepPreferences m_preferences;
    Application::SubPrepClassInformationState m_informationState;
    int m_summaryReadCount = 0;
    int m_selectedDetailReadCount = 0;
    int m_scheduleReadCount = 0;
    bool m_loading = false;
    bool m_active = false;
    bool m_modeInitialized = false;
};

} // namespace ClassMngr::Next::Ui
