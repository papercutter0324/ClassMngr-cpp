#pragma once

#include "ui/pages/basepage.h"
#include "models/teacher.h"

class QScrollArea;
class QLabel;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QPushButton;
class QTimer;

class ApplicationServices;

class TeacherInfoPage : public BasePage
{
    Q_OBJECT

public:

    explicit TeacherInfoPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadTeacher(
        const Teacher& teacher
        );

    void saveData() override;

    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    QString unsavedChangesTitle() const override;
    QString unsavedChangesMessage() const override;
    void setSaveMode(
        SaveMode mode
        ) override;

    void refresh() override;

    Teacher teacher() const;

signals:

    void teacherSaved(
        int teacherId
        );

private slots:

    void saveTeacher();

    void handleFieldChanged();

    void autosaveTeacher();

private:

    void buildUi();

    QLabel* createFieldLabel(
        const QString& text
        );

    Teacher teacherFromForm() const;

    bool formDiffersFromTeacher() const;

    bool saveTeacherInternal();

    void clearDirty();

    void updateActions();

private:

    // =====================================================
    // Services
    // =====================================================

    ApplicationServices* m_services = nullptr;



    // =====================================================
    // State
    // =====================================================

    Teacher m_teacher;
    bool m_loading = false;
    bool m_dirty = false;
    SaveMode m_saveMode = SaveMode::Automatic;



    // =====================================================
    // Widgets
    // =====================================================

    QScrollArea* m_scroll = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    QLineEdit* m_teacherKrEdit = nullptr;
    QLineEdit* m_teacherEnEdit = nullptr;
    QLineEdit* m_roomNumberEdit = nullptr;

    QLineEdit* m_wifiNameEdit = nullptr;
    QLineEdit* m_wifiPasswordEdit = nullptr;
    QComboBox* m_internetTypeCombo = nullptr;

    QLineEdit* m_zoomIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QComboBox* m_projectionTypeCombo = nullptr;

    QTextEdit* m_notesEdit = nullptr;

    QPushButton* m_saveButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};
