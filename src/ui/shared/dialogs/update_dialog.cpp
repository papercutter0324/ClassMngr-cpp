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
#include <QVBoxLayout>

UpdateDialog::UpdateDialog(
    UpdateService* service,
    bool startupComplete,
    QWidget* parent
    )
    : QDialog(parent)
    , m_service(service)
    , m_downloader(new UpdateDownloader(this))
    , m_startupComplete(startupComplete)
{
    Q_ASSERT(m_service);

    buildUi();
    connectSignals();
    showInitialState();
}

void UpdateDialog::refreshForOpen()
{
    if (m_service->hasResult())
    {
        showResult(
            *m_service->lastResult()
            );
    }

    if (m_service->isBusy())
    {
        handleCheckStarted();
        return;
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
        m_downloader->cancel();
    }

    QDialog::closeEvent(event);
}

void UpdateDialog::handleCheckStarted()
{
    m_lastRefreshError.clear();
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
        if (m_hasResult && m_result.updateAvailable)
        {
            m_downloader->download(
                m_result.artifact
                );
        }
        return;

    case PrimaryAction::CancelDownload:
        m_downloader->cancel();
        return;

    case PrimaryAction::Install:
    case PrimaryAction::Reveal:
        installOrRevealUpdate();
        return;

    case PrimaryAction::None:
        return;
    }
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
        PrimaryAction::CancelDownload,
        tr("Cancel Download"),
        true,
        tr("Close")
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

void UpdateDialog::handleDownloadFailed(
    const QString& message
    )
{
    showOperationFailure(
        tr("The update download failed: %1")
            .arg(message)
        );
}

void UpdateDialog::handleDownloadCancelled()
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
            tr("Updates"),
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

    m_statusIndicator =
        new QLabel(
            QStringLiteral("○"),
            m_programFrame
            );
    m_statusIndicator->setObjectName("programUpdateIndicator");
    m_statusIndicator->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    QFont indicatorFont =
        m_statusIndicator->font();
    indicatorFont.setPointSize(18);
    indicatorFont.setWeight(QFont::DemiBold);
    m_statusIndicator->setFont(indicatorFont);

    m_titleLabel =
        new QLabel(m_programFrame);
    m_titleLabel->setObjectName("programUpdateTitle");
    QFont titleFont =
        m_titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setWeight(QFont::DemiBold);
    m_titleLabel->setFont(titleFont);

    m_detailsLabel =
        new QLabel(m_programFrame);
    m_detailsLabel->setObjectName("programUpdateDetails");
    m_detailsLabel->setWordWrap(true);

    m_metadataLabel =
        new QLabel(m_programFrame);
    m_metadataLabel->setObjectName("programUpdateMetadata");
    m_metadataLabel->setWordWrap(true);

    m_notesLabel =
        new QLabel(m_programFrame);
    m_notesLabel->setObjectName("programUpdateNotes");
    m_notesLabel->setOpenExternalLinks(true);
    m_notesLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    m_progressBar =
        new QProgressBar(m_programFrame);
    m_progressBar->setObjectName("programUpdateProgress");
    m_progressBar->setVisible(false);

    programLayout->addWidget(m_statusIndicator, 0, 0, 2, 1);
    programLayout->addWidget(m_titleLabel, 0, 1);
    programLayout->addWidget(m_detailsLabel, 1, 1);
    programLayout->addWidget(m_metadataLabel, 2, 1);
    programLayout->addWidget(m_notesLabel, 3, 1);
    programLayout->addWidget(m_progressBar, 4, 0, 1, 2);

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
        &UpdateDownloader::downloadCancelled,
        this,
        &UpdateDialog::handleDownloadCancelled
        );

    connect(
        m_checkButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::forceCheck
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
        &QDialog::reject
        );
}

void UpdateDialog::showInitialState()
{
    m_progressBar->setVisible(false);
    m_notesLabel->clear();
    m_metadataLabel->clear();

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
}

void UpdateDialog::showResult(
    const UpdateCheckResult& result
    )
{
    m_result =
        result;
    m_hasResult =
        true;
    m_progressBar->setVisible(false);
    m_metadataLabel->setText(
        resultMetadata(result)
        );

    if (result.releaseUrl.isValid())
    {
        m_notesLabel->setText(
            QStringLiteral("<a href=\"%1\">%2</a>")
                .arg(
                    result.releaseUrl.toString().toHtmlEscaped(),
                    tr("View release notes")
                    )
            );
    }
    else
    {
        m_notesLabel->clear();
    }

    if (result.updateAvailable)
    {
        setStatus(
            QStringLiteral("●"),
            QStringLiteral("#e9a23b"),
            tr("Update Available"),
            resultDetails(result)
            );
        configureActions(
            false,
            false,
            PrimaryAction::Download,
            tr("Download Update"),
            true,
            tr("Not Now")
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
    m_metadataLabel->clear();
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
}

void UpdateDialog::showOperationFailure(
    const QString& message
    )
{
    m_progressBar->setVisible(false);

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

QString UpdateDialog::resultDetails(
    const UpdateCheckResult& result
    ) const
{
    if (result.updateAvailable)
    {
        return tr("ClassMngr %1 is available. You are currently using %2.")
            .arg(
                result.latestVersion.toString(),
                result.currentVersion.toString()
                );
    }

    return tr("ClassMngr %1 is the newest available version.")
        .arg(
            result.currentVersion.toString()
            );
}

QString UpdateDialog::resultMetadata(
    const UpdateCheckResult& result
    ) const
{
    QStringList metadata;

    if (result.releaseDate.isValid())
    {
        metadata.append(
            tr("Released %1")
                .arg(
                    QLocale::system().toString(
                        result.releaseDate,
                        QLocale::ShortFormat
                        )
                    )
            );
    }

    if (result.checkedAtUtc.isValid())
    {
        metadata.append(
            tr("Last checked %1")
                .arg(
                    QLocale::system().toString(
                        result.checkedAtUtc.toLocalTime(),
                        QLocale::ShortFormat
                        )
                    )
            );
    }

    return metadata.join(
        QStringLiteral("  •  ")
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
