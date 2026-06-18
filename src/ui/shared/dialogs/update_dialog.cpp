#include "update_dialog.h"

#include "core/fontmanager.h"
#include "core/updater/update_installer.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

UpdateDialog::UpdateDialog(
    QWidget* parent
    )
    : QDialog(parent)
    , m_service(new UpdateService(UpdateConfiguration::fromBuild(), this))
    , m_downloader(new UpdateDownloader(this))
{
    buildUi();
    connectSignals();
}

void UpdateDialog::beginCheck()
{
    m_downloadedFilePath.clear();
    m_service->checkForUpdates();
}

void UpdateDialog::showAvailableUpdate(
    const UpdateCheckResult& result
    )
{
    handleCheckSucceeded(result);
}

void UpdateDialog::handleCheckStarted()
{
    m_progressBar->setVisible(false);
    m_notesLabel->clear();
    setStatus(
        tr("Checking for Updates"),
        tr("Contacting the update server...")
        );
    updateButtons(
        false,
        false,
        false
        );
}

void UpdateDialog::handleCheckSucceeded(
    const UpdateCheckResult& result
    )
{
    m_result =
        result;
    m_progressBar->setVisible(false);

    if (!result.updateAvailable)
    {
        setStatus(
            tr("You're Up to Date"),
            tr("ClassMngr %1 is the newest available version.")
                .arg(result.currentVersion.toString())
            );
        m_notesLabel->clear();
        updateButtons(
            true,
            false,
            false
            );
        return;
    }

    setStatus(
        tr("Update Available"),
        updateDetails(result)
        );

    if (result.notesUrl.isValid())
    {
        m_notesLabel->setText(
            QStringLiteral("<a href=\"%1\">%2</a>")
                .arg(
                    result.notesUrl.toString().toHtmlEscaped(),
                    tr("View release notes")
                    )
            );
    }
    else
    {
        m_notesLabel->clear();
    }

    updateButtons(
        true,
        true,
        false
        );
}

void UpdateDialog::handleCheckFailed(
    const QString& message
    )
{
    showFailure(message);
}

void UpdateDialog::startDownload()
{
    if (!m_result.updateAvailable)
    {
        return;
    }

    m_downloader->download(
        m_result.artifact
        );
}

void UpdateDialog::handleDownloadStarted()
{
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);

    setStatus(
        tr("Downloading Update"),
        tr("Downloading %1...")
            .arg(m_result.artifact.fileName)
        );
    updateButtons(
        false,
        false,
        false
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

    setStatus(
        tr("Ready to Install"),
        tr("The update was downloaded and verified.")
        );
    updateButtons(
        true,
        false,
        true
        );
}

void UpdateDialog::handleDownloadFailed(
    const QString& message
    )
{
    showFailure(message);
}

void UpdateDialog::installUpdate()
{
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
        auto status =
            UpdateInstaller::launch(
                m_downloadedFilePath
                );
        !status
        )
    {
        showFailure(status.error());
        return;
    }

    accept();

    QTimer::singleShot(
        250,
        qApp,
        &QCoreApplication::quit
        );
}

void UpdateDialog::buildUi()
{
    setWindowTitle(
        tr("Software Update")
        );
    setMinimumWidth(520);

    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        20,
        20,
        20,
        20
        );
    layout->setSpacing(14);

    m_titleLabel =
        new QLabel(this);
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            16,
            QFont::DemiBold
            )
        );

    m_detailsLabel =
        new QLabel(this);
    m_detailsLabel->setWordWrap(true);

    m_notesLabel =
        new QLabel(this);
    m_notesLabel->setOpenExternalLinks(true);
    m_notesLabel->setTextInteractionFlags(
        Qt::TextBrowserInteraction
        );

    m_progressBar =
        new QProgressBar(this);
    m_progressBar->setVisible(false);

    auto* buttonLayout =
        new QHBoxLayout;

    m_checkButton =
        new QPushButton(
            tr("Check Again"),
            this
            );
    m_downloadButton =
        new QPushButton(
            tr("Download"),
            this
            );
    m_installButton =
        new QPushButton(
            tr("Install and Close"),
            this
            );
    m_closeButton =
        new QPushButton(
            tr("Close"),
            this
            );

    m_downloadButton->setObjectName("primaryButton");
    m_installButton->setObjectName("primaryButton");

    buttonLayout->addWidget(m_checkButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_downloadButton);
    buttonLayout->addWidget(m_installButton);
    buttonLayout->addWidget(m_closeButton);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_detailsLabel);
    layout->addWidget(m_notesLabel);
    layout->addWidget(m_progressBar);
    layout->addLayout(buttonLayout);

    setStatus(
        tr("Software Update"),
        tr("Check for a newer version of ClassMngr.")
        );
    updateButtons(
        true,
        false,
        false
        );
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
        m_checkButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::beginCheck
        );
    connect(
        m_downloadButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::startDownload
        );
    connect(
        m_installButton,
        &QPushButton::clicked,
        this,
        &UpdateDialog::installUpdate
        );
    connect(
        m_closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
}

void UpdateDialog::setStatus(
    const QString& title,
    const QString& details
    )
{
    m_titleLabel->setText(title);
    m_detailsLabel->setText(details);
}

void UpdateDialog::showFailure(
    const QString& message
    )
{
    m_progressBar->setVisible(false);
    setStatus(
        tr("Update Check Failed"),
        message
        );
    updateButtons(
        true,
        false,
        false
        );
}

void UpdateDialog::updateButtons(
    bool checkEnabled,
    bool downloadEnabled,
    bool installEnabled
    )
{
    m_checkButton->setEnabled(checkEnabled);
    m_downloadButton->setEnabled(downloadEnabled);
    m_downloadButton->setVisible(downloadEnabled);
    m_installButton->setEnabled(installEnabled);
    m_installButton->setVisible(installEnabled);
}

QString UpdateDialog::updateDetails(
    const UpdateCheckResult& result
    ) const
{
    QString details =
        tr("ClassMngr %1 is available. You are currently using %2.")
            .arg(
                result.latestVersion.toString(),
                result.currentVersion.toString()
                );

    if (result.releaseDate.isValid())
    {
        details +=
            QStringLiteral("\n")
            + tr("Release date: %1")
                .arg(
                    result.releaseDate.toString(Qt::ISODate)
                    );
    }

    if (!result.currentVersionSupported)
    {
        details +=
            QStringLiteral("\n\n")
            + tr("This version is below the minimum supported version. Please update soon.");
    }

    return details;
}
