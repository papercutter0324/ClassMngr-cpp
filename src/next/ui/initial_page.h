#pragma once

#include "next/application/personal_details_save.h"
#include "next/domain/personal_signature_image_save_bytes.h"
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
    Application::PersonalDetailsSaveRequest m_personalDetails;
    QByteArray m_imageSignature;
    bool m_setupWasPending = false;
    bool m_personalDetailsLoaded = false;
    bool m_personalDetailsEdited = false;
    bool m_updatingPersonalDetails = false;
};

}  // namespace ClassMngr::Next::Ui
