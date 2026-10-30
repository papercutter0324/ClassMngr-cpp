#pragma once

#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"
#include "next/application/manual_update_check.h"
#include "next/application/manual_update_check_use_case.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <memory>

class QCloseEvent;
class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;

namespace ClassMngr::Next::UiShared::Details
{

[[nodiscard]] UpdateCheckResult toPresentationResult(
    const ClassMngr::Next::Application::ManualUpdateCheckResult& result
    );

} // namespace ClassMngr::Next::UiShared::Details

class UpdateDialog : public DialogShell
{
    Q_OBJECT

public:
    // Legacy callers retain their resource-pack placeholder. Next explicitly
    // omits this UI at construction rather than inspecting private widgets.
    enum class ResourcePackPlaceholder
    {
        Show,
        Omit
    };

    explicit UpdateDialog(
        ClassMngr::Next::Application::ManualUpdateCheckPort& manualUpdateCheckPort,
        bool startupComplete,
        QWidget* parent = nullptr,
        const QString& skippedVersion = QString(),
        ResourcePackPlaceholder resourcePackPlaceholder = ResourcePackPlaceholder::Show
        );

    void refreshForOpen();
    void setStartupComplete(
        bool complete
        );
    void setSkippedVersion(
        const QString& version
        );
    void setSkipVersionResultHandlingEnabled(
        bool enabled
        );
    void completeSkipVersion(
        const QString& version
        );
    void completeUnskipVersion();
    void showSkippedVersionSaveFailure(
        const QString& message
        );
    void showSkippedVersionClearFailure(
        const QString& message
        );

signals:
    void checkResultReceived(
        const ClassMngr::Next::Application::ManualUpdateCheckResult& result
        );
    void skipVersionRequested(
        const QString& version
        );
    void unskipVersionRequested();

protected:
    void closeEvent(
        QCloseEvent* event
        ) override;

private slots:
    void handleCheckStarted();
    void handleCheckSucceeded(
        const UpdateCheckResult& result
        );
    void handleCheckFailed(
        const QString& message
        );
    void forceCheck();
    void handlePrimaryAction();
    void handleSecondaryAction();
    void handleDownloadPreparing(
        qint64 bytesAvailable,
        qint64 bytesTotal
        );
    void handleDownloadStarted();
    void handleDownloadProgress(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void handleDownloadSucceeded(
        const QString& filePath
        );
    void handleDownloadFailed(
        const QString& message
        );
    void handleDownloadPaused(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void handleDownloadVerifying();
    void handleDownloadDiscarded();

private:
    enum class PrimaryAction
    {
        None,
        Download,
        Resume,
        Pause,
        Install,
        Reveal
    };

    enum class SecondaryAction
    {
        None,
        SkipVersion,
        UnskipVersion,
        DiscardDownload
    };

    void buildUi(ResourcePackPlaceholder resourcePackPlaceholder);
    void connectSignals();
    void handleManualCheckEvent(
        ClassMngr::Next::Application::ManualUpdateCheckEvent event
        );
    void showInitialState();
    void showResult(
        const UpdateCheckResult& result
        );
    void showReadyToInstall();
    void showPausedDownload(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void showFailure(
        const QString& message
        );
    void showOperationFailure(
        const QString& message
        );
    void setStatus(
        const QString& indicator,
        const QString& indicatorColor,
        const QString& title,
        const QString& details
        );
    void setProgressText(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void clearProgressText();
    void configureActions(
        bool checkVisible,
        bool checkEnabled,
        PrimaryAction primaryAction,
        const QString& primaryText,
        bool primaryEnabled,
        const QString& closeText
        );
    void configureSecondaryAction(
        SecondaryAction action,
        const QString& text,
        bool enabled = true
        );
    QString resultDetails(
        const UpdateCheckResult& result
        ) const;
    void installOrRevealUpdate();

private:
    UpdateDownloader* m_downloader = nullptr;
    std::unique_ptr<
        ClassMngr::Next::Application::ManualUpdateCheckUseCase
        > m_manualUpdateCheckUseCase;

    QFrame* m_programFrame = nullptr;
    QLabel* m_statusIndicator = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_detailsLabel = nullptr;
    QLabel* m_notesLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_progressLabel = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_secondaryButton = nullptr;
    QPushButton* m_primaryButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    UpdateCheckResult m_result;
    QString m_downloadedFilePath;
    QString m_skippedVersion;
    PrimaryAction m_primaryAction = PrimaryAction::None;
    SecondaryAction m_secondaryAction = SecondaryAction::None;
    bool m_hasResult = false;
    bool m_startupComplete = false;
    bool m_skipVersionResultHandlingEnabled = false;
};
