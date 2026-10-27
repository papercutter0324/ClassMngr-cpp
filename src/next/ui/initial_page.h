#pragma once

#include "next/application/personal_details_save.h"
#include "next/application/initial_setup_teacher_choices_read_snapshot.h"
#include "next/application/initial_setup_class_details_draft.h"
#include "next/application/custom_color_palette_preferences.h"
#include "next/application/teacher_create.h"
#include "next/domain/personal_signature_image_save_bytes.h"
#include "next/ui/initial_setup_teacher_import_dialog.h"
#include "next/ui/page.h"

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QGroupBox;

namespace ClassMngr::Next::Ui
{

struct InitialSetupPageState final
{
    bool setupPending = false;
    bool recoveryAvailable = false;
    bool recoveryError = false;
    bool recoveryHasOriginalProfile = false;
    QString profileLocation;
    QString recoveryProfileLocation;
    QString statusMessage;
};

struct InitialSetupPageActions final
{
    std::function<InitialSetupPageState()> state;
    std::function<void()> createProfile;
    std::function<void()> resumeSetup;
    std::function<void()> restoreSetup;
    std::function<void()> cancelSetup;
    std::function<Application::PersonalDetailsReadResult()>
        loadPersonalDetails;
    std::function<Application::PersonalDetailsSaveResult(
        const Application::PersonalDetailsSaveRequest&
        )> savePersonalDetails;
    std::function<Application::InitialSetupTeacherChoicesResult()>
        loadTeacherChoices;
    std::function<Application::TeacherCreateResult(
        const Application::TeacherCreateRequest&
        )> createTeacher;
    std::function<Domain::Result<Application::CustomColorPalette>()>
        readClassColorPalette;
    std::function<Domain::Result<void>(
        const Application::CustomColorPalette&
        )> saveClassColorPalette;
    std::function<Application::InitialSetupClassDetailsDraftCaptureResult(
        const Application::InitialSetupClassDetailsDraft&
        )> captureClassDetailsDraft;
    InitialSetupTeacherImportActions teacherImport;
    std::function<Domain::Result<std::optional<QByteArray>>()> chooseSignatureImage;
};

class InitialPage final : public Page
{
    Q_OBJECT

public:
    explicit InitialPage(
        const PageContext& context,
        InitialSetupPageActions actions = {},
        QWidget* parent = nullptr
        );

    void refreshSetupState();
    void refreshPersonalDetails();

private:
    void loadPersonalDetails();
    void savePersonalDetails();
    void saveTeacher(bool addAnother);
    void importTeachersFromWorkbook();
    void clearTeacherFields();
    void refreshClassDetailsTeacherChoices();
    void populateClassDetailsTeacherChoices(
        const Application::InitialSetupTeacherChoicesResult& choices
        );
    void updateClassDetailsLevels();
    void updateClassDetailsBooks();
    void updateClassColorPreview();
    void chooseClassColor();
    void continueClassDetails();
    void clearClassDetails();
    void updatePreferredTeacherNames();
    void updateTeacherEntryControls();
    void syncPersonalDetailsFromForm();
    void updatePersonalDetailsControls();
    void updateSignaturePreview();
    void setPersonalDetailsStatus(const QString& message);

    InitialSetupPageActions m_actions;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_profileLabel = nullptr;
    QLabel* m_recoveryLabel = nullptr;
    QPushButton* m_createButton = nullptr;
    QPushButton* m_resumeButton = nullptr;
    QPushButton* m_restoreButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QGroupBox* m_personalDetailsGroup = nullptr;
    QLineEdit* m_personalNameEdit = nullptr;
    QRadioButton* m_imageSignatureModeButton = nullptr;
    QRadioButton* m_typedSignatureModeButton = nullptr;
    QLabel* m_signaturePreviewLabel = nullptr;
    QPushButton* m_chooseSignatureButton = nullptr;
    QPushButton* m_removeSignatureButton = nullptr;
    QWidget* m_typedSignatureControls = nullptr;
    QLineEdit* m_typedSignatureEdit = nullptr;
    QComboBox* m_signatureFontCombo = nullptr;
    QLabel* m_personalDetailsStatusLabel = nullptr;
    QPushButton* m_retryPersonalDetailsButton = nullptr;
    QPushButton* m_savePersonalDetailsButton = nullptr;
    QGroupBox* m_teacherEntryGroup = nullptr;
    QLineEdit* m_teacherEnglishNameEdit = nullptr;
    QLineEdit* m_teacherKoreanNameEdit = nullptr;
    QLineEdit* m_teacherPreferredSpellingEdit = nullptr;
    QLineEdit* m_teacherRoomEdit = nullptr;
    QComboBox* m_teacherPreferredNameCombo = nullptr;
    QComboBox* m_teacherInternetTypeCombo = nullptr;
    QLineEdit* m_teacherWifiNameEdit = nullptr;
    QLineEdit* m_teacherWifiPasswordEdit = nullptr;
    QComboBox* m_teacherProjectionTypeCombo = nullptr;
    QLineEdit* m_teacherZoomIdEdit = nullptr;
    QLineEdit* m_teacherZoomPasswordEdit = nullptr;
    QLabel* m_teacherEntryStatusLabel = nullptr;
    QPushButton* m_addAnotherTeacherButton = nullptr;
    QPushButton* m_continueTeacherEntryButton = nullptr;
    QPushButton* m_importTeachersButton = nullptr;
    QGroupBox* m_classDetailsGroup = nullptr;
    QComboBox* m_classDetailsTeacherCombo = nullptr;
    QPushButton* m_retryClassDetailsTeachersButton = nullptr;
    QLabel* m_classDetailsStatusLabel = nullptr;
    QLabel* m_classDetailsValidationLabel = nullptr;
    QLabel* m_classColorPreview = nullptr;
    QPushButton* m_chooseClassColorButton = nullptr;
    QComboBox* m_classGradeCombo = nullptr;
    QComboBox* m_classLevelCombo = nullptr;
    QComboBox* m_readingBookCombo = nullptr;
    QComboBox* m_essayBookCombo = nullptr;
    QPushButton* m_continueClassDetailsButton = nullptr;
    Application::PersonalDetailsSaveRequest m_personalDetails;
    QByteArray m_imageSignature;
    bool m_setupWasPending = false;
    bool m_personalDetailsLoaded = false;
    bool m_personalDetailsEdited = false;
    bool m_updatingPersonalDetails = false;
    bool m_teacherEntryComplete = false;
    bool m_classColorChosen = false;
    QString m_classColor = QStringLiteral("#FFFFFF");
};

}  // namespace ClassMngr::Next::Ui
