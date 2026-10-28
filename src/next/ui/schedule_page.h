#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_page_read.h"
#include "next/application/schedule_slot_state_save.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_state_snapshot.h"
#include "next/ui/page.h"
#include "next/ui/schedule_page_model.h"

#include <QString>
#include <QPointer>

#include <functional>
#include <optional>

class QCheckBox;
class QAction;
class QButtonGroup;
class QLabel;
class QMenu;
class QPushButton;
class QTableView;
class QToolButton;
class QWidget;
class QIODevice;

namespace ClassMngr::Next::Ui
{

struct SchedulePageActions final
{
    std::function<QString()> activeWorkspacePath;
    std::function<QString()> workspaceIdentity;
    std::function<Application::SchedulePageReadResult(const QString&)> readSchedule;
    std::function<Application::ScheduleDisplayPreferencesSaveResult(
        const QString&,
        const Application::ScheduleDisplayPreferencesSaveRequest&)>
        saveDisplayPreferences;
    std::function<Application::ScheduleDisplayModePreferencesSaveResult(
        const QString&,
        Application::ScheduleDisplayMode)>
        saveDisplayMode;
    std::function<Application::ScheduleSlotStateSaveResult(
        const QString&,
        const Application::ScheduleSlotStateSaveRequest&)>
        saveSlotState;
    std::function<QString(QWidget*)> chooseScheduleImportWorkbook;
    std::function<Result<ScheduleImportWorkbook>(QIODevice&, ScheduleImportKind)>
        parseScheduleImportWorkbook;
    std::function<int()> scheduleImportTimeoutMilliseconds;
    std::function<Application::ScheduleImportStateSnapshotOutcome(
        const QString&)> readScheduleImportState;
    std::function<Application::ScheduleImportApplyResult(
        const QString&,
        const Application::ScheduleImportApplyRequest&)> applyScheduleImport;
    std::function<void(const Domain::ClassId&)> openClassDetails;
};

struct SchedulePageVisualMetrics final
{
    SchedulePageRuntimeMetrics model;
    int tableViewCount = 0;
    int tableCellWidgetCount = 0;
    int descendantWidgetCount = 0;
};

class SchedulePage final : public Page
{
public:
    explicit SchedulePage(
        const PageContext& context,
        SchedulePageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;
    void clearForWorkspaceChange();
    void clearForClosedWorkspace();
    [[nodiscard]] bool reload();
    [[nodiscard]] SchedulePageModel* model() const noexcept;
    [[nodiscard]] QTableView* scheduleTable() const noexcept;
    [[nodiscard]] QPushButton* modeButton(
        Application::ScheduleDisplayMode mode
        ) const noexcept;
    [[nodiscard]] QCheckBox* preferenceCheckBox(int index) const noexcept;
    [[nodiscard]] QPushButton* retryButton() const noexcept;
    [[nodiscard]] QLabel* statusLabel() const noexcept;
    [[nodiscard]] class ScheduleImportWorkflowDialog* importDialog() const noexcept;
    [[nodiscard]] SchedulePageVisualMetrics visualMetrics() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    void clearRetainedState(bool clearIdentity);
    void setStatus(const QString& message, bool error = false);
    void setControlsEnabled(bool enabled);
    void selectMode(Application::ScheduleDisplayMode mode);
    void handleModeChanged(Application::ScheduleDisplayMode mode);
    void handlePreferenceChanged();
    void handleCellClicked(int row, int column);
    [[nodiscard]] QString activePath() const;
    [[nodiscard]] QString workspaceIdentity() const;
    [[nodiscard]] Application::ScheduleDisplayPreferences
    preferencesFromControls() const;
    void restoreControlsFromSnapshot();
    void updateTestingBanner();
    void updateRowHeights();
    void startScheduleImport();
    void closeScheduleImport() noexcept;
    void handleScheduleImportApplied(
        const Application::ScheduleImportApplySummary& summary
        );

    SchedulePageActions m_actions;
    SchedulePageModel* m_model = nullptr;
    QTableView* m_table = nullptr;
    QWidget* m_statusContainer = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_testingBanner = nullptr;
    QPushButton* m_retry = nullptr;
    QToolButton* m_preferencesButton = nullptr;
    QMenu* m_preferencesMenu = nullptr;
    QAction* m_importAction = nullptr;
    QPointer<class ScheduleImportWorkflowDialog> m_importDialog;
    QPushButton* m_modeButtons[3]{};
    QCheckBox* m_preferenceCheckBoxes[5]{};
    QButtonGroup* m_modeGroup = nullptr;
    QString m_workspaceIdentity;
    QString m_lastError;
    bool m_pageActive = false;
    bool m_loading = false;
    bool m_synchronizingControls = false;
};

} // namespace ClassMngr::Next::Ui
