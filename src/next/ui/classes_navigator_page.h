#pragma once

#include "next/application/classes_list_read_port.h"
#include "next/application/classes_navigation_snapshot.h"
#include "next/application/classes_navigator_preferences.h"
#include "next/ui/classes_navigator_model.h"
#include "next/ui/page.h"

#include <QString>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class QLabel;
class QListView;
class QComboBox;
class QTabBar;
class QToolButton;
class QVBoxLayout;

namespace ClassMngr::Next::Ui
{

struct ClassesNavigatorPageActions final
{
    std::function<QString()> workspaceIdentity;
    std::function<std::optional<QString>()> activeWorkspacePath;
    std::function<Application::ClassesListReadResult(const QString&)> readClasses;
    std::function<Application::ClassesNavigationSnapshotResult(
        const QString&,
        const Application::ClassesNavigationSnapshotQuery&)> readNavigation;
    std::function<Application::ClassesNavigatorPreferences(const QString&)>
        readPreferences;
    std::function<Application::ScheduleDisplayModePreferencesSaveResult(
        const QString&,
        Application::ScheduleDisplayMode)> saveScheduleMode;
    std::function<Application::ClassVisibilityPreferencesSaveResult(
        const QString&,
        Application::ClassVisibilityScope)> saveVisibilityScope;
};

class ClassesNavigatorPage final : public Page
{
public:
    explicit ClassesNavigatorPage(
        const PageContext& context,
        ClassesNavigatorPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] ClassesNavigatorModel* model() const noexcept;
    [[nodiscard]] QTabBar* gradeTabs() const noexcept;
    [[nodiscard]] QListView* classList() const noexcept;
    [[nodiscard]] QComboBox* scheduleSourceCombo() const noexcept;
    [[nodiscard]] QComboBox* visibilityScopeCombo() const noexcept;
    [[nodiscard]] const std::optional<Domain::ClassId>& selectedClassId()
        const noexcept;
    [[nodiscard]] bool weekendAvailable() const noexcept;
    [[nodiscard]] bool navigationMetadataAvailable() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;

    [[nodiscard]] bool refreshFromStore();
    void clearForWorkspaceChange();
    void clearForClosedWorkspace();

protected:
    void changeEvent(QEvent* event) override;

private:
    enum class Status
    {
        Ready,
        NoWorkspace,
        Empty,
        ListError,
        NavigationError,
        PreferenceError,
        NoMatches,
        WorkspaceChanged
    };

    void buildUi();
    void retranslateUi();
    void rebuildGroupTabs();
    void rebuildProjection(
        const std::optional<Domain::ClassId>& preferredClass,
        const QString& preferredGroup
        );
    void activateGroup(const QString& groupKey);
    void selectClass(const Domain::ClassId& classId);
    void applyFilter();
    void handleScheduleSourceChanged(int index);
    void handleVisibilityScopeChanged(int index);
    void handleDayFilterChanged(const QString& key, bool enabled);
    void updateStatus();
    void updateControls();
    void rememberSelectedClass();
    void discardSelectionState();
    void discardDayFilterState();
    [[nodiscard]] Application::ClassDayFilterPolicy dayFilterPolicy() const;
    [[nodiscard]] std::optional<QString> activePath() const;
    [[nodiscard]] bool validateListSnapshot(
        const Application::ClassesListSnapshot& snapshot,
        QString* error
        ) const;
    [[nodiscard]] bool validateNavigationSnapshot(
        const Application::ClassesListSnapshot& classes,
        const Application::ClassesNavigationSnapshot& navigation,
        QString* error
        ) const;

    ClassesNavigatorPageActions m_actions;
    ClassesNavigatorModel* m_model = nullptr;
    QTabBar* m_gradeTabs = nullptr;
    QListView* m_classList = nullptr;
    QComboBox* m_scheduleSource = nullptr;
    QComboBox* m_visibilityScope = nullptr;
    QToolButton* m_dayButtons[6]{};
    QLabel* m_statusLabel = nullptr;
    QToolButton* m_retryButton = nullptr;
    QVBoxLayout* m_layout = nullptr;

    Application::ClassesListSnapshot m_classes;
    std::optional<Application::ClassesNavigationSnapshot> m_navigation;
    Application::ClassesNavigatorPreferences m_preferences;
    Application::ClassDayFilterPolicy m_filter{
        {},
        Application::ClassDayScheduleSource::Regular,
        Application::ClassDayVisibilityScope::ActiveSchedule
    };
    std::unordered_map<std::string, Domain::ClassId> m_selectedByGroup;
    std::optional<Domain::ClassId> m_selectedClassId;
    std::optional<QString> m_workspaceIdentity;
    QString m_preferredGroupKey;
    QString m_lastError;
    Status m_status = Status::NoWorkspace;
    bool m_weekendAvailable = false;
    bool m_active = false;
    bool m_rebuilding = false;
    bool m_retranslating = false;
};

} // namespace ClassMngr::Next::Ui
