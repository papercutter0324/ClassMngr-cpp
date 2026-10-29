#include "next/ui/speaking_evaluation_reports_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"
#include "features/speaking_eval/ui/speaking_eval_private_notes_editor.h"
#include "features/speaking_eval/services/speaking_eval_report_output.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
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
#include <QScrollArea>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace ClassMngr::Next::Ui
{
namespace
{
namespace Service = SpeakingEvalBatchReportService;
QString reportText(const char* text) { return QCoreApplication::translate("SpeakingEvalReportDialog", text); }
QString batchText(const char* text) { return QCoreApplication::translate("SpeakingEvalBatchExportDialog", text); }
QPushButton* button(const QString& text, const QString& name, QWidget* parent)
{
    auto* result = new QPushButton(text, parent);
    result->setObjectName(name);
    result->setMinimumWidth(result->sizeHint().width());
    return result;
}
void notice(const SpeakingEvaluationReportActions& actions, QWidget* parent, const DocumentOutputResult& output)
{
    if (output.status == DocumentOutputStatus::Canceled && output.savedPdfPaths.isEmpty()
        && output.savedArchivePath.isEmpty()) return;
    const bool failed = output.status == DocumentOutputStatus::Failed
        || output.status == DocumentOutputStatus::InternalRendererFailed;
    const QString title = batchText(failed ? "Report Export Failed" : "Reports Ready");
    QString message = output.message;
    QStringList saved = output.savedPdfPaths;
    if (!output.savedArchivePath.isEmpty()) saved.append(output.savedArchivePath);
    if (!saved.isEmpty()) message += QStringLiteral("\n\n") + saved.join(QLatin1Char('\n'));
    if (actions.showNotice) actions.showNotice(parent, failed, title, message);
    else if (failed) QMessageBox::warning(parent, title, message);
    else QMessageBox::information(parent, title, message);
}
Output::SpeakingEvaluationReportRunResult run(QWidget* parent, Service::Request request,
    const SpeakingEvaluationReportActions& actions)
{
    QProgressDialog progress(batchText("Creating speaking-evaluation reports…"), batchText("Cancel"),
        0, request.reports.size(), parent);
    progress.setWindowTitle(batchText("Exporting Reports"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    const auto suppliedProgress = request.progressCallback;
    request.progressCallback = [&](int completed, int total, const QString& name) {
        progress.setMaximum(total); progress.setValue(completed);
        if (!name.isEmpty()) progress.setLabelText(QObject::tr("Creating report for %1…").arg(name));
        QCoreApplication::processEvents();
        return !progress.wasCanceled() && (!suppliedProgress || suppliedProgress(completed, total, name));
    };
    auto callbacks = actions.run;
    if (!callbacks.confirmPowerPointRetry) callbacks.confirmPowerPointRetry = [parent](const QString& message) {
        return QMessageBox::question(parent, batchText("Internal Export Failed"),
            batchText("The internal renderer could not create this batch. No PDFs were saved or printed.\n\n%1\n\nRetry the entire batch using the PowerPoint template?").arg(message),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
    };
    if (!callbacks.confirmPowerPointDataAccess) callbacks.confirmPowerPointDataAccess = [parent, &actions] {
#ifdef Q_OS_MACOS
        if (actions.showPowerPointDataAccessNotice)
            return QMessageBox::question(parent, batchText("PowerPoint Data Access"),
                batchText("To generate these reports, ClassMngr temporarily copies the PowerPoint template and signature into Microsoft PowerPoint's protected data folder. macOS may ask ClassMngr to access data from other apps.\n\nChoose Allow in the macOS prompt. Temporary files are deleted when the batch finishes."),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Ok;
#endif
        return true;
    };
    SpeakingEvalReportAssetSession session(actions.assets);
    if (!session.isValid()) return {{DocumentOutputStatus::Failed, batchText("The report resources are busy.")}, {}, 0};
    auto result = Output::runSpeakingEvaluationReports(std::move(request), callbacks);
    progress.setValue(progress.maximum());
    return result;
}
}

SpeakingEvaluationReportsDialog::SpeakingEvaluationReportsDialog(QList<Service::StudentReport> reports,
    int current, SpeakingEvaluationReportActions actions, QWidget* parent, bool interactive)
    : QDialog(parent), m_reports(std::move(reports)), m_actions(std::move(actions)),
      m_assets(std::make_unique<SpeakingEvalReportAssetSession>(m_actions.assets))
{
    setObjectName(QStringLiteral("nextSpeakingEvaluationReports"));
    setWindowTitle(reportText("Speaking Evaluation Reports")); resize(940, 900);
    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(reportText("Student:"), this));
    m_students = new QComboBox(this); m_students->setObjectName(QStringLiteral("nextReportStudent"));
    for (const auto& report : m_reports) m_students->addItem(report.displayName);
    row->addWidget(m_students, 1);
    auto* previous = button(reportText("Previous"), QStringLiteral("nextReportPrevious"), this);
    auto* next = button(reportText("Next"), QStringLiteral("nextReportNext"), this);
    row->addWidget(previous); row->addWidget(next); layout->addLayout(row);
    auto* notesLabel = new QLabel(reportText("Private Notes (not included in the report)"), this);
    notesLabel->setVisible(interactive); layout->addWidget(notesLabel);
    m_notes = new SpeakingEvalPrivateNotesEditor(this); m_notes->setEditorHeight(88);
    m_notes->setVisible(interactive); layout->addWidget(m_notes);
    auto* aiRow = new QHBoxLayout;
    auto* previewAi = button(reportText("Preview AI Prompt"), QStringLiteral("nextReportAiPrompt"), this);
    auto* openAi = button(reportText("Copy Prompt and Open %1").arg(QStringLiteral("AI Website")), QStringLiteral("nextReportAiOpen"), this);
    previewAi->setVisible(interactive && bool(m_actions.previewAiPrompt));
    openAi->setVisible(interactive && bool(m_actions.copyAndOpenAiPrompt));
    aiRow->addStretch(); aiRow->addWidget(previewAi); aiRow->addWidget(openAi); layout->addLayout(aiRow);
    auto* scroll = new QScrollArea(this); scroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scroll->setWidgetResizable(false);
    m_report = new SpeakingEvalReportWidget(scroll); m_report->setInteractive(interactive);
    scroll->setWidget(m_report); layout->addWidget(scroll, 1);
    auto* box = new QDialogButtonBox(this);
    auto* close = box->addButton(
        reportText("Close"), QDialogButtonBox::RejectRole);
    close->setObjectName(QStringLiteral("nextReportClose"));
    auto* print = box->addButton(reportText("Print"), QDialogButtonBox::ActionRole);
    print->setObjectName(QStringLiteral("nextReportPrint"));
    auto* save = box->addButton(reportText("Save As PDF"), QDialogButtonBox::ActionRole);
    save->setObjectName(QStringLiteral("nextReportSavePdf")); layout->addWidget(box);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_students, &QComboBox::currentIndexChanged, this, [this] { updateReport(); });
    connect(previous, &QPushButton::clicked, this, [this] { if (m_students->count()) m_students->setCurrentIndex((m_students->currentIndex() + m_students->count() - 1) % m_students->count()); });
    connect(next, &QPushButton::clicked, this, [this] { if (m_students->count()) m_students->setCurrentIndex((m_students->currentIndex() + 1) % m_students->count()); });
    connect(m_report, &SpeakingEvalReportWidget::scoreEdited, this, [this](int metric, const QString& score) { edit(metric + 3, score); });
    connect(m_report, &SpeakingEvalReportWidget::commentsEdited, this, [this](const QString& comments) { edit(9, comments); });
    connect(m_notes, &SpeakingEvalPrivateNotesEditor::notesChanged, this, [this] { edit(10, m_notes->notes()); });
    connect(save, &QPushButton::clicked, this, [this] { saveCurrent(); });
    connect(print, &QPushButton::clicked, this, [this] { printCurrent(); });
    connect(previewAi, &QPushButton::clicked, this, [this] { if (m_actions.previewAiPrompt && m_students->currentIndex() >= 0) m_actions.previewAiPrompt(m_reports.at(m_students->currentIndex()).sourceRow); });
    connect(openAi, &QPushButton::clicked, this, [this] { if (m_actions.copyAndOpenAiPrompt && m_students->currentIndex() >= 0) m_actions.copyAndOpenAiPrompt(m_reports.at(m_students->currentIndex()).sourceRow); });
    const bool valid = !m_reports.isEmpty() && m_reports.size() <= 25 && m_assets->isValid();
    m_students->setEnabled(valid); save->setEnabled(valid); print->setEnabled(valid);
    previous->setEnabled(valid); next->setEnabled(valid); m_notes->setEnabled(valid);
    if (valid) m_students->setCurrentIndex(std::clamp(current, 0, int(m_reports.size()) - 1));
    updateReport();
}
SpeakingEvaluationReportsDialog::~SpeakingEvaluationReportsDialog() { delete m_report; m_report = nullptr; }
void SpeakingEvaluationReportsDialog::updateReport()
{
    const int index = m_students->currentIndex();
    if (index < 0 || index >= m_reports.size()) return;
    const QSignalBlocker block(m_notes);
    m_report->setReportData(m_reports.at(index).report);
    m_notes->setNotes(m_reports.at(index).report.notes);
}
void SpeakingEvaluationReportsDialog::edit(int column, const QString& value)
{
    const int index = m_students->currentIndex();
    if (index < 0 || index >= m_reports.size()) return;
    auto& report = m_reports[index];
    if (column >= 3 && column <= 8) report.report.scores[column - 3] = value;
    else if (column == 9) report.report.comments = value;
    else if (column == 10) report.report.notes = value;
    if (m_actions.valueEdited) m_actions.valueEdited(report.sourceRow, column, value);
}
void SpeakingEvaluationReportsDialog::saveCurrent()
{
    const int index = m_students->currentIndex(); if (index < 0) return;
    const QString suggested = Service::safeFileName(m_reports.at(index).report.englishName, m_reports.at(index).report.koreanName);
    QString path = m_actions.choosePdfPath ? m_actions.choosePdfPath(this, suggested)
        : QFileDialog::getSaveFileName(this, reportText("Save As PDF"), suggested, QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) path += QStringLiteral(".pdf");
    bool overwrite = false;
    if (QFileInfo::exists(path))
    {
        overwrite = m_actions.confirmOverwrite ? m_actions.confirmOverwrite(this, 1)
            : QMessageBox::question(this, batchText("Overwrite Existing Output?"), batchText("%1 existing output file(s) will be replaced. Do you want to overwrite them?").arg(1)) == QMessageBox::Yes;
        if (!overwrite) return;
    }
    auto request = m_actions.engineOverrides;
    request.parent = this; request.reports = {m_reports.at(index)}; request.savePdf = true;
    request.printReports = false; request.outputFilePath = path; request.overwriteExisting = overwrite;
    notice(m_actions, this, run(this, std::move(request), m_actions).output);
}
void SpeakingEvaluationReportsDialog::printCurrent()
{
    const int index = m_students->currentIndex(); if (index < 0) return;
    auto request = m_actions.engineOverrides;
    request.parent = this; request.reports = {m_reports.at(index)}; request.savePdf = false; request.printReports = true;
    notice(m_actions, this, run(this, std::move(request), m_actions).output);
}

SpeakingEvaluationReportBatchDialog::SpeakingEvaluationReportBatchDialog(const QList<Service::StudentReport>& reports,
    int current, QString directory, Mode mode, SpeakingEvaluationReportActions actions, QWidget* parent)
    : QDialog(parent), m_reports(reports), m_current(current), m_mode(mode), m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextSpeakingEvaluationBatchReports"));
    setWindowTitle(batchText(mode == Mode::SaveAs ? "Save Speaking Reports As" : "Print Speaking Reports"));
    resize(680, 420);
    auto* layout = new QVBoxLayout(this); auto* form = new QFormLayout;
    auto* intro = new QLabel(batchText(mode == Mode::SaveAs ? "Save speaking-evaluation reports to files." : "Print speaking-evaluation reports."), this);
    intro->setWordWrap(true); layout->addWidget(intro);
    m_scope = new QComboBox(this); m_scope->setObjectName(QStringLiteral("nextReportScope"));
    m_scope->addItem(batchText("Current Student")); m_scope->addItem(batchText("All Students (%1)").arg(reports.size())); m_scope->setCurrentIndex(1);
    form->addRow(batchText("Students:"), m_scope);
    m_renderer = new QComboBox(this); m_renderer->setObjectName(QStringLiteral("nextReportRenderer"));
    m_renderer->addItem(Service::rendererDisplayName(Service::Renderer::Internal));
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    m_renderer->addItem(Service::rendererDisplayName(Service::Renderer::PowerPoint));
#endif
    form->addRow(batchText("Renderer:"), m_renderer);
    m_note = new QLabel(this); m_note->setWordWrap(true); form->addRow(QString(), m_note);
    m_keep = new QCheckBox(batchText("Keep individual PDFs after zipping"), this);
    m_keep->setObjectName(QStringLiteral("nextReportKeepPdfs")); form->addRow(QString(), m_keep);
    auto* folderRow = new QHBoxLayout;
    m_directory = new QLineEdit(directory, this); m_directory->setObjectName(QStringLiteral("nextReportDirectory"));
    folderRow->addWidget(m_directory); auto* choose = button(batchText("Choose…"), QStringLiteral("nextReportChooseDirectory"), this); folderRow->addWidget(choose);
    form->addRow(batchText("Output Folder:"), folderRow);
    form->labelForField(folderRow)->setVisible(mode == Mode::SaveAs);
    m_directory->setVisible(mode == Mode::SaveAs); choose->setVisible(mode == Mode::SaveAs);
    m_open = new QCheckBox(batchText("Open Output Folder after saving"), this);
    m_open->setObjectName(QStringLiteral("nextReportOpenOutput"));
    m_open->setVisible(mode == Mode::SaveAs); form->addRow(QString(), m_open);
    layout->addLayout(form); m_status = new QLabel(this); m_status->setWordWrap(true); layout->addWidget(m_status);
    auto* buttons = new QDialogButtonBox(this);
    auto* cancel = buttons->addButton(
        batchText("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setObjectName(QStringLiteral("nextReportBatchCancel"));
    auto* preview = buttons->addButton(batchText("Preview Reports"), QDialogButtonBox::ActionRole); preview->setObjectName(QStringLiteral("nextReportBatchPreview"));
    m_export = buttons->addButton(batchText(mode == Mode::SaveAs ? "Save As..." : "Print"), QDialogButtonBox::ActionRole); m_export->setObjectName(QStringLiteral("nextReportBatchExport"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this] { updateControls(); });
    connect(m_renderer, &QComboBox::currentIndexChanged, this, [this] { updateControls(); });
    connect(choose, &QPushButton::clicked, this, [this] {
        const QString folder = m_actions.chooseOutputDirectory ? m_actions.chooseOutputDirectory(this, m_directory->text())
            : QFileDialog::getExistingDirectory(this, batchText("Choose Output Folder"), m_directory->text());
        if (!folder.isEmpty()) m_directory->setText(folder);
    });
    connect(preview, &QPushButton::clicked, this, [this] {
        auto selected = selectedReports(); if (selected.isEmpty()) return;
        SpeakingEvaluationReportsDialog dialog(std::move(selected), m_scope->currentIndex() == 1 ? m_current : 0, m_actions, this, false); dialog.exec();
    });
    connect(m_export, &QPushButton::clicked, this, [this] { exportReports(); });
    preview->setEnabled(!reports.isEmpty() && reports.size() <= 25); updateControls();
}
QList<Service::StudentReport> SpeakingEvaluationReportBatchDialog::selectedReports() const
{
    if (m_scope->currentIndex() == 1) return m_reports; // implicit sharing; no detached report copy
    return m_current >= 0 && m_current < m_reports.size() ? QList<Service::StudentReport>{m_reports.at(m_current)} : QList<Service::StudentReport>{};
}
void SpeakingEvaluationReportBatchDialog::updateControls()
{
    m_keep->setVisible(m_mode == Mode::SaveAs && m_scope->currentIndex() == 1 && m_reports.size() > 1);
    m_export->setEnabled(!selectedReports().isEmpty() && m_reports.size() <= 25);
    m_note->setText(m_renderer->currentIndex() == 0
        ? batchText("This is the recommended method for creating reports.")
#ifdef Q_OS_MACOS
        : batchText("Only use if the internal method doesn't work. This may take longer to complete, and you will likely be presented with multiple file and folder permission requests. Save any work you are presently doing in PowerPoint as there is a risk of your progress being lost."));
#else
        : batchText("Only use if the internal method doesn't work. Save any work you are presently doing in PowerPoint as there is a risk of your progress being lost."));
#endif
    if (layout() && height() < layout()->sizeHint().height())
        resize(width(), layout()->sizeHint().height());
}
void SpeakingEvaluationReportBatchDialog::exportReports()
{
    auto request = m_actions.engineOverrides;
    request.parent = this; request.reports = selectedReports();
    request.renderer = m_renderer->currentIndex() == 0 ? Service::Renderer::Internal : Service::Renderer::PowerPoint;
    request.savePdf = m_mode == Mode::SaveAs; request.printReports = m_mode == Mode::Print;
    request.outputDirectory = m_directory->text().trimmed(); request.outputFilePath.clear();
    request.keepIndividualPdfFiles = request.reports.size() > 1 && m_keep->isChecked();
    if (request.savePdf)
    {
        if (request.outputDirectory.isEmpty() || !QDir().mkpath(request.outputDirectory))
        { m_status->setText(batchText("The selected output folder does not exist and could not be created. Choose another folder and try again.")); return; }
        QStringList targets; QString error;
        const bool overwriteAlreadyAllowed = request.overwriteExisting;
        request.overwriteExisting = true;
        const bool targetsResolved = SpeakingEvalReportOutput::targetFilePaths(
            request, true, &targets, &error);
        request.overwriteExisting = overwriteAlreadyAllowed;
        if (!targetsResolved)
        { m_status->setText(error); return; }
        if (request.reports.size() > 1)
        {
            if (!request.keepIndividualPdfFiles) targets.clear();
            targets.append(Service::batchArchivePath(request.outputDirectory));
        }
        int existing = 0; for (const auto& path : targets) if (QFileInfo::exists(path)) ++existing;
        if (existing)
        {
            request.overwriteExisting = m_actions.confirmOverwrite ? m_actions.confirmOverwrite(this, existing)
                : QMessageBox::question(this, batchText("Overwrite Existing Output?"), batchText("%1 existing output file(s) will be replaced. Do you want to overwrite them?").arg(existing)) == QMessageBox::Yes;
            if (!request.overwriteExisting) return;
        }
    }
    m_lastResult = run(this, std::move(request), m_actions); m_status->setText(m_lastResult.output.message);
    notice(m_actions, this, m_lastResult.output);
    if (m_lastResult.output.succeeded())
    {
        if (m_mode == Mode::SaveAs && m_open->isChecked())
        {
            if (m_actions.openOutputFolder) m_actions.openOutputFolder(m_directory->text());
            else QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory->text()));
        }
        accept();
    }
}
}
