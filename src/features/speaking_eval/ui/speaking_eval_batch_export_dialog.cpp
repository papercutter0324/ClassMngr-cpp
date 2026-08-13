#include "speaking_eval_batch_export_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
    Mode mode,
    QWidget* parent
    )
    : QDialog(parent)
    , m_reports(reports)
    , m_currentStudentIndex(currentStudentIndex)
    , m_mode(mode)
{
    const bool saving = m_mode == Mode::SaveAs;
    setWindowTitle(
        saving
            ? tr("Save Speaking Reports As")
            : tr("Print Speaking Reports")
        );

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* introduction = new QLabel(
        saving
            ? tr("Save speaking-evaluation reports to files.")
            : tr("Print speaking-evaluation reports."),
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
#endif
    formLayout->addRow(tr("Renderer:"), m_rendererSelector);

    m_rendererNote = new QLabel(this);
    m_rendererNote->setWordWrap(true);
    formLayout->addRow(QString(), m_rendererNote);

    m_savePdfCheck = new QCheckBox(tr("Export PDFs"), this);
    m_savePdfCheck->setChecked(saving);
    m_savePdfCheck->setEnabled(false);
    m_savePdfCheck->setVisible(saving);
    formLayout->addRow(tr("Output:"), m_savePdfCheck);
    formLayout->labelForField(m_savePdfCheck)->setVisible(saving);

    m_keepIndividualPdfsCheck = new QCheckBox(
        tr("Keep individual PDFs after zipping"),
        this
        );
    m_keepIndividualPdfsCheck->setChecked(false);
    formLayout->addRow(QString(), m_keepIndividualPdfsCheck);

    m_printReportsCheck = new QCheckBox(tr("Print Reports"), this);
    m_printReportsCheck->setChecked(!saving);
    m_printReportsCheck->setEnabled(false);
    m_printReportsCheck->setVisible(!saving);
    formLayout->addRow(QString(), m_printReportsCheck);

    auto* outputDirectoryLayout = new QHBoxLayout;
    m_outputDirectoryEdit = new QLineEdit(this);
    m_outputDirectoryEdit->setText(defaultOutputDirectory);
    m_chooseDirectoryButton = new TextFitPushButton(tr("Choose…"), this);
    outputDirectoryLayout->addWidget(m_outputDirectoryEdit, 1);
    outputDirectoryLayout->addWidget(m_chooseDirectoryButton);
    formLayout->addRow(tr("Output Folder:"), outputDirectoryLayout);
    formLayout->labelForField(outputDirectoryLayout)->setVisible(saving);
    m_outputDirectoryEdit->setVisible(saving);
    m_chooseDirectoryButton->setVisible(saving);

    m_openOutputFolderCheck = new QCheckBox(
        tr("Open Output Folder after saving"),
        this
        );
    m_openOutputFolderCheck->setChecked(false);
    m_openOutputFolderCheck->setVisible(saving);
    formLayout->addRow(QString(), m_openOutputFolderCheck);

    layout->addLayout(formLayout);
    layout->addSpacing(16);

    auto* buttons = new QHBoxLayout;
    m_previewButton = new TextFitPushButton(tr("Preview Reports"), this);
    buttons->addWidget(m_previewButton);
    buttons->addStretch();
    m_exportButton = new TextFitPushButton(
        saving ? tr("Save As...") : tr("Print"),
        this
        );
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
            tr("This is the recommended method for creating reports.")
            ),
        rendererNoteHeightForText(
#ifdef Q_OS_MACOS
            tr("Only use if the internal method doesn't work. This may take longer to complete, and you will likely be presented with multiple file and folder permission requests. Save any work you are presently doing in PowerPoint as there is a risk of your progress being lost.")
#else
            tr("Only use if the internal method doesn't work. Save any work you are presently doing in PowerPoint as there is a risk of your progress being lost.")
#endif
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
    m_savePdfCheck->setText(oneReport ? tr("Export PDF") : tr("Export ZIP"));
    m_keepIndividualPdfsCheck->setVisible(
        m_mode == Mode::SaveAs && !oneReport
        );
    m_keepIndividualPdfsCheck->setEnabled(saving && !oneReport);
    m_printReportsCheck->setText(oneReport ? tr("Print Report") : tr("Print Reports"));
    if (m_mode == Mode::SaveAs)
    {
        m_exportButton->setText(tr("Save As..."));
    }
    else
    {
        m_exportButton->setText(tr("Print"));
    }

    if (selectedRenderer()
        == SpeakingEvalBatchReportService::Renderer::Internal)
    {
        m_rendererNote->setText(
            tr("This is the recommended method for creating reports.")
            );
        return;
    }

    m_rendererNote->setText(
#ifdef Q_OS_MACOS
        tr("Only use if the internal method doesn't work. This may take longer to complete, and you will likely be presented with multiple file and folder permission requests. Save any work you are presently doing in PowerPoint as there is a risk of your progress being lost.")
#else
        tr("Only use if the internal method doesn't work. Save any work you are presently doing in PowerPoint as there is a risk of your progress being lost.")
#endif
        );
}

void SpeakingEvalBatchExportDialog::chooseOutputDirectory()
{
    const std::optional<QString> selection =
        DialogServices::fileDialogs().selectDirectory(
            DirectoryRequest{
                .parent = this,
                .title = tr("Choose Output Folder"),
                .purpose = FileDialogPurpose::ExportReport,
                .initialDirectory = m_outputDirectoryEdit->text()
            }
            );

    if (selection)
    {
        m_outputDirectoryEdit->setText(*selection);
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
    request.keepIndividualPdfFiles =
        reports.size() > 1
        && m_keepIndividualPdfsCheck->isChecked();
    request.outputDirectory = m_outputDirectoryEdit->text().trimmed();

    if (request.savePdf)
    {
        QDir outputDirectory(request.outputDirectory);
        if (request.outputDirectory.isEmpty()
            || (!outputDirectory.exists()
                && !QDir().mkpath(request.outputDirectory)))
        {
            DialogServices::showWarning(
                this,
                tr("Output Folder Unavailable"),
                tr("The selected output folder does not exist and could not be created. Choose another folder and try again.")
                );
            return;
        }

        QStringList existingFiles;
        if (reports.size() > 1)
        {
            const QString archivePath =
                SpeakingEvalBatchReportService::batchArchivePath(
                    request.outputDirectory
                    );
            if (QFileInfo::exists(archivePath))
            {
                existingFiles.append(
                    QFileInfo(archivePath).fileName()
                    );
            }
        }
        if (reports.size() == 1
            || request.keepIndividualPdfFiles)
        {
            for (int index = 0; index < reports.size(); ++index)
            {
                const QString path = outputDirectory.filePath(
                    SpeakingEvalBatchReportService::safeFileName(
                        reports.at(index).report.englishName,
                        reports.at(index).report.koreanName
                        )
                    );
                if (QFileInfo::exists(path))
                {
                    existingFiles.append(
                        QFileInfo(path).fileName()
                        );
                }
            }
        }

        if (!existingFiles.isEmpty())
        {
            const PromptChoice overwrite = DialogServices::confirm(
                this,
                tr("Overwrite Existing Output?"),
                tr("%1 existing output file(s) will be replaced. Do you want to overwrite them?")
                    .arg(existingFiles.size()),
                tr("Overwrite"),
                tr("Cancel"),
                true
                );
            if (overwrite != PromptChoice::Destructive)
            {
                return;
            }
            request.overwriteExisting = true;
        }
    }

    if (request.renderer
            == SpeakingEvalBatchReportService::Renderer::PowerPoint
        && !confirmPowerPointDataAccess())
    {
        return;
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
        const PromptChoice retry =
            DialogServices::confirm(
                this,
                tr("Internal Export Failed"),
                tr("The internal renderer could not create this batch. No PDFs were saved or printed.\n\n%1\n\nRetry the entire batch using the PowerPoint template?")
                    .arg(result.message),
                tr("Retry with PowerPoint"),
                tr("Cancel")
                );
        if (retry == PromptChoice::Accepted)
        {
            if (!confirmPowerPointDataAccess())
            {
                return;
            }
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
    {
        QString successMessage;
        if (!request.savePdf)
        {
            successMessage =
                tr("%1 report(s) were sent to the printer.")
                    .arg(reports.size());
        }
        else if (!result.savedArchivePath.isEmpty())
        {
            successMessage =
                tr("%1 report(s) were saved to %2.")
                    .arg(
                        reports.size()
                        )
                    .arg(
                        QFileInfo(result.savedArchivePath).fileName()
                        );
            if (request.keepIndividualPdfFiles)
            {
                successMessage +=
                    QStringLiteral("\n\n")
                    + tr("Individual PDF files were also saved.");
            }
            if (request.printReports)
            {
                successMessage +=
                    QStringLiteral("\n\n")
                    + tr("The reports were also sent to the printer.");
            }
        }
        else
        {
            successMessage =
                tr("%1 report(s) were saved%2.")
                    .arg(reports.size())
                    .arg(
                        request.printReports
                            ? tr(" and sent to the printer")
                            : QString()
                        );
        }
        DialogServices::showInformation(
            this,
            tr("Reports Ready"),
            successMessage
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
    }
    case SpeakingEvalBatchReportService::Status::Canceled:
        return;
    case SpeakingEvalBatchReportService::Status::Failed:
    case SpeakingEvalBatchReportService::Status::InternalRendererFailed:
        DialogServices::showWarning(
            this,
            tr("Report Export Failed"),
            result.message
            );
        return;
    }
}

bool SpeakingEvalBatchExportDialog::confirmPowerPointDataAccess()
{
#ifdef Q_OS_MACOS
    if (!SettingsManager::instance()
            .showPowerPointDataAccessNotice())
    {
        return true;
    }

    return DialogServices::confirm(
        this,
        tr("PowerPoint Data Access"),
        tr(
            "To generate these reports, ClassMngr temporarily copies the PowerPoint template and signature into Microsoft PowerPoint's protected data folder. macOS may ask ClassMngr to access data from other apps.\n\nChoose Allow in the macOS prompt. Temporary files are deleted when the batch finishes."
            ),
        tr("Continue"),
        tr("Cancel")
        ) == PromptChoice::Accepted;
#else
    return true;
#endif
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
