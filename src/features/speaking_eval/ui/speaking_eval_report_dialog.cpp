#include "speaking_eval_report_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "classmngr/engine/speaking_evaluation_report_content.h"
#include "classmngr/engine/speaking_evaluation_report_model.h"

#include "features/speaking_eval/services/speaking_eval_ai_prompt.h"
#include "features/speaking_eval/ui/speaking_eval_private_notes_editor.h"
#include "core/settingsmanager.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/state/ai_comment_options.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <array>
#include <vector>

QString speakingEvalReportDate(
    const QDate& date,
    SpeakingEvalReportTemplate reportTemplate
    )
{
    const auto portableTemplate =
        reportTemplate == SpeakingEvalReportTemplate::Advanced
            ? classmngr::engine::SpeakingEvaluationReportTemplate::Advanced
            : classmngr::engine::SpeakingEvaluationReportTemplate::Standard;
    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportModel::reportDate(
            date.year(),
            static_cast<unsigned>(date.month()),
            portableTemplate
            )
        );
}

namespace
{

AiCommentProvider preferredAiCommentProvider()
{
    const int storedValue =
        SettingsManager::instance()
            .get(
                QString::fromUtf8(
                    OptionKeys::AiCommentProvider
                    ),
                std::to_underlying(
                    AiCommentProvider::ChatGPT
                    )
                )
            .toInt();

    switch (static_cast<AiCommentProvider>(storedValue))
    {
    case AiCommentProvider::ChatGPT:
    case AiCommentProvider::Gemini:
    case AiCommentProvider::Claude:
    case AiCommentProvider::MicrosoftCopilot:
    case AiCommentProvider::CustomWebsite:
        return static_cast<AiCommentProvider>(storedValue);
    }

    return AiCommentProvider::ChatGPT;
}

AiCommentVoice preferredAiCommentVoice()
{
    const int storedValue =
        SettingsManager::instance()
            .get(
                QString::fromUtf8(
                    OptionKeys::AiCommentVoice
                    ),
                std::to_underlying(
                    AiCommentVoice::DirectToStudent
                    )
                )
            .toInt();

    return static_cast<AiCommentVoice>(storedValue)
            == AiCommentVoice::ThirdPerson
        ? AiCommentVoice::ThirdPerson
        : AiCommentVoice::DirectToStudent;
}

QUrl preferredAiCommentProviderUrl()
{
    return aiCommentProviderUrl(
        preferredAiCommentProvider(),
        SettingsManager::instance()
            .get(
                QString::fromUtf8(
                    OptionKeys::AiCommentCustomWebsiteUrl
                    )
                )
            .toString()
        );
}

void copyAiPrompt(
    const QString& prompt
    )
{
    QApplication::clipboard()->setText(prompt);
}

void copyAiPromptAndOpenProvider(
    QWidget* parent,
    const QString& prompt
    )
{
    copyAiPrompt(prompt);
    const QUrl providerUrl =
        preferredAiCommentProviderUrl();
    if (
        providerUrl.isEmpty()
        || !QDesktopServices::openUrl(providerUrl)
        )
    {
        DialogServices::showWarning(
            parent,
            QCoreApplication::translate(
                "SpeakingEvalReportDialog",
                "Unable to Open AI Website"
                ),
            QCoreApplication::translate(
                "SpeakingEvalReportDialog",
                "The AI website could not be opened. "
                "The prompt is still available on the clipboard."
                )
            );
    }
}

class SpeakingEvalAiPromptPreviewDialog final : public DialogShell
{
public:
    explicit SpeakingEvalAiPromptPreviewDialog(
        const QString& prompt,
        QWidget* parent = nullptr
        )
        : DialogShell(QStringLiteral("speakingEvalAiPromptPreview"), parent)
    {
        setWindowTitle(
            QCoreApplication::translate(
                "SpeakingEvalReportDialog",
                "AI Comment Prompt"
                )
            );
        resize(760, 660);

        auto* layout = contentLayout();

        auto* privacyLabel =
            new QLabel(
                QCoreApplication::translate(
                    "SpeakingEvalReportDialog",
                    "The prompt uses STD_NAME instead of the "
                    "student's real name. Review it before sharing."
                    ),
                this
                );
        privacyLabel->setWordWrap(true);
        layout->addWidget(privacyLabel);

        auto* promptEdit =
            new QPlainTextEdit(this);
        promptEdit->setObjectName(
            QStringLiteral("speakingEvalAiPromptPreviewText")
            );
        promptEdit->setPlainText(prompt);
        promptEdit->setReadOnly(true);
        layout->addWidget(promptEdit, 1);

        auto* buttons = addButtonBox(QDialogButtonBox::Close);
        auto* copyButton = buttons->addButton(
            QCoreApplication::translate(
                "SpeakingEvalReportDialog",
                "Copy Prompt"
                ),
            QDialogButtonBox::ActionRole
            );
        copyButton->setObjectName(
            QStringLiteral("speakingEvalAiPromptPreviewCopy")
            );
        auto* copyOpenButton = buttons->addButton(
            QCoreApplication::translate(
                "SpeakingEvalReportDialog",
                "Copy Prompt and Open %1"
                )
                .arg(
                    aiCommentProviderName(
                        preferredAiCommentProvider()
                        )
                    ),
            QDialogButtonBox::ActionRole
            );
        copyOpenButton->setObjectName(
            QStringLiteral("speakingEvalAiPromptPreviewCopyOpen")
            );
        connect(
            copyButton,
            &QPushButton::clicked,
            this,
            [prompt]()
            {
                copyAiPrompt(prompt);
            }
            );
        connect(
            copyOpenButton,
            &QPushButton::clicked,
            this,
            [this, prompt]()
            {
                copyAiPromptAndOpenProvider(
                    this,
                    prompt
                    );
            }
            );
    }
};

classmngr::engine::ClassInfo toPortableReportClassInfo(
    const ClassInfo& source
    )
{
    classmngr::engine::ClassInfo result;
    result.classGrade = source.classGrade.toStdString();
    result.classLevel = source.classLevel.toStdString();
    result.teacherEn = source.teacherEn.toStdString();
    result.teacherKr = source.teacherKr.toStdString();
    return result;
}

classmngr::engine::SpeakingEvaluationReportStudentInput
toPortableReportStudent(
    const QStringList& values
    )
{
    using PortableInput =
        classmngr::engine::SpeakingEvaluationReportStudentInput;

    PortableInput input;
    input.englishName = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
        ).toStdString();
    input.koreanName = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
        ).toStdString();
    input.comments = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::Comments)
        ).toStdString();
    input.notes = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::Notes)
        ).toStdString();

    const std::array<SpeakingEvalColumn, 6> scoreColumns{
        SpeakingEvalColumn::Grammar,
        SpeakingEvalColumn::Pronunciation,
        SpeakingEvalColumn::Fluency,
        SpeakingEvalColumn::Manner,
        SpeakingEvalColumn::Content,
        SpeakingEvalColumn::OverallEffort
    };
    for (std::size_t index = 0; index < scoreColumns.size(); ++index)
    {
        input.scores[index] = values.value(
            SpeakingEval::toInt(scoreColumns[index])
            ).toStdString();
    }

    return input;
}

SpeakingEvalReportData toQtReportData(
    const classmngr::engine::SpeakingEvaluationReportContent& source,
    const QByteArray& signatureImage
    )
{
    SpeakingEvalReportData data;
    data.englishName = QString::fromStdString(source.englishName);
    data.koreanName = QString::fromStdString(source.koreanName);
    data.classLabel = QString::fromStdString(source.classLabel);
    data.nativeTeacher = QString::fromStdString(source.nativeTeacher);
    data.koreanTeacher = QString::fromStdString(source.koreanTeacher);
    data.date = QString::fromStdString(source.date);
    data.comments = QString::fromStdString(source.comments);
    data.notes = QString::fromStdString(source.notes);
    data.grade = source.grade;
    for (std::size_t index = 0; index < source.scores.size(); ++index)
    {
        data.scores[index] = QString::fromStdString(source.scores[index]);
    }
    data.signatureImage = signatureImage;
    data.reportTemplate =
        source.reportTemplate
                == classmngr::engine::SpeakingEvaluationReportTemplate::Advanced
            ? SpeakingEvalReportTemplate::Advanced
            : SpeakingEvalReportTemplate::Standard;
    return data;
}

} // namespace

QList<SpeakingEvalBatchReportService::StudentReport>
buildSpeakingEvalStudentReports(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    const QByteArray& signatureImage
    )
{
    const QDate reportDate = QDate::currentDate();
    std::vector<
        classmngr::engine::SpeakingEvaluationReportStudentInput
        > students;
    students.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& values : rows)
    {
        students.push_back(toPortableReportStudent(values));
    }

    const auto portableReports =
        classmngr::engine::SpeakingEvaluationReportContentService::buildReports(
            students,
            toPortableReportClassInfo(classInfo),
            reportDate.year(),
            static_cast<unsigned>(reportDate.month())
            );

    QList<SpeakingEvalBatchReportService::StudentReport> reports;
    reports.reserve(static_cast<qsizetype>(portableReports.size()));
    for (const auto& source : portableReports)
    {
        reports.append({
            QString::fromStdString(source.displayName),
            toQtReportData(source, signatureImage),
            source.sourceRow
        });
    }
    return reports;
}

int speakingEvalReportIndexForSourceRow(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    int sourceRow
    )
{
    for (int index = 0; index < reports.size(); ++index)
    {
        if (reports.at(index).sourceRow == sourceRow)
        {
            return index;
        }
    }

    return -1;
}

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    QWidget* parent
    )
    : SpeakingEvalReportDialog(
        buildSpeakingEvalStudentReports(rows, classInfo),
        0,
        parent,
        true
        )
{
}

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    const QByteArray& signatureImage,
    QWidget* parent
    )
    : SpeakingEvalReportDialog(
        buildSpeakingEvalStudentReports(
            rows,
            classInfo,
            signatureImage
            ),
        0,
        parent,
        true
        )
{
}

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    int currentStudentIndex,
    QWidget* parent,
    bool interactive
    )
    : DialogShell(QStringLiteral("speakingEvalReport"), parent)
    , m_reports(reports)
    , m_interactive(interactive)
{
    setWindowTitle(tr("Speaking Evaluation Reports"));
    resize(940, 900);

    auto* layout = contentLayout();

    auto* selectorLayout =
        new QHBoxLayout;
    selectorLayout->setContentsMargins(0, 0, 0, 0);

    auto* studentLabel =
        new QLabel(
            tr("Student:"),
            this
            );

    m_studentSelector =
        new QComboBox(this);

    m_studentSelector->setObjectName(
        QStringLiteral("speakingEvalReportStudentSelector")
        );
    m_studentSelector->setSizeAdjustPolicy(
        QComboBox::AdjustToContents
        );

    auto* previousButton =
        new TextFitPushButton(
            tr("Previous"),
            this
            );
    auto* nextButton =
        new TextFitPushButton(
            tr("Next"),
            this
            );

    selectorLayout->addWidget(studentLabel);
    selectorLayout->addWidget(m_studentSelector, 1);
    selectorLayout->addWidget(previousButton);
    selectorLayout->addWidget(nextButton);
    layout->addLayout(selectorLayout);

    m_notesLabel =
        new QLabel(
            tr("Private Notes (not included in the report)"),
            this
            );

    m_notesFields =
        new SpeakingEvalPrivateNotesEditor(this);
    m_notesFields->setEditorHeight(144);

    m_notesLabel->setVisible(m_interactive);
    m_notesFields->setVisible(m_interactive);
    layout->addWidget(m_notesLabel);
    layout->addWidget(m_notesFields);

    auto* aiPromptButtonLayout =
        new QHBoxLayout;
    m_previewAiPromptButton =
        new TextFitPushButton(
            tr("Preview AI Prompt"),
            this
            );
    m_previewAiPromptButton->setObjectName(
        QStringLiteral("speakingEvalPreviewAiPromptButton")
        );
    m_copyOpenAiPromptButton =
        new TextFitPushButton(
            tr("Copy Prompt and Open %1")
                .arg(
                    aiCommentProviderName(
                        preferredAiCommentProvider()
                        )
                    ),
            this
            );
    m_copyOpenAiPromptButton->setObjectName(
        QStringLiteral("speakingEvalCopyOpenAiPromptButton")
        );
    m_previewAiPromptButton->setVisible(m_interactive);
    m_copyOpenAiPromptButton->setVisible(m_interactive);
    aiPromptButtonLayout->addStretch();
    aiPromptButtonLayout->addWidget(m_previewAiPromptButton);
    aiPromptButtonLayout->addWidget(m_copyOpenAiPromptButton);
    layout->addLayout(aiPromptButtonLayout);

    auto* scrollArea =
        new QScrollArea(this);

    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scrollArea->setFrameShape(QFrame::StyledPanel);

    m_report =
        new SpeakingEvalReportWidget(scrollArea);
    m_report->setInteractive(m_interactive);

    scrollArea->setWidget(m_report);
    layout->addWidget(scrollArea, 1);

    auto* buttons = addButtonBox(QDialogButtonBox::Close);
    auto* printButton = buttons->addButton(
        tr("Print"),
        QDialogButtonBox::ActionRole
        );
    printButton->setObjectName(
        QStringLiteral("speakingEvalReportPrintButton")
        );

    auto* saveAsPdfButton = buttons->addButton(
        tr("Save As PDF"),
        QDialogButtonBox::ActionRole
        );
    saveAsPdfButton->setObjectName(
        QStringLiteral("speakingEvalReportSaveAsPdfButton")
        );

    for (int index = 0; index < m_reports.size(); ++index)
    {
        m_studentSelector->addItem(
            m_reports.at(index).displayName,
            index
            );
    }

    const bool hasStudents =
        m_studentSelector->count() > 0;

    if (!hasStudents)
    {
        m_studentSelector->addItem(
            tr("No students available")
            );
    }

    m_studentSelector->setEnabled(hasStudents);
    previousButton->setEnabled(hasStudents);
    nextButton->setEnabled(hasStudents);
    printButton->setEnabled(hasStudents);
    saveAsPdfButton->setEnabled(hasStudents);
    m_notesFields->setEnabled(hasStudents);
    m_report->setInteractive(m_interactive && hasStudents);
    if (hasStudents)
    {
        m_studentSelector->setCurrentIndex(
            qBound(0, currentStudentIndex, m_reports.size() - 1)
            );
    }

    connect(
        m_studentSelector,
        &QComboBox::currentIndexChanged,
        this,
        [this](int)
        {
            updateReport();
        }
        );
    connect(
        previousButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::moveToPreviousStudent
        );
    connect(
        nextButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::moveToNextStudent
        );
    connect(
        m_report,
        &SpeakingEvalReportWidget::scoreEdited,
        this,
        [this](int metricIndex, const QString& score)
        {
            static constexpr std::array<SpeakingEvalColumn, 6> columns{
                SpeakingEvalColumn::Grammar,
                SpeakingEvalColumn::Pronunciation,
                SpeakingEvalColumn::Fluency,
                SpeakingEvalColumn::Manner,
                SpeakingEvalColumn::Content,
                SpeakingEvalColumn::OverallEffort
            };
            const int reportIndex =
                m_studentSelector
                    ? m_studentSelector->currentData().toInt()
                    : -1;
            if (
                !m_interactive
                || reportIndex < 0
                || reportIndex >= m_reports.size()
                || metricIndex < 0
                || metricIndex >= columns.size()
                )
            {
                return;
            }

            m_reports[reportIndex].report.scores[metricIndex] = score;
            emit reportValueEdited(
                m_reports.at(reportIndex).sourceRow,
                columns[metricIndex],
                score
                );
        }
        );
    connect(
        m_report,
        &SpeakingEvalReportWidget::commentsEdited,
        this,
        [this](const QString& comments)
        {
            const int reportIndex =
                m_studentSelector
                    ? m_studentSelector->currentData().toInt()
                    : -1;
            if (
                !m_interactive
                || reportIndex < 0
                || reportIndex >= m_reports.size()
                )
            {
                return;
            }

            m_reports[reportIndex].report.comments = comments;
            emit reportValueEdited(
                m_reports.at(reportIndex).sourceRow,
                SpeakingEvalColumn::Comments,
                comments
                );
        }
        );
    connect(
        m_notesFields,
        &SpeakingEvalPrivateNotesEditor::notesChanged,
        this,
        &SpeakingEvalReportDialog::updatePrivateNotes
        );
    connect(
        m_previewAiPromptButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::previewAiPrompt
        );
    connect(
        m_copyOpenAiPromptButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::copyAiPromptAndOpen
        );
    connect(
        printButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::printCurrentReport
        );
    connect(
        saveAsPdfButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::saveCurrentReportAsPdf
        );
    updateReport();
}

const SpeakingEvalBatchReportService::StudentReport*
SpeakingEvalReportDialog::currentReport() const
{
    if (!m_studentSelector)
    {
        return nullptr;
    }

    const int reportIndex =
        m_studentSelector->currentData().toInt();
    if (reportIndex < 0 || reportIndex >= m_reports.size())
    {
        return nullptr;
    }

    return &m_reports.at(reportIndex);
}

void SpeakingEvalReportDialog::updateReport()
{
    if (!m_report || !m_studentSelector)
    {
        return;
    }

    const int row =
        m_studentSelector->currentData().toInt();

    if (row < 0 || row >= m_reports.size())
    {
        m_report->setReportData({});
        if (m_notesFields)
        {
            m_notesFields->setNotes({});
        }
        updateAiPromptActions();
        return;
    }

    m_report->setReportData(
        m_reports.at(row).report
        );

    if (m_notesFields)
    {
        m_notesFields->setNotes(
            m_reports.at(row).report.notes
            );
    }
    updateAiPromptActions();
}

void SpeakingEvalReportDialog::updatePrivateNotes()
{
    const int reportIndex =
        m_studentSelector
            ? m_studentSelector->currentData().toInt()
            : -1;
    if (
        !m_interactive
        || !m_notesFields
        || reportIndex < 0
        || reportIndex >= m_reports.size()
        )
    {
        return;
    }

    const QString notes =
        m_notesFields->notes();
    m_reports[reportIndex].report.notes = notes;
    emit reportValueEdited(
        m_reports.at(reportIndex).sourceRow,
        SpeakingEvalColumn::Notes,
        notes
        );
    updateAiPromptActions();
}

QString SpeakingEvalReportDialog::currentAiPrompt() const
{
    const auto* report =
        currentReport();
    if (!report || !m_notesFields)
    {
        return {};
    }

    SpeakingEvalAiPromptInput input;
    input.grade = report->report.grade;
    input.englishName =
        report->report.englishName;
    input.koreanName =
        report->report.koreanName;
    input.didWell =
        m_notesFields->didWellNotes();
    input.needsImprovement =
        m_notesFields->needsImprovementNotes();
    input.voice =
        preferredAiCommentVoice();
    return buildSpeakingEvalAiCommentPrompt(input);
}

QString SpeakingEvalReportDialog::aiPromptUnavailableReason() const
{
    const auto* report =
        currentReport();
    if (!report)
    {
        return tr("Select a student to create an AI prompt.");
    }
    if (
        report->report.englishName.trimmed().isEmpty()
        && report->report.koreanName.trimmed().isEmpty()
        )
    {
        return tr("Enter the student's name to create an AI prompt.");
    }
    if (
        report->report.grade < 4
        || report->report.grade > 6
        )
    {
        return tr("AI prompts are available for grades E4 through E6.");
    }
    if (
        !m_notesFields
        || speakingEvalAiObservationItems(
            m_notesFields->didWellNotes()
            ).isEmpty()
        )
    {
        return tr("Add at least one Did Well note.");
    }
    if (
        speakingEvalAiObservationItems(
            m_notesFields->needsImprovementNotes()
            ).isEmpty()
        )
    {
        return tr("Add at least one Needs Improvement note.");
    }
    return {};
}

void SpeakingEvalReportDialog::updateAiPromptActions()
{
    const QString reason =
        aiPromptUnavailableReason();
    const bool enabled =
        m_interactive && reason.isEmpty();
    for (
        QPushButton* button :
        {
            m_previewAiPromptButton,
            m_copyOpenAiPromptButton
        }
        )
    {
        if (!button)
        {
            continue;
        }
        button->setEnabled(enabled);
        button->setToolTip(reason);
    }
}

void SpeakingEvalReportDialog::previewAiPrompt()
{
    const QString prompt =
        currentAiPrompt();
    if (prompt.isEmpty())
    {
        updateAiPromptActions();
        return;
    }

    SpeakingEvalAiPromptPreviewDialog dialog(
        prompt,
        this
        );
    dialog.exec();
}

void SpeakingEvalReportDialog::copyAiPromptAndOpen()
{
    const QString prompt =
        currentAiPrompt();
    if (prompt.isEmpty())
    {
        updateAiPromptActions();
        return;
    }

    copyAiPromptAndOpenProvider(
        this,
        prompt
        );
}

void SpeakingEvalReportDialog::moveToPreviousStudent()
{
    if (!m_studentSelector || m_studentSelector->count() < 2)
    {
        return;
    }

    const int previousIndex =
        (m_studentSelector->currentIndex() - 1 + m_studentSelector->count())
        % m_studentSelector->count();

    m_studentSelector->setCurrentIndex(previousIndex);
}

void SpeakingEvalReportDialog::moveToNextStudent()
{
    if (!m_studentSelector || m_studentSelector->count() < 2)
    {
        return;
    }

    m_studentSelector->setCurrentIndex(
        (m_studentSelector->currentIndex() + 1)
        % m_studentSelector->count()
        );
}

void SpeakingEvalReportDialog::printCurrentReport()
{
    const SpeakingEvalBatchReportService::StudentReport* selectedReport =
        currentReport();
    if (!selectedReport)
    {
        return;
    }

    SpeakingEvalBatchReportService::Request request;
    request.parent = this;
    request.reports = { *selectedReport };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::Internal;
    request.printReports = true;

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    if (result.status
        == SpeakingEvalBatchReportService::Status::Failed
        || result.status
            == SpeakingEvalBatchReportService::Status::InternalRendererFailed)
    {
        DialogServices::showWarning(
            this,
            tr("Report Printing Failed"),
            result.message
            );
    }
}

void SpeakingEvalReportDialog::saveCurrentReportAsPdf()
{
    const SpeakingEvalBatchReportService::StudentReport* selectedReport =
        currentReport();
    if (!selectedReport)
    {
        return;
    }

    const QString suggestedFileName =
        SpeakingEvalBatchReportService::safeFileName(
            selectedReport->report.englishName,
            selectedReport->report.koreanName
            );

    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = this,
                .title = tr("Save Speaking Evaluation Report As"),
                .purpose = FileDialogPurpose::ExportReport,
                .suggestedFileName = suggestedFileName,
                .nameFilters = {tr("PDF Documents (*.pdf)")},
                .defaultSuffix = QStringLiteral("pdf")
            }
            );

    if (!selection)
    {
        return;
    }

    QString savePath = *selection;
    if (QFileInfo(savePath).suffix().isEmpty())
    {
        savePath += QStringLiteral(".pdf");
    }

    SpeakingEvalBatchReportService::Request request;
    request.parent = this;
    request.reports = { *selectedReport };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::Internal;
    request.savePdf = true;
    request.overwriteExisting = true;
    request.outputFilePath = savePath;

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    if (result.status
        == SpeakingEvalBatchReportService::Status::Failed
        || result.status
            == SpeakingEvalBatchReportService::Status::InternalRendererFailed)
    {
        DialogServices::showWarning(
            this,
            tr("Save PDF Failed"),
            result.message
            );
    }
}
