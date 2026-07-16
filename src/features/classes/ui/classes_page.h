#pragma once

#include "domain/models/classroom.h"
#include "ui/shared/pages/basepage.h"

#include <QList>

class ApplicationServices;
class ClassDetailsPage;
class ClassNotesPage;
class QLabel;
class QStackedWidget;
class QTabWidget;
class QVBoxLayout;
class RosterPage;
class QWidget;

enum class ClassesSection
{
    Details,
    Roster,
    Notes
};

class ClassesPage : public BasePage
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

    bool loadClasses();

    int currentClassId() const;
    ClassesSection currentSection() const;

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    QString unsavedChangesTitle() const override;
    QString unsavedChangesMessage() const override;
    void setSaveMode(SaveMode mode) override;
    void refresh() override;
    void retranslateUi() override;

signals:
    void classInfoSaved(int classId);

private:
    void buildUi();
    void rebuildClassTabs(int selectedClassId);
    bool activateClass(int classId);
    bool activateSection(ClassesSection section);
    bool commitActiveEditor();
    void loadEditors(const Classroom& classroom);
    void restoreSelections();
    void syncTabsToClass(int classId);
    int currentClassIdFromTabs(QTabWidget* tabs) const;
    int firstNavigationClassId() const;
    Classroom classroomById(int classId) const;
    BasePage* activeEditor() const;
    void showActiveEditor();
    void updateHeaderText();
    void setEditorAvailable(bool available);
    void handleClassInfoSaved(int classId);

private:
    ApplicationServices* m_services = nullptr;
    QList<Classroom> m_classes;
    int m_currentClassId = -1;
    ClassesSection m_currentSection = ClassesSection::Details;
    bool m_rebuildingTabs = false;
    bool m_restoringTabs = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QWidget* m_navigationContainer = nullptr;
    QWidget* m_classTabsContainer = nullptr;
    QVBoxLayout* m_classTabsLayout = nullptr;
    QTabWidget* m_classTabs = nullptr;
    QTabWidget* m_sectionTabs = nullptr;
    QStackedWidget* m_editorStack = nullptr;
    ClassDetailsPage* m_detailsPage = nullptr;
    RosterPage* m_rosterPage = nullptr;
    ClassNotesPage* m_notesPage = nullptr;
};
