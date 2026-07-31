#pragma once

#include "domain/models/testing_class.h"
#include "ui/shared/pages/basepage.h"

#include <QString>

class ApplicationServices;
class ClickableColorPreview;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTextEdit;
class QTimer;
class RosterEditorWidget;

class TestingClassesPage : public BasePage
{
    Q_OBJECT

public:
    explicit TestingClassesPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void openTestingClass(
        int classId = -1,
        const QString& pendingDay = {},
        const QString& pendingStartTime = {}
        );

    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;
    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    QString unsavedChangesTitle() const override;
    QString unsavedChangesMessage() const override;
    void setSaveMode(
        SaveMode mode
        ) override;

signals:
    void returnToScheduleRequested();
    void testingDataChanged();

private:
    void buildUi();
    void populateTeachers();
    void updateLevelOptions();
    void rebuildClassList(
        int preferredClassId = -1
        );
    void loadClass(
        int classId
        );
    void beginNewClass(
        const QString& pendingDay = {},
        const QString& pendingStartTime = {}
        );
    TestingClass editorValue() const;
    void loadEditorValue(
        const TestingClass& testingClass
        );
    void autosave();
    void markDirty();
    void updateActions();
    void updateColorButtons();
    void chooseClassColor();
    void chooseFontColor();
    void deleteCurrentClass();

    ApplicationServices* m_services = nullptr;
    TestingClass m_savedClass;
    int m_currentClassId{-1};
    bool m_loading = false;
    bool m_dirty = false;
    SaveMode m_saveMode = SaveMode::Automatic;
    QString m_pendingDay;
    QString m_pendingStartTime;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_gradeLevelLabel = nullptr;
    QLabel* m_roomLabel = nullptr;
    QLabel* m_teacherLabel = nullptr;
    QLabel* m_classColorLabel = nullptr;
    QLabel* m_fontColorLabel = nullptr;
    QListWidget* m_classList = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTimer* m_autosaveTimer = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_gradeCombo = nullptr;
    QComboBox* m_levelCombo = nullptr;
    QLineEdit* m_roomEdit = nullptr;
    QComboBox* m_teacherCombo = nullptr;
    ClickableColorPreview* m_classColorPreview = nullptr;
    ClickableColorPreview* m_fontColorPreview = nullptr;
    QPushButton* m_classColorButton = nullptr;
    QPushButton* m_fontColorButton = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    RosterEditorWidget* m_rosterEditor = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_saveButton = nullptr;
};
