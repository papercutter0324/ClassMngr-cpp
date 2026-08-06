#pragma once

#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"

#include <QDialog>

class QCloseEvent;
class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(
        UpdateService* service,
        bool startupComplete,
        QWidget* parent = nullptr
        );

    void refreshForOpen();
    void setStartupComplete(
        bool complete
        );

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
    void handleDownloadCancelled();

private:
    enum class PrimaryAction
    {
        None,
        Download,
        CancelDownload,
        Install,
        Reveal
    };

    void buildUi();
    void connectSignals();
    void showInitialState();
    void showResult(
        const UpdateCheckResult& result
        );
    void showReadyToInstall();
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
    void configureActions(
        bool checkVisible,
        bool checkEnabled,
        PrimaryAction primaryAction,
        const QString& primaryText,
        bool primaryEnabled,
        const QString& closeText
        );
    QString resultDetails(
        const UpdateCheckResult& result
        ) const;
    QString resultMetadata(
        const UpdateCheckResult& result
        ) const;
    void installOrRevealUpdate();

private:
    UpdateService* m_service = nullptr;
    UpdateDownloader* m_downloader = nullptr;

    QFrame* m_programFrame = nullptr;
    QLabel* m_statusIndicator = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_detailsLabel = nullptr;
    QLabel* m_metadataLabel = nullptr;
    QLabel* m_notesLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_primaryButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    UpdateCheckResult m_result;
    QString m_downloadedFilePath;
    QString m_lastRefreshError;
    PrimaryAction m_primaryAction = PrimaryAction::None;
    bool m_hasResult = false;
    bool m_startupComplete = false;
};
