#pragma once

#include "ui/shared/pages/basepage.h"
#include "domain/models/teacher.h"

class AutosaveCoordinator;
class FormValidationBinder;
class PageHeader;
class ScrollablePageBody;
class QShowEvent;
class QLabel;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QPushButton;
class TeacherSectionCard;
class OnScreenKeyboard;

class ApplicationServices;

namespace ClassMngr::Next::Application
{
class TeacherProfileReadPort;
}

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

    // Retain identity separately from the legacy int-based Teacher fields.
    void loadTeacher(const Teacher& fields, qint64 teacherId);
    [[nodiscard]] qint64 teacherId() const { return m_teacherId; }

    // Reload the current profile through the typed read contract. On failure,
    // the visible profile is retained and the page requires a fresh load.
    [[nodiscard]] bool reloadTeacherProfile(
        const ClassMngr::Next::Application::TeacherProfileReadPort& readPort
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

    // Active Sidebar and navigation consumers use this full identity.
    void teacherIdentitySaved(qint64 teacherId);

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

    void updateFormValidation();
    bool saveTeacherInternal(bool showErrors = true);

    void clearDirty();
    void setReloadRequired(bool required);
    void showSavedButReloadFailedStatus();
    void clearSavedButReloadFailedStatus();

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
    qint64 m_teacherId = -1;
    bool m_reloadRequired = false;
    AutosaveCoordinator* m_autosave = nullptr;
    FormValidationBinder* m_validationBinder = nullptr;



    // =====================================================
    // Widgets
    // =====================================================

    ScrollablePageBody* m_pageBody = nullptr;
    PageHeader* m_pageHeader = nullptr;
    QPushButton* m_koreanKeyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;

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
    QLabel* m_reloadFailureStatusLabel = nullptr;
    bool m_savedButReloadFailedStatusActive = false;
};
