#include "update_dialog.h"

#include "core/updater/update_installer.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QtGlobal>
#include <QVBoxLayout>

UpdateDialog::UpdateDialog(
    UpdateService* service,
    bool startupComplete,
    QWidget* parent,
    const QString& skippedVersion
    )
    : QDialog(parent)
    , m_service(service)
    , m_downloader(new UpdateDownloader(this))
    , m_startupComplete(startupComplete)
    , m_skippedVersion(skippedVersion.trimmed())
{
    Q_ASSERT(m_service);
    if (!m_service)
    {
        qFatal("UpdateDialog requires a valid UpdateService pointer.");
    }

    buildUi();
    connectSignals();
    showInitialState();
}

void UpdateDialog::setSkippedVersion(
    const QString& version
    )
{
    m_skippedVersion =
        version.trimmed();

    if (m_hasResult && !m_downloader->isBusy())
    {
        showResult(m_result);
    }
}

void UpdateDialog::refreshForOpen()
{
    if (m_service->isBusy())
    {
        handleCheckStarted();
        return;
    }

    if (m_service->hasResult())
    {
        showResult(
            *m_service->lastResult()
            );
    }

    if (!m_service->isResultFresh())
    {
        m_service->checkForUpdates(
            UpdateService::CheckPolicy::IfStale
            );
    }
}

void UpdateDialog::setStartupComplete(
    bool complete
    )
{
    m_startupComplete =
        complete;

    if (
        complete
        && !m_downloadedFilePath.isEmpty()
        && m_hasResult
        && m_result.updateAvailable
        && m_downloader->hasCompletedDownload(
            m_result.artifact
            )
        )
    {
        showReadyToInstall();
    }
}

void UpdateDialog::closeEvent(
    QCloseEvent* event
    )
{
    if (m_downloader->isBusy())
    {
        m_downloader->pause();
    }

    QDialog::closeEvent(event);
}

void UpdateDialog::handleCheckStarted()
{
    m_lastRefreshError.clear();

    m_metadataLabel->clear();
    m_metadataLabel->setVisible(false);
    m_notesLabel->clear();
    m_notesLabel->setVisible(false);

    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(true);

    setStatus(
        QStringLiteral("◌"),
        QStringLiteral("#4da3ff"),
        tr("Checking for Updates"),
        tr("Contacting GitHub Releases...")
        );
    configureActions(
        false,
        false,
        PrimaryAction::None,
        QString(),
        false,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::handleCheckSucceeded(
    const UpdateCheckResult& result
    )
{
    m_lastRefreshError.clear();
    showResult(result);
}

void UpdateDialog::handleCheckFailed(
    const QString& message
    )
{
    m_lastRefreshError =
        message;
    showFailure(message);
}

void UpdateDialog::forceCheck()
{
    m_service->checkForUpdates(
        UpdateService::CheckPolicy::Force
        );
}

void UpdateDialog::handlePrimaryAction()
{
    switch (m_primaryAction)
    {
    case PrimaryAction::Download:
    case PrimaryAction::Resume:
        if (m_hasResult && m_result.updateAvailable)
        {
            m_downloader->download(
                m_result.artifact
                );
        }
        return;

    case PrimaryAction::Pause:
        m_downloader->pause();
        return;

    case PrimaryAction::Install:
    case PrimaryAction::Reveal:
        installOrRevealUpdate();
        return;

    case PrimaryAction::None:
        return;
    }
}

void UpdateDialog::handleSecondaryAction()
{
    switch (m_secondaryAction)
    {
    case SecondaryAction::SkipVersion:
        if (m_hasResult && m_result.updateAvailable)
        {
            m_skippedVersion =
                m_result.latestVersion.toString();
            emit skipVersionRequested(
                m_skippedVersion
                );
            reject();
        }
        return;

    case SecondaryAction::UnskipVersion:
        m_skippedVersion.clear();
        emit unskipVersionRequested();
        if (m_hasResult)
        {
            showResult(m_result);
        }
        return;

    case SecondaryAction::DiscardDownload:
        m_downloader->discard();
        return;

    case SecondaryAction::None:
        return;
    }
}

void UpdateDialog::handleDownloadPreparing(
    qint64 bytesAvailable,
    qint64 bytesTotal
    )
{
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(
        bytesTotal > 0
            ? static_cast<int>(
                (bytesAvailable * 100) / bytesTotal
                )
            : 0
        );
    m_progressBar->setVisible(true);

    setStatus(
        QStringLiteral("◌"),
        QStringLiteral("#4da3ff"),
        tr("Preparing Download"),
        tr("Verifying the saved update data before resuming...")
        );
    configureActions(
        false,
        false,
        PrimaryAction::Pause,
        tr("Pause Download"),
        true,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::handleDownloadStarted()
{
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);

    setStatus(
        QStringLiteral("↓"),
        QStringLiteral("#4da3ff"),
        tr("Downloading Update"),
        tr("Downloading %1...")
            .arg(m_result.artifact.fileName)
        );
    configureActions(
        false,
        false,
        PrimaryAction::Pause,
        tr("Pause Download"),
        true,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::handleDownloadProgress(
    qint64 bytesReceived,
    qint64 bytesTotal
    )
{
    if (bytesTotal <= 0)
    {
        m_progressBar->setRange(0, 0);
        return;
    }

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(
        static_cast<int>(
            (bytesReceived * 100) / bytesTotal
            )
        );
}

void UpdateDialog::handleDownloadSucceeded(
    const QString& filePath
    )
{
    m_downloadedFilePath =
        filePath;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    showReadyToInstall();
}

void UpdateDialog::handleDownloadPaused(
    qint64 bytesReceived,
    qint64 bytesTotal
    )
{
    m_downloadedFilePath.clear();
    showPausedDownload(
        bytesReceived,
        bytesTotal
        );
}

void UpdateDialog::handleDownloadVerifying()
{
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);

    setStatus(
        QStringLiteral("◌"),
        QStringLiteral("#4da3ff"),
        tr("Verifying Download"),
        tr("Checking the update size and SHA-256 checksum...")
        );
    configureActions(
        false,
        false,
        PrimaryAction::Pause,
        tr("Pause Download"),
        true,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::handleDownloadFailed(
    const QString& message
    )
{
    showOperationFailure(
        tr("The update download failed: %1")
            .arg(message)
        );
}

void UpdateDialog::handleDownloadDiscarded()
{
    m_downloadedFilePath.clear();

    if (m_hasResult)
    {
        showResult(m_result);
    }
    else
    {
        showInitialState();
    }
}

void UpdateDialog::buildUi()
{
    setWindowTitle(
        tr("Updates")
        );
    setMinimumWidth(560);

    auto* rootLayout =
        new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(14);

    auto* heading =
        new QLabel(
            tr("ClassMngr"),
            this
            );
    heading->setObjectName("pageTitle");
    QFont headingFont =
        heading->font();
    headingFont.setPointSize(16);
    headingFont.setWeight(QFont::DemiBold);
    heading->setFont(headingFont);
    rootLayout->addWidget(heading);

    m_programFrame =
        new QFrame(this);
    m_programFrame->setObjectName("programUpdateSection");
    m_programFrame->setFrameShape(QFrame::StyledPanel);

    auto* programLayout =
        new QGridLayout(m_programFrame);
    programLayout->setContentsMargins(16, 14, 16, 14);
    programLayout->setHorizontalSpacing(12);
    programLayout->setVerticalSpacing(6);

    // Column 0: indicator (fixed width ~32px)
    programLayout->setColumnMinimumWidth(0, 32);
    // Column 1: horizontal spacer (~24px minimum)
    programLayout->setColumnMinimumWidth(1, 24);
    // Column 2: content area expands
    programLayout->setColumnStretch(0, 0);
    programLayout->setColumnStretch(1, 0);
    programLayout->setColumnStretch(2, 1);

    // Row 0-3: content rows (minimum size, no expansion)
    programLayout->setRowStretch(0, 0);
    programLayout->setRowStretch(1, 0);
    programLayout->setRowStretch(2, 0);
    programLayout->setRowStretch(3, 0);
    // Row 4: progress bar (expands if needed)
    programLayout->setRowStretch(4, 1);

    m_statusIndicator =
        new QLabel(
            QStringLiteral("○"),
            m_programFrame
            );
    m_statusIndicator->setObjectName("programUpdateIndicator");
    m_statusIndicator->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    QFont indicatorFont =
        m_statusIndicator->font();
    indicatorFont.setPointSize(18);
    indicatorFont.setWeight(QFont::DemiBold);
    m_statusIndicator->setFont(indicatorFont);

    // Swap font sizes: title larger (14pt), details smaller (10pt)
    m_titleLabel =
        new QLabel(m_programFrame);
    m_titleLabel->setObjectName("programUpdateTitle");
    QFont titleFont =
        m_titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setWeight(QFont::DemiBold);
    m_titleLabel->setFont(titleFont);

    m_detailsLabel =
        new QLabel(m_programFrame);
    m_detailsLabel->setObjectName("programUpdateDetails");
    m_detailsLabel->setWordWrap(true);
    QFont detailsFont =
        m_detailsLabel->font();
    detailsFont.setPointSize(10);
    m_detailsLabel->setFont(detailsFont);

    m_metadataLabel =
        new QLabel(m_programFrame);
    m_metadataLabel->setObjectName("programUpdateMetadata");
    m_metadataLabel->setWordWrap(true);

    m_notesLabel =
        new QLabel(m_programFrame);
    m_notesLabel->setObjectName("programUpdateNotes");
    m_notesLabel->setOpenExternalLinks(true);
    m_notesLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QFont notesFont =
        m_notesLabel->font();
    notesFont.setPointSize(10);
    m_notesLabel->setFont(notesFont);

    m_progressBar =
        new QProgressBar(m_programFrame);
    m_progressBar->setObjectName("programUpdateProgress");
    m_progressBar->setVisible(false);

    // Add left alignment to all content labels
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_detailsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_metadataLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_notesLabel->setAlignment(Qt::AlignLeft);

    // Place indicator in row 0-3 (spanning all content rows, vertically centered)
    programLayout->addWidget(m_statusIndicator, 0, 0, 4, 1, Qt::AlignVCenter);
    // Content labels start from row 0 in column 2
    programLayout->addWidget(m_titleLabel, 0, 2);
    programLayout->addWidget(m_detailsLabel, 1, 2);
    programLayout->addWidget(m_metadataLabel, 2, 2);
    programLayout->addWidget(m_notesLabel, 3, 2);
    programLayout->addWidget(m_progressBar, 4, 0, 1, 3);

    rootLayout->addWidget(m_programFrame);

    auto* resourceFrame =
        new QFrame(this);
    resourceFrame->setObjectName("resourcePackUpdateSection");
    resourceFrame->setFrameShape(QFrame::StyledPanel);
    resourceFrame->setEnabled(false);

    auto* resourceLayout =
        new QHBoxLayout(resourceFrame);
    resourceLayout->setContentsMargins(16, 14, 16, 14);
    resourceLayout->setSpacing(12);

    auto* resourceIndicator =
        new QLabel(
            QStringLiteral("○"),
            resourceFrame
            );
    resourceIndicator->setObjectName("resourcePackUpdateIndicator");
    resourceIndicator->setFont(indicatorFont);

    auto* resourceText =
        new QLabel(
            tr("<b>Resource Packs</b><br>Coming soon"),
            resourceFrame
            );
    resourceText->setObjectName("resourcePackUpdateText");

    resourceLayout->addWidget(resourceIndicator);
    resourceLayout->addWidget(resourceText, 1);
    rootLayout->addWidget(resourceFrame);

    auto* buttonLayout =
        new QHBoxLayout;

    m_checkButton =
        new TextFitPushButton(
            tr("Check Again"),
            this
            );
    m_checkButton->setObjectName("updateCheckButton");

    m_secondaryButton =
        new TextFitPushButton(this);
    m_secondaryButton->setObjectName("updateSecondaryButton");

    m_primaryButton =
        new TextFitPushButton(this);
    m_primaryButton->setObjectName("updatePrimaryButton");

    m_closeButton =
        new TextFitPushButton(
            tr("Close"),
            this
            );
    m_closeButton->setObjectName("updateCloseButton");

    buttonLayout->addWidget(m_checkButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_secondaryButton);
    buttonLayout->addWidget(m_primaryButton);
    buttonLayout->addWidget(m_closeButton);
    rootLayout->addLayout(buttonLayout);
}

void UpdateDialog::connectSignals()
{
    connect(
        m_service,
        &UpdateService::checkStarted,
        this,
        &UpdateDialog::handleCheckStarted
        );
    connect(
        m_service,
        &UpdateService::checkSucceeded,
        this,
        &UpdateDialog::handleCheckSucceeded
        );
    connect(
        m_service,
        &UpdateService::checkFailed,
        this,
        &UpdateDialog::handleCheckFailed
        );

    connect(
        m_downloader,
        &UpdateDownloader::downloadPreparing,
        this,
        &UpdateDialog::handleDownloadPreparing
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadStarted,
        this,
        &UpdateDialog::handleDownloadStarted
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadProgress,
        this,
        &UpdateDialog::handleDownloadProgress
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadPaused,
        this,
        &UpdateDialog::handleDownloadPaused
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadVerifying,
        this,
        &UpdateDialog::handleDownloadVerifying
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadSucceeded,
        this,
        &UpdateDialog::handleDownloadSucceeded
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadFailed,
        this,
        &UpdateDialog::handleDownloadFailed
        );
    connect(
        m_downloader,
        &UpdateDownloader::downloadDiscarded,
        this,
        &UpdateDialog::handleDownloadDiscarded
        );

    connect(
        m_checkButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::forceCheck
        );
    connect(
        m_secondaryButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::handleSecondaryAction
        );
    connect(
        m_primaryButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::handlePrimaryAction
        );
    connect(
        m_closeButton,
        &QPushButton::clicked,
        this,
        &QWidget::close
        );
}

void UpdateDialog::showInitialState()
{
    m_progressBar->setVisible(false);
    m_notesLabel->clear();
    m_notesLabel->setVisible(false);
    m_metadataLabel->clear();
    m_metadataLabel->setVisible(false);

    setStatus(
        QStringLiteral("○"),
        QStringLiteral("#808080"),
        tr("ClassMngr"),
        tr("No update check has been completed yet.")
        );
    configureActions(
        true,
        true,
        PrimaryAction::None,
        QString(),
        false,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::showResult(
    const UpdateCheckResult& result
    )
{
    const bool versionChanged =
        m_hasResult
        && m_result.latestVersion.toString()
            != result.latestVersion.toString();
    const bool artifactChanged =
        m_hasResult
        && (
            m_result.artifact.fileName != result.artifact.fileName
            || m_result.artifact.sizeBytes != result.artifact.sizeBytes
            );

    if (!result.updateAvailable || versionChanged || artifactChanged)
    {
        m_downloadedFilePath.clear();
    }

    m_result =
        result;
    m_hasResult =
        true;
    m_progressBar->setVisible(false);
    const QString metadataText =
        resultMetadata(result);
    if (metadataText.isEmpty())
    {
        m_metadataLabel->clear();
        m_metadataLabel->setVisible(false);
    }
    else
    {
        m_metadataLabel->setText(metadataText);
        m_metadataLabel->setVisible(true);
    }

    if (result.releaseUrl.isValid())
    {
        m_notesLabel->setText(
            QStringLiteral("<a href=\"%1\">%2</a>")
                .arg(
                    result.releaseUrl.toString().toHtmlEscaped(),
                    tr("View release notes")
                    )
            );
        m_notesLabel->setVisible(true);
    }
    else
    {
        m_notesLabel->clear();
        m_notesLabel->setVisible(false);
    }

    if (result.updateAvailable)
    {
        if (m_downloader->hasCompletedDownload(result.artifact))
        {
            if (!m_downloadedFilePath.isEmpty())
            {
                showReadyToInstall();
                return;
            }

            // Re-enter the downloader so it can verify the saved artifact and
            // report the verified file path through downloadSucceeded().
            m_downloader->download(
                result.artifact
                );
            return;
        }

        if (m_downloader->hasResumableDownload(result.artifact))
        {
            showPausedDownload(
                m_downloader->resumableBytes(result.artifact),
                result.artifact.sizeBytes
                );
            return;
        }

        const bool skipped =
            m_skippedVersion
            == result.latestVersion.toString();
        QString details =
            resultDetails(result);
        if (skipped)
        {
            details +=
                QStringLiteral("\n\n")
                + tr("Automatic prompts are disabled for this version.");
        }

        setStatus(
            QStringLiteral("●"),
            QStringLiteral("#e9a23b"),
            tr("Update Available"),
            details
            );
        configureActions(
            false,
            false,
            PrimaryAction::Download,
            tr("Download Update"),
            true,
            tr("Not Now")
            );
        configureSecondaryAction(
            skipped
                ? SecondaryAction::UnskipVersion
                : SecondaryAction::SkipVersion,
            skipped
                ? tr("Notify Me About This Version")
                : tr("Skip This Version")
            );
        return;
    }

    setStatus(
        QStringLiteral("●"),
        QStringLiteral("#35a853"),
        tr("You're Up to Date"),
        resultDetails(result)
        );
    configureActions(
        true,
        true,
        PrimaryAction::None,
        QString(),
        false,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::showReadyToInstall()
{
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);

    QString details =
        tr("The update was downloaded and verified.");

    if (!m_startupComplete)
    {
        details +=
            QStringLiteral("\n")
            + tr("Installation will be available after ClassMngr finishes starting.");
    }

#if defined(Q_OS_LINUX)
    setStatus(
        QStringLiteral("✓"),
        QStringLiteral("#35a853"),
        tr("Download Complete"),
        details
        );
    configureActions(
        false,
        false,
        PrimaryAction::Reveal,
        tr("Open Download Folder"),
        m_startupComplete,
        tr("Close")
        );
#else
    setStatus(
        QStringLiteral("✓"),
        QStringLiteral("#35a853"),
        tr("Ready to Install"),
        details
        );
    configureActions(
        false,
        false,
        PrimaryAction::Install,
        tr("Install and Close"),
        m_startupComplete,
        tr("Not Now")
        );
#endif

    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::showPausedDownload(
    qint64 bytesReceived,
    qint64 bytesTotal
    )
{
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(
        bytesTotal > 0
            ? static_cast<int>(
                (bytesReceived * 100) / bytesTotal
                )
            : 0
        );

    const QString amount =
        tr("%1 of %2")
            .arg(
                QLocale::system().formattedDataSize(bytesReceived),
                QLocale::system().formattedDataSize(bytesTotal)
                );

    setStatus(
        QStringLiteral("Ⅱ"),
        QStringLiteral("#e9a23b"),
        tr("Download Paused"),
        tr("%1 has been saved and can be resumed.")
            .arg(amount)
        );
    configureActions(
        false,
        false,
        PrimaryAction::Resume,
        tr("Resume Download"),
        true,
        tr("Close")
        );
    configureSecondaryAction(
        SecondaryAction::DiscardDownload,
        tr("Discard Download")
        );
}

void UpdateDialog::showFailure(
    const QString& message
    )
{
    m_progressBar->setVisible(false);

    if (m_service->hasResult())
    {
        showResult(
            *m_service->lastResult()
            );
        m_statusIndicator->setText(
            QStringLiteral("!")
            );
        m_statusIndicator->setStyleSheet(
            QStringLiteral("color: #d9534f;")
            );
        m_detailsLabel->setText(
            m_detailsLabel->text()
            + QStringLiteral("\n\n")
            + tr("The latest refresh failed: %1")
                .arg(message)
            );
        m_checkButton->setVisible(true);
        m_checkButton->setEnabled(true);
        return;
    }

    m_notesLabel->clear();
    m_notesLabel->setVisible(false);
    m_metadataLabel->clear();
    m_metadataLabel->setVisible(false);
    setStatus(
        QStringLiteral("!"),
        QStringLiteral("#d9534f"),
        tr("Update Check Failed"),
        message
        );
    configureActions(
        true,
        true,
        PrimaryAction::None,
        QString(),
        false,
        tr("Close")
        );
    m_checkButton->setText(
        tr("Try Again")
        );
    configureSecondaryAction(
        SecondaryAction::None,
        QString()
        );
}

void UpdateDialog::showOperationFailure(
    const QString& message
    )
{
    m_progressBar->setVisible(false);

    if (
        m_hasResult
        && m_downloader->hasResumableDownload(
            m_result.artifact
            )
        )
    {
        showPausedDownload(
            m_downloader->resumableBytes(
                m_result.artifact
                ),
            m_result.artifact.sizeBytes
            );
        m_statusIndicator->setText(
            QStringLiteral("!")
            );
        m_statusIndicator->setStyleSheet(
            QStringLiteral("color: #d9534f;")
            );
        m_detailsLabel->setText(
            m_detailsLabel->text()
            + QStringLiteral("\n\n")
            + message
            );
        return;
    }

    if (m_hasResult)
    {
        showResult(m_result);
    }

    m_statusIndicator->setText(
        QStringLiteral("!")
        );
    m_statusIndicator->setStyleSheet(
        QStringLiteral("color: #d9534f;")
        );
    m_detailsLabel->setText(
        m_detailsLabel->text()
        + QStringLiteral("\n\n")
        + message
        );
}

void UpdateDialog::setStatus(
    const QString& indicator,
    const QString& indicatorColor,
    const QString& title,
    const QString& details
    )
{
    m_statusIndicator->setText(indicator);
    m_statusIndicator->setStyleSheet(
        QStringLiteral("color: %1;")
            .arg(indicatorColor)
        );
    m_titleLabel->setText(title);
    m_detailsLabel->setText(details);
}

void UpdateDialog::configureActions(
    bool checkVisible,
    bool checkEnabled,
    PrimaryAction primaryAction,
    const QString& primaryText,
    bool primaryEnabled,
    const QString& closeText
    )
{
    m_checkButton->setText(
        tr("Check Again")
        );
    m_checkButton->setVisible(checkVisible);
    m_checkButton->setEnabled(checkEnabled);

    m_primaryAction =
        primaryAction;
    m_primaryButton->setText(primaryText);
    m_primaryButton->setVisible(
        primaryAction != PrimaryAction::None
        );
    m_primaryButton->setEnabled(primaryEnabled);

    m_closeButton->setText(closeText);
    m_closeButton->setEnabled(true);
}

void UpdateDialog::configureSecondaryAction(
    SecondaryAction action,
    const QString& text,
    bool enabled
    )
{
    m_secondaryAction =
        action;
    m_secondaryButton->setText(text);
    m_secondaryButton->setVisible(
        action != SecondaryAction::None
        );
    m_secondaryButton->setEnabled(enabled);
}

QString UpdateDialog::resultDetails(
    const UpdateCheckResult& result
    ) const
{
    QString releaseInfo;
    if (result.releaseDate.isValid())
    {
        releaseInfo = tr(" (Released: %1)")
            .arg(
                QLocale::system().toString(
                    result.releaseDate,
                    QLocale::ShortFormat
                    )
                );
    }

    if (result.updateAvailable)
    {
        return tr("ClassMngr %1 is available%2. You are currently using %3.")
            .arg(
                result.latestVersion.toString(),
                releaseInfo,
                result.currentVersion.toString()
                );
    }

    return tr("Latest Version: %1%2")
        .arg(
            result.latestVersion.toString(),
            releaseInfo.isEmpty()
                ? QString()
                : releaseInfo
            );
}

QString UpdateDialog::resultMetadata(
    const UpdateCheckResult& result
    ) const
{
    if (!result.updateAvailable)
    {
        return QString();
    }

    const QString fileName =
        result.artifact.fileName.trimmed();

    if (fileName.isEmpty())
    {
        return QString();
    }

    if (result.artifact.sizeBytes <= 0)
    {
        return fileName;
    }

    return tr("%1 • %2")
        .arg(
            fileName,
            QLocale::system().formattedDataSize(
                result.artifact.sizeBytes
                )
            );
}

void UpdateDialog::installOrRevealUpdate()
{
    if (m_downloadedFilePath.isEmpty() || !m_startupComplete)
    {
        return;
    }

#if defined(Q_OS_LINUX)
    if (
        const Status status =
            UpdateInstaller::revealInFolder(
                m_downloadedFilePath
                );
        !status
        )
    {
        showOperationFailure(status.error());
        return;
    }

    QMessageBox::information(
        this,
        tr("Linux Update Downloaded"),
        tr("Extract the downloaded archive and replace your existing ClassMngr application directory when convenient.")
        );
#else
    const auto response =
        QMessageBox::question(
            this,
            tr("Install Update"),
            tr("ClassMngr will close after opening the update package. Continue?")
            );

    if (response != QMessageBox::Yes)
    {
        return;
    }

    if (
        const Status status =
            UpdateInstaller::launch(
                m_downloadedFilePath
                );
        !status
        )
    {
        showOperationFailure(status.error());
        return;
    }

    accept();

    QTimer::singleShot(
        250,
        qApp,
        &QCoreApplication::quit
        );
#endif
}