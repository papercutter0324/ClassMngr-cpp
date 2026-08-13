#pragma once

#include "ui/shared/pages/basepage.h"
#include "domain/models/teacher.h"

class AutosaveCoordinator;
class PageHeader;
class ScrollablePageBody;
class QShowEvent;
class QLabel;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QPushButton;
class TeacherSectionCard;

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
    void clearDatabaseState() override;
    void retranslateUi() override;

    Teacher teacher() const;

signals:

    void teacherSaved(
        int teacherId
        );

protected:

    void showEvent(QShowEvent* event) override;

private slots:

    void saveTeacher();

    void handleFieldChanged();

private:

    void buildUi();

    QLabel* createFieldLabel(
        const QString& text
        );

    Teacher teacherFromForm() const;

    bool formDiffersFromTeacher() const;

    bool saveTeacherInternal(bool showErrors = true);

    void clearDirty();

    void updateActions();
    void updateFieldWidths();
    void matchBirthdayWidthToKoreanName();
    void updatePreferredNameChoices(bool promptForSelection);

private:

    // =====================================================
    // Services
    // =====================================================

    ApplicationServices* m_services = nullptr;



    // =====================================================
    // State
    // =====================================================

    Teacher m_teacher;
    AutosaveCoordinator* m_autosave = nullptr;



    // =====================================================
    // Widgets
    // =====================================================

    ScrollablePageBody* m_pageBody = nullptr;
    PageHeader* m_pageHeader = nullptr;

    TeacherSectionCard* m_detailsCard = nullptr;
    TeacherSectionCard* m_connectivityCard = nullptr;
    TeacherSectionCard* m_notesCard = nullptr;

    QLabel* m_teacherKrLabel = nullptr;
    QLabel* m_teacherEnLabel = nullptr;
    QLabel* m_preferredRomanizationLabel = nullptr;
    QLabel* m_preferredNameLabel = nullptr;
    QLabel* m_roomNumberLabel = nullptr;
    QLabel* m_birthdayLabel = nullptr;
    QLabel* m_phoneNumberLabel = nullptr;
    QLabel* m_internetTypeLabel = nullptr;
    QLabel* m_wifiNameLabel = nullptr;
    QLabel* m_wifiPasswordLabel = nullptr;
    QLabel* m_projectionTypeLabel = nullptr;
    QLabel* m_zoomIdLabel = nullptr;
    QLabel* m_zoomPasswordLabel = nullptr;

    QLineEdit* m_teacherKrEdit = nullptr;
    QLineEdit* m_teacherEnEdit = nullptr;
    QLineEdit* m_preferredRomanizationEdit = nullptr;
    QComboBox* m_preferredNameCombo = nullptr;
    QLineEdit* m_roomNumberEdit = nullptr;
    QLineEdit* m_birthdayEdit = nullptr;
    QLineEdit* m_phoneNumberEdit = nullptr;

    QLineEdit* m_wifiNameEdit = nullptr;
    QLineEdit* m_wifiPasswordEdit = nullptr;
    QComboBox* m_internetTypeCombo = nullptr;

    QLineEdit* m_zoomIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QComboBox* m_projectionTypeCombo = nullptr;

    QTextEdit* m_notesEdit = nullptr;

    QPushButton* m_saveButton = nullptr;
};
