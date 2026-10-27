#pragma once

#include "next/application/save_mode_preferences.h"
#include "next/application/teacher_profile_edit_use_case.h"
#include "next/application/teacher_profile_read_port.h"
#include "next/ui/page.h"

#include <QIcon>
#include <QHash>

#include <functional>
#include <optional>

class QCalendarWidget;
class QComboBox;
class QEvent;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QVBoxLayout;
class QWidget;
class OnScreenKeyboard;

namespace ClassMngr::Next::Ui
{

class TeacherBirthdayDateEdit;

struct TeacherProfilePageActions final
{
    std::function<Application::TeacherProfileReadResult(
        const Domain::TeacherId&)> read;
    std::function<Application::TeacherProfileValidationResult(
        const Domain::TeacherProfile&)> validate;
    std::function<Application::TeacherProfileEditResult(
        const Application::TeacherProfileEditRequest&)> edit;
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
};

// One static page holds at most one selected teacher profile. The selected
// identity stays in this feature page and is supplied by the typed bootstrap
// open-by-ID seam.
class TeacherProfilePage final : public Page
{
public:
    explicit TeacherProfilePage(
        const PageContext& context,
        TeacherProfilePageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] bool displayProfile(
        const Application::TeacherProfileReadSnapshot& profile
        );
    [[nodiscard]] bool refreshFromStore(QString* error = nullptr);
    [[nodiscard]] std::optional<Domain::TeacherId> selectedTeacherId() const;

    [[nodiscard]] bool hasUnsavedChanges() const noexcept override;
    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;
    void onSuspending() noexcept override;

    void setSaveMode(Application::SaveMode mode);
    void retranslateUi();

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void updateKeyboardTriggerTheme();
    void addLineField(
        QGridLayout& layout,
        const QString& field,
        const QString& label,
        const QString& objectName,
        int labelRow,
        int column
        );
    void addComboField(
        QGridLayout& layout,
        const QString& field,
        const QString& label,
        const QStringList& values,
        bool editable,
        int labelRow,
        int column
        );
    void addBirthdayField(QGridLayout& layout, int labelRow, int column);
    void addNotesField(QVBoxLayout& layout);
    [[nodiscard]] QWidget* fieldContainer(
        const QString& field,
        QWidget* input
        );
    QLabel* createValidationLabel(const QString& field);
    void updatePreferredNameChoices();
    void handleFieldChanged();
    void updateValidation();
    void showValidation(
        const std::vector<Application::TeacherProfileValidationIssue>& issues
        );
    [[nodiscard]] QString validationMessage(
        const Application::TeacherProfileValidationIssue& issue
        ) const;
    [[nodiscard]] Domain::TeacherProfileFields fieldsFromForm() const;
    void populateForm(const Domain::TeacherProfileFields& fields);
    void updatePageState();
    [[nodiscard]] bool fieldsDifferFromSaved() const;
    [[nodiscard]] PageOperationResult<void> saveProfile(bool interactive);
    void setPersistenceStatus(QString text, bool error, bool reloadRequired = false);
    void clearPersistenceStatus();

    TeacherProfilePageActions m_actions;
    std::optional<Domain::TeacherId> m_teacherId;
    Domain::TeacherProfileFields m_savedFields;
    Application::SaveMode m_saveMode = Application::SaveMode::Automatic;
    bool m_loading = false;
    bool m_dirty = false;
    bool m_reloadRequired = false;
    bool m_hasBlockingErrors = false;
    bool m_validationStatusActive = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_noSelectionLabel = nullptr;
    QWidget* m_pageHeader = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QWidget* m_formContainer = nullptr;
    QWidget* m_detailsGroup = nullptr;
    QWidget* m_connectivityGroup = nullptr;
    QWidget* m_notesGroup = nullptr;
    QLabel* m_statusLabel = nullptr;
    TeacherBirthdayDateEdit* m_birthdayEdit = nullptr;
    QPushButton* m_keyboardButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
    QTimer* m_saveTimer = nullptr;
    QHash<QString, QWidget*> m_fields;
    QHash<QString, QLabel*> m_validationLabels;
    QHash<QString, QLabel*> m_fieldLabels;
};

} // namespace ClassMngr::Next::Ui
