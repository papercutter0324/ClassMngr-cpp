#pragma once

#include "next/application/personal_details_save.h"
#include "next/application/my_info_campus_directory_query_port.h"
#include "next/application/save_mode_preferences.h"
#include "next/ui/page.h"

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QEvent;
class QTimer;
class QWidget;

namespace ClassMngr::Next::Ui
{

struct MyWorkspacePageActions final
{
    std::function<Application::PersonalDetailsReadResult()> readDetails;
    std::function<Application::PersonalDetailsSaveResult(
        const Application::PersonalDetailsSaveRequest&)>
        saveDetails;
    std::function<std::vector<Application::MyInfoCampusMetadata>(
        const PageContext&)>
        loadCampuses;
    std::function<Domain::Result<std::optional<QByteArray>>()> chooseSignatureImage;
    std::function<QString()> workspaceIdentity;
    std::function<void(const QString&, const QString&)> reportWarning;
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
};

// The legacy My Workspace route and tabs remain together; this page owns the
// normal-workspace My Details draft and its complete typed compatibility
// bundle. Schedule and Calendar stay visible as deferred tabs.
class MyWorkspacePage final : public Page
{
public:
    explicit MyWorkspacePage(
        const PageContext& context,
        MyWorkspacePageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    [[nodiscard]] bool hasUnsavedChanges() const noexcept override;
    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;
    void onSuspending() noexcept override;

    void refreshForWorkspaceIdentity();
    void clearForClosedWorkspace();
    void reloadPersonalDetails();
    void setSaveMode(Application::SaveMode mode);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    [[nodiscard]] bool loadPersonalDetails(bool resetForNewIdentity);
    void applyDetailsToControls(
        const Application::PersonalDetailsSaveRequest& details
        );
    void syncDetailsFromControls();
    void markDirty();
    [[nodiscard]] PageOperationResult<void> saveDetails(bool interactive);
    [[nodiscard]] PageOperationResult<void> reloadForDiscard();
    void updateSignatureControls();
    void updateSignaturePreview();
    void updateTypedSignatureImage();
    void updateZoomControls();
    void updateSaveStatus(const QString& message, bool error = false);
    void clearForNewWorkspace();
    void reportWarning(const QString& title, const QString& message);

    MyWorkspacePageActions m_actions;
    Application::PersonalDetailsSaveRequest m_details;
    QString m_workspaceIdentity;
    Application::SaveMode m_saveMode = Application::SaveMode::Automatic;
    QTimer* m_autosaveTimer = nullptr;
    QWidget* m_statusFooter = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_reloadButton = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_campusCombo = nullptr;
    QLineEdit* m_zoomLoginIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QCheckBox* m_zoomNotAvailableCheck = nullptr;
    QPushButton* m_imageModeButton = nullptr;
    QPushButton* m_typeModeButton = nullptr;
    QLabel* m_signaturePreviewLabel = nullptr;
    QWidget* m_imageControls = nullptr;
    QPushButton* m_chooseImageButton = nullptr;
    QPushButton* m_removeImageButton = nullptr;
    QWidget* m_typedControls = nullptr;
    QLineEdit* m_typedSignatureEdit = nullptr;
    std::vector<QPushButton*> m_fontButtons;
    std::vector<QLabel*> m_fontPreviewLabels;
    bool m_loading = false;
    bool m_loaded = false;
    bool m_dirty = false;
};

} // namespace ClassMngr::Next::Ui
