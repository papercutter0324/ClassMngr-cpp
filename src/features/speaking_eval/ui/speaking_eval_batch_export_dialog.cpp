#include "speaking_eval_batch_export_dialog.h"

#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

enum class Scope
{
    CurrentStudent,
    AllStudents
};

} // namespace

SpeakingEvalBatchExportDialog::SpeakingEvalBatchExportDialog(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    int currentStudentIndex,
    const QString& defaultOutputDirectory,
    QWidget* parent
    )
    : QDialog(parent)
    , m_reports(reports)
    , m_currentStudentIndex(currentStudentIndex)
{
    setWindowTitle(tr("Export / Print Speaking Reports"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* introduction = new QLabel(
        tr("Export PDF speaking-evaluation reports, print them, or do both."),
        this
        );
    introduction->setWordWrap(true);
    layout->addWidget(introduction);

    auto* formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_scopeSelector = new QComboBox(this);
    m_scopeSelector->addItem(
        tr("Current Student"),
        static_cast<int>(Scope::CurrentStudent)
        );
    m_scopeSelector->addItem(
        tr("All Students (%1)").arg(m_reports.size()),
        static_cast<int>(Scope::AllStudents)
        );
    m_scopeSelector->setCurrentIndex(1);
    formLayout->addRow(tr("Reports:"), m_scopeSelector);

    m_rendererSelector = new QComboBox(this);
    m_rendererSelector->addItem(
        SpeakingEvalBatchReportService::rendererDisplayName(
            SpeakingEvalBatchReportService::Renderer::Internal
            ),
        static_cast<int>(SpeakingEvalBatchReportService::Renderer::Internal)
        );
#ifndef Q_OS_LINUX
    m_rendererSelector->addItem(
        SpeakingEvalBatchReportService::rendererDisplayName(
            SpeakingEvalBatchReportService::Renderer::PowerPoint
            ),
        static_cast<int>(SpeakingEvalBatchReportService::Renderer::PowerPoint)
        );
    m_rendererSelector->setCurrentIndex(1);
#endif
    formLayout->addRow(tr("Renderer:"), m_rendererSelector);

    m_rendererNote = new QLabel(this);
    m_rendererNote->setWordWrap(true);
    formLayout->addRow(QString(), m_rendererNote);

    m_savePdfCheck = new QCheckBox(tr("Export PDFs"), this);
    m_savePdfCheck->setChecked(true);
    formLayout->addRow(tr("Output:"), m_savePdfCheck);

    m_printReportsCheck = new QCheckBox(tr("Print Reports"), this);
    formLayout->addRow(QString(), m_printReportsCheck);

    auto* outputDirectoryLayout = new QHBoxLayout;
    m_outputDirectoryEdit = new QLineEdit(this);
    m_outputDirectoryEdit->setText(defaultOutputDirectory);
    m_chooseDirectoryButton = new TextFitPushButton(tr("Choose…"), this);
    outputDirectoryLayout->addWidget(m_outputDirectoryEdit, 1);
    outputDirectoryLayout->addWidget(m_chooseDirectoryButton);
    formLayout->addRow(tr("PDF Folder:"), outputDirectoryLayout);

    m_openOutputFolderCheck = new QCheckBox(
        tr("Open PDF Folder after saving"),
        this
        );
    m_openOutputFolderCheck->setChecked(false);
    formLayout->addRow(QString(), m_openOutputFolderCheck);

    layout->addLayout(formLayout);
    layout->addSpacing(16);

    auto* buttons = new QHBoxLayout;
    m_previewButton = new TextFitPushButton(tr("Preview Reports"), this);
    buttons->addWidget(m_previewButton);
    buttons->addStretch();
    m_exportButton = new TextFitPushButton(tr("Export"), this);
    auto* cancelButton = new TextFitPushButton(tr("Cancel"), this);
    buttons->addWidget(m_exportButton);
    buttons->addWidget(cancelButton);
    layout->addLayout(buttons);

    connect(
        m_scopeSelector,
        &QComboBox::currentIndexChanged,
        this,
        [this](int)
        {
            updateControls();
        }
        );
    connect(
        m_savePdfCheck,
        &QCheckBox::toggled,
        this,
        [this](bool)
        {
            updateControls();
        }
        );
    connect(
        m_previewButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalBatchExportDialog::previewReports
        );
    connect(
        m_printReportsCheck,
        &QCheckBox::toggled,
        this,
        [this](bool)
        {
            updateControls();
        }
        );
    connect(
        m_rendererSelector,
        &QComboBox::currentIndexChanged,
        this,
        [this](int)
        {
            updateControls();
        }
        );
    connect(
        m_chooseDirectoryButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalBatchExportDialog::chooseOutputDirectory
        );
    connect(
        m_exportButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalBatchExportDialog::exportReports
        );
    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );

    updateControls();

    const int dialogWidth =
        introduction->fontMetrics().horizontalAdvance(introduction->text())
        + 56;
    setFixedWidth(dialogWidth);
    resize(dialogWidth, layout->sizeHint().height());
    layout->activate();

    const int rendererNoteWidth = qMax(1, m_rendererNote->width());
    const auto rendererNoteHeightForText =
        [this, rendererNoteWidth](const QString& text)
        {
            m_rendererNote->setText(text);
            return m_rendererNote->heightForWidth(rendererNoteWidth);
        };
    const int rendererNoteHeight = qMax(
        rendererNoteHeightForText(
            tr("Only recommended if PowerPoint is unavailable or an error occurs. (Supports Windows, MacOS, and Linux.)")
            ),
        rendererNoteHeightForText(
            tr("Uses PowerPoint to generate student reports using a bundled template. (Supports Windows and MacOS.)")
            )
        );
    m_rendererNote->setFixedHeight(rendererNoteHeight);
    updateControls();

    layout->activate();
    setFixedHeight(layout->totalHeightForWidth(dialogWidth));
}

void SpeakingEvalBatchExportDialog::updateControls()
{
    const bool saving = m_savePdfCheck && m_savePdfCheck->isChecked();
    const bool hasOutput = saving
        || (m_printReportsCheck && m_printReportsCheck->isChecked());

    m_outputDirectoryEdit->setEnabled(saving);
    m_chooseDirectoryButton->setEnabled(saving);
    m_openOutputFolderCheck->setEnabled(saving);
    m_exportButton->setEnabled(hasOutput && !selectedReports().isEmpty());
    m_previewButton->setEnabled(!selectedReports().isEmpty());

    const bool oneReport = selectedReports().size() == 1;
    m_savePdfCheck->setText(oneReport ? tr("Export PDF") : tr("Export PDFs"));
    m_printReportsCheck->setText(oneReport ? tr("Print Report") : tr("Print Reports"));
    if (saving && m_printReportsCheck->isChecked())
    {
        m_exportButton->setText(tr("Export & Print"));
    }
    else if (saving)
    {
        m_exportButton->setText(tr("Export"));
    }
    else
    {
        m_exportButton->setText(tr("Print"));
    }

    if (selectedRenderer()
        == SpeakingEvalBatchReportService::Renderer::Internal)
    {
        m_rendererNote->setText(
            tr("Only recommended if PowerPoint is unavailable or an error occurs. (Supports Windows, MacOS, and Linux.)")
            );
        return;
    }

    m_rendererNote->setText(
        tr("Uses PowerPoint to generate student reports using a bundled template. (Supports Windows and MacOS.)")
        );
}

void SpeakingEvalBatchExportDialog::chooseOutputDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("Choose PDF Folder"),
        m_outputDirectoryEdit->text()
        );

    if (!directory.isEmpty())
    {
        m_outputDirectoryEdit->setText(directory);
    }
}

void SpeakingEvalBatchExportDialog::previewReports()
{
    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        selectedReports();
    if (reports.isEmpty())
    {
        return;
    }

    const bool previewingAll =
        m_scopeSelector->currentData().toInt()
        == static_cast<int>(Scope::AllStudents);
    SpeakingEvalReportDialog dialog(
        reports,
        previewingAll ? m_currentStudentIndex : 0,
        this
        );
    dialog.exec();
}

void SpeakingEvalBatchExportDialog::exportReports()
{
    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        selectedReports();
    if (reports.isEmpty())
    {
        return;
    }

    SpeakingEvalBatchReportService::Request request;
    request.parent = this;
    request.reports = reports;
    request.renderer = selectedRenderer();
    request.savePdf = m_savePdfCheck->isChecked();
    request.printReports = m_printReportsCheck->isChecked();
    request.outputDirectory = m_outputDirectoryEdit->text().trimmed();

    if (request.savePdf)
    {
        QDir outputDirectory(request.outputDirectory);
        if (request.outputDirectory.isEmpty()
            || (!outputDirectory.exists()
                && !QDir().mkpath(request.outputDirectory)))
        {
            QMessageBox::warning(
                this,
                tr("PDF Folder Unavailable"),
                tr("The selected PDF folder does not exist and could not be created. Choose another folder and try again.")
                );
            return;
        }

        QStringList existingFiles;
        for (int index = 0; index < reports.size(); ++index)
        {
            const QString path = outputDirectory.filePath(
                SpeakingEvalBatchReportService::safeFileName(
                    reports.at(index).displayName,
                    index + 1
                    )
                );
            if (QFileInfo::exists(path))
            {
                existingFiles.append(QFileInfo(path).fileName());
            }
        }

        if (!existingFiles.isEmpty())
        {
            const QMessageBox::StandardButton overwrite = QMessageBox::question(
                this,
                tr("Overwrite Existing Reports?"),
                tr("%1 existing report(s) will be replaced. Do you want to overwrite them?")
                    .arg(existingFiles.size()),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel
                );
            if (overwrite != QMessageBox::Yes)
            {
                return;
            }
            request.overwriteExisting = true;
        }
    }

    QProgressDialog progress(
        tr("Creating speaking-evaluation reports…"),
        tr("Cancel"),
        0,
        reports.size(),
        this
        );
    progress.setWindowTitle(tr("Exporting Reports"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    request.progressCallback = [&progress](
                                   int completed,
                                   int total,
                                   const QString& studentName
                                   )
    {
        progress.setMaximum(total);
        progress.setValue(completed);
        if (!studentName.isEmpty())
        {
            progress.setLabelText(
                QObject::tr("Creating report for %1…").arg(studentName)
                );
        }
        QCoreApplication::processEvents();
        return !progress.wasCanceled();
    };

    SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    progress.setValue(reports.size());

    if (result.status
        == SpeakingEvalBatchReportService::Status::InternalRendererFailed
        && SpeakingEvalBatchReportService::isPowerPointRendererAvailable())
    {
        const QMessageBox::StandardButton retry =
            QMessageBox::question(
                this,
                tr("Internal Export Failed"),
                tr("The internal renderer could not create this batch. No PDFs were saved or printed.\n\n%1\n\nRetry the entire batch using the PowerPoint template?")
                    .arg(result.message),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
                );
        if (retry == QMessageBox::Yes)
        {
            request.renderer =
                SpeakingEvalBatchReportService::Renderer::PowerPoint;
            progress.reset();
            result = SpeakingEvalBatchReportService::exportReports(request);
            progress.setValue(reports.size());
        }
    }

    switch (result.status)
    {
    case SpeakingEvalBatchReportService::Status::Completed:
        QMessageBox::information(
            this,
            tr("Reports Ready"),
            request.savePdf
                ? tr("%1 report(s) were saved%2.")
                      .arg(reports.size())
                      .arg(
                          request.printReports
                              ? tr(" and sent to the printer")
                              : QString()
                          )
                : tr("%1 report(s) were sent to the printer.")
                      .arg(reports.size())
            );
        if (request.savePdf && m_openOutputFolderCheck->isChecked())
        {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(
                    QDir(request.outputDirectory).absolutePath()
                    )
                );
        }
        accept();
        return;
    case SpeakingEvalBatchReportService::Status::Canceled:
        return;
    case SpeakingEvalBatchReportService::Status::Failed:
    case SpeakingEvalBatchReportService::Status::InternalRendererFailed:
        QMessageBox::warning(
            this,
            tr("Report Export Failed"),
            result.message
            );
        return;
    }
}

QList<SpeakingEvalBatchReportService::StudentReport>
SpeakingEvalBatchExportDialog::selectedReports() const
{
    if (!m_scopeSelector
        || m_scopeSelector->currentData().toInt()
            == static_cast<int>(Scope::AllStudents))
    {
        return m_reports;
    }

    if (m_currentStudentIndex < 0 || m_currentStudentIndex >= m_reports.size())
    {
        return {};
    }

    return { m_reports.at(m_currentStudentIndex) };
}

SpeakingEvalBatchReportService::Renderer
SpeakingEvalBatchExportDialog::selectedRenderer() const
{
    if (!m_rendererSelector)
    {
        return SpeakingEvalBatchReportService::Renderer::Internal;
    }

    return static_cast<SpeakingEvalBatchReportService::Renderer>(
        m_rendererSelector->currentData().toInt()
        );
}
