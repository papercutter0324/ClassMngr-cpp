#pragma once

#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(
        QWidget* parent = nullptr
        );

    void beginCheck();
    void showAvailableUpdate(
        const UpdateCheckResult& result
        );

private slots:
    void handleCheckStarted();
    void handleCheckSucceeded(
        const UpdateCheckResult& result
        );
    void handleCheckFailed(
        const QString& message
        );
    void startDownload();
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
    void installUpdate();

private:
    void buildUi();
    void connectSignals();
    void setStatus(
        const QString& title,
        const QString& details
        );
    void showFailure(
        const QString& message
        );
    void updateButtons(
        bool checkEnabled,
        bool downloadEnabled,
        bool installEnabled
        );
    QString updateDetails(
        const UpdateCheckResult& result
        ) const;

private:
    UpdateService* m_service = nullptr;
    UpdateDownloader* m_downloader = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_detailsLabel = nullptr;
    QLabel* m_notesLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_installButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    UpdateCheckResult m_result;
    QString m_downloadedFilePath;
};
