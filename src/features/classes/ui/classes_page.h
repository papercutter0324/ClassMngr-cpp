#pragma once

#include "core/memory_usage_diagnostics.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/classroom.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/pages/basepage.h"

#include <QHash>
#include <QList>

class ApplicationServices;
class ClassCoTeacherPage;
class ClassDetailsPage;
class ClassNotesPage;
class ClassAnalyticsPage;
class SpeakingEvalPage;
class QLabel;
class QHideEvent;
class NavigationPillButton;
class NavigationTabWidget;
class QPushButton;
class QResizeEvent;
class QStackedWidget;
class QVBoxLayout;
class RosterEditorWidget;
class OnScreenKeyboard;
class QWidget;

enum class ClassesSection
{
    Details,
    Roster,
    Analytics,
    Evaluations,
    CoTeacher,
    Notes
};

class ClassesPage : public BasePage, public MemoryBreakdownProvider
{
    Q_OBJECT

public:
    explicit ClassesPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    bool openClass(
        int classId = -1,
        ClassesSection section = ClassesSection::Details
        );

    bool openEvaluation(
        int classId,
        const QString& evaluationName
        );

    bool loadClasses();

    int currentClassId() const;
    ClassesSection currentSection() const;
    [[nodiscard]] bool isEditorInstantiated(
        ClassesSection section
        ) const;
    [[nodiscard]] QList<MemoryBreakdownEntry>
        memoryBreakdown() const override;

    void setScheduleDisplayMode(
        ScheduleDisplayMode mode
        );
    void setEmbeddedDatabaseOpen(bool databaseOpen);

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    QString unsavedChangesTitle() const override;
    QString unsavedChangesMessage() const override;
    void setSaveMode(SaveMode mode) override;
    void refresh() override;
    void refreshNavigationPreferences();
    void clearDatabaseState() override;
    [[nodiscard]] Status prepareForActivation() override;
    void releaseFeatureResources() override;
    void retranslateUi() override;
    [[nodiscard]] PageOutputCapabilities outputCapabilities() const override;
    void printCurrentPage() override;
    void saveCurrentPageAs() override;

signals:
    void classInfoSaved(int classId);

private:
    void buildUi();
    void rebuildClassTabs(int selectedClassId);
    void rebuildSectionTabs();
    void createDayFilterControls(NavigationTabWidget* gradeTabs);
    void updateFirstRowLayout();
    void scheduleFirstRowLayout();
    void setDayFilterEnabled(
        const QString& key,
        bool enabled
        );
    bool dayFilterEnabled(const QString& key) const;
    bool hasWeekendClasses(
        const QList<ClassTabNavigation::ClassEntry>& entries
        ) const;
    void rememberClassSelection(
        const QString& grade,
        int classId
        );
    int rememberedClassId(const QString& grade) const;
    void discardClassSelectionState();
    void discardDayFilterState();
    void setNavigationSelectionVisible(bool visible);
    void setScheduleSource(
        ClassTabNavigation::ScheduleSource source
        );
    void setVisibilityScope(
        ClassTabNavigation::VisibilityScope visibilityScope
        );
    void setShowMiddleSchoolAnalyticsAndEvaluations(
        bool show
        );
    bool activateClass(int classId);
    bool activateSection(ClassesSection section);
    [[nodiscard]] Status acquireEvaluationResources();
    void releaseEvaluationResources();
    bool commitActiveEditor();
    BasePage* ensureEditor(
        ClassesSection section
        );
    void loadActiveEditor();
    void restoreSelections();
    void syncTabsToClass(int classId);
    int currentClassIdFromTabs(NavigationTabWidget* tabs) const;
    int firstNavigationClassId() const;
    Classroom classroomById(int classId) const;
    BasePage* activeEditor() const;
    void showActiveEditor();
    void updateHeaderText();
    void setEditorAvailable(bool available);
    void handleClassInfoSaved(int classId);
    void applyEditorState(BasePage* editor);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    ApplicationServices* m_services = nullptr;
    QList<Classroom> m_classes;
    int m_currentClassId = -1;
    ClassesSection m_currentSection = ClassesSection::Details;
    SaveMode m_saveMode = SaveMode::Automatic;
    bool m_embeddedDatabaseStateSet = false;
    bool m_embeddedDatabaseOpen = false;
    bool m_rebuildingTabs = false;
    bool m_rebuildingSectionTabs = false;
    bool m_showMiddleSchoolAnalyticsAndEvaluations = false;
    bool m_restoringTabs = false;
    bool m_weekendClassesAvailable = false;
    bool m_firstRowLayoutQueued = false;
    bool m_updatingFirstRowLayout = false;
    QString m_selectedGrade;
    QHash<QString, int> m_selectedClassIds;
    QHash<int, int> m_loadedEditorClassIds;
    QList<ClassesSection> m_visibleSections;
    ClassTabNavigation::DayFilter m_dayFilter{
        {},
        ClassTabNavigation::ScheduleSource::Regular,
        ClassTabNavigation::VisibilityScope::ActiveSchedule
    };

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QPushButton* m_koreanKeyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
    QWidget* m_navigationContainer = nullptr;
    QWidget* m_classTabsContainer = nullptr;
    QVBoxLayout* m_classTabsLayout = nullptr;
    QWidget* m_dayFilterControls = nullptr;
    QHash<QString, NavigationPillButton*> m_dayFilterButtons;
    NavigationTabWidget* m_classTabs = nullptr;
    NavigationTabWidget* m_sectionTabs = nullptr;
    QStackedWidget* m_editorStack = nullptr;
    ClassDetailsPage* m_detailsPage = nullptr;
    RosterEditorWidget* m_rosterEditor = nullptr;
    ClassAnalyticsPage* m_analyticsPage = nullptr;
    SpeakingEvalPage* m_evaluationsPage = nullptr;
    ClassCoTeacherPage* m_coTeacherPage = nullptr;
    ClassNotesPage* m_notesPage = nullptr;
    ResourcePackLease m_evaluationResourceLease;
};
