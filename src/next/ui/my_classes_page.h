#pragma once

#include "next/application/my_classes_page_read.h"
#include "next/ui/my_classes_summary_model.h"
#include "next/ui/page.h"

#include <optional>
#include <functional>
#include <QString>

class QLabel;
class QListView;
class QPlainTextEdit;
class QTabBar;
class QGridLayout;
class QEvent;

namespace ClassMngr::Next::Ui
{

struct MyClassesPageActions final
{
    std::function<Application::MyClassesPageSummaryResult()> readSummaries;
    std::function<Application::MyClassesPageSelectedDetailsResult(
        const Domain::ClassId&)> readSelectedDetails;
    std::function<QString()> workspaceIdentity;
};

// One compact model backs class navigation. The page creates one reusable
// selected-detail panel and queries its data by stable class ID.
class MyClassesPage final : public Page
{
public:
    explicit MyClassesPage(
        const PageContext& context,
        MyClassesPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] MyClassesSummaryModel* summaryModel() const noexcept;
    [[nodiscard]] QListView* classList() const noexcept;
    [[nodiscard]] QTabBar* gradeTabs() const noexcept;
    [[nodiscard]] const std::optional<Domain::ClassId>& selectedClassId()
        const noexcept;
    [[nodiscard]] const std::optional<Application::MyClassesPageSelectedDetails>&
    selectedDetails() const noexcept;
    [[nodiscard]] bool detailsReleased() const noexcept;

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;

    [[nodiscard]] bool refreshFromStore(QString* error = nullptr);
    void clearForClosedWorkspace();
    void clearForWorkspaceChange();

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    void rebuildGradeTabs(
        const std::optional<Domain::ClassId>& preferredClass
        );
    void rebuildClassList(
        const std::optional<Domain::ClassId>& preferredClass
        );
    void handleGradeChanged(int index);
    void activateSelectedClass();
    void showSelectedDetails(
        const Application::MyClassesPageSelectedDetails& details
        );
    void clearDetailFields();
    void setDetailError(const QString& error);
    [[nodiscard]] QModelIndex firstClassIndexForGrade(int gradeRow) const;

    MyClassesPageActions m_actions;
    MyClassesSummaryModel* m_model = nullptr;
    QTabBar* m_gradeTabs = nullptr;
    QListView* m_classList = nullptr;
    QWidget* m_detailPanel = nullptr;
    QGridLayout* m_classFields = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_loadState = nullptr;
    QLabel* m_classTitle = nullptr;
    QLabel* m_detailError = nullptr;
    QLabel* m_teacherTitle = nullptr;
    QLabel* m_studentCount = nullptr;
    QLabel* m_regularScheduleLabel = nullptr;
    QLabel* m_regularSchedule = nullptr;
    QLabel* m_intensiveScheduleLabel = nullptr;
    QLabel* m_intensiveSchedule = nullptr;
    QLabel* m_emptyScheduleLabel = nullptr;
    QLabel* m_emptySchedule = nullptr;
    QLabel* m_internetType = nullptr;
    QLabel* m_wifiName = nullptr;
    QLabel* m_wifiPassword = nullptr;
    QLabel* m_projectionType = nullptr;
    QLabel* m_zoomId = nullptr;
    QLabel* m_zoomPassword = nullptr;
    QPlainTextEdit* m_teacherNotes = nullptr;
    QPlainTextEdit* m_classNotes = nullptr;
    QPlainTextEdit* m_timeFiller = nullptr;
    std::optional<QString> m_workspaceIdentity;
    std::optional<Domain::ClassId> m_selectedClassId;
    std::optional<Application::MyClassesPageSelectedDetails> m_details;
    bool m_loading = false;
    bool m_retranslating = false;
    bool m_active = false;
};

} // namespace ClassMngr::Next::Ui
