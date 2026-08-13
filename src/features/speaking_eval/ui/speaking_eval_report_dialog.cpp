#include "speaking_eval_report_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "features/speaking_eval/services/speaking_eval_ai_prompt.h"
#include "features/speaking_eval/ui/speaking_eval_private_notes_editor.h"
#include "core/settingsmanager.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/state/ai_comment_options.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <array>

QString speakingEvalReportDate(
    const QDate& date,
    SpeakingEvalReportTemplate reportTemplate
    )
{
    if (reportTemplate == SpeakingEvalReportTemplate::Advanced)
    {
        return date.toString(QStringLiteral("MMM. yyyy"));
    }

    return date.toString(
        date.month() >= 5 && date.month() <= 7
            ? QStringLiteral("MMMM yyyy")
            : QStringLiteral("MMM. yyyy")
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

class SpeakingEvalAiPromptPreviewDialog final : public QDialog
{
public:
    explicit SpeakingEvalAiPromptPreviewDialog(
        const QString& prompt,
        QWidget* parent = nullptr
        )
        : QDialog(parent)
    {
        setObjectName(
            QStringLiteral("speakingEvalAiPromptPreviewDialog")
            );
        setWindowTitle(
            QCoreApplication::translate(
                "SpeakingEvalReportDialog",
                "AI Comment Prompt"
                )
            );
        resize(760, 660);

        auto* layout =
            new QVBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(10);

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

        auto* buttonLayout =
            new QHBoxLayout;
        auto* copyButton =
            new TextFitPushButton(
                QCoreApplication::translate(
                    "SpeakingEvalReportDialog",
                    "Copy Prompt"
                    ),
                this
                );
        copyButton->setObjectName(
            QStringLiteral("speakingEvalAiPromptPreviewCopy")
            );
        auto* copyOpenButton =
            new TextFitPushButton(
                QCoreApplication::translate(
                    "SpeakingEvalReportDialog",
                    "Copy Prompt and Open %1"
                    )
                    .arg(
                        aiCommentProviderName(
                            preferredAiCommentProvider()
                            )
                        ),
                this
                );
        copyOpenButton->setObjectName(
            QStringLiteral("speakingEvalAiPromptPreviewCopyOpen")
            );
        auto* closeButton =
            new TextFitPushButton(
                QCoreApplication::translate(
                    "SpeakingEvalReportDialog",
                    "Close"
                    ),
                this
                );

        buttonLayout->addWidget(copyButton);
        buttonLayout->addWidget(copyOpenButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);

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
        connect(
            closeButton,
            &QPushButton::clicked,
            this,
            &QDialog::accept
            );
    }
};

QString classLabel(
    const ClassInfo& info
    )
{
    QStringList parts;

    if (!info.classGrade.trimmed().isEmpty())
    {
        parts.append(info.classGrade.trimmed());
    }

    if (!info.classLevel.trimmed().isEmpty())
    {
        parts.append(info.classLevel.trimmed());
    }

    return parts.join(QLatin1Char(' '));
}

SpeakingEvalReportTemplate reportTemplateForClass(
    const ClassInfo& info
    )
{
    const QString grade =
        info.classGrade.trimmed();
    const QString level =
        info.classLevel.trimmed();

    const bool usesAdvancedTemplate = (
               grade.compare(
                   QStringLiteral("E5"),
                   Qt::CaseInsensitive
                   ) == 0
               && level.compare(
                   QStringLiteral("Athena"),
                   Qt::CaseInsensitive
                   ) == 0
               )
        || (
               grade.compare(
                   QStringLiteral("E6"),
                   Qt::CaseInsensitive
                   ) == 0
               && level.compare(
                   QStringLiteral("Song's"),
                   Qt::CaseInsensitive
                   ) == 0
               );

    return usesAdvancedTemplate
        ? SpeakingEvalReportTemplate::Advanced
        : SpeakingEvalReportTemplate::Standard;
}

QString studentDisplayName(
    const SpeakingEvalRows& rows,
    int row
    )
{
    if (row < 0 || row >= rows.size())
    {
        return {};
    }

    const QString englishName =
        rows[row].value(
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
            ).trimmed();
    const QString koreanName =
        rows[row].value(
            SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
            ).trimmed();

    if (englishName.isEmpty())
    {
        return koreanName;
    }
    if (koreanName.isEmpty())
    {
        return englishName;
    }

    return QObject::tr("%1 (%2)").arg(englishName, koreanName);
}

SpeakingEvalReportData reportDataForRow(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    int row
    )
{
    SpeakingEvalReportData data;
    if (row < 0 || row >= rows.size())
    {
        return data;
    }

    const QStringList& values = rows[row];
    data.englishName = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
        );
    data.koreanName = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
        );
    data.classLabel = classLabel(classInfo);
    data.nativeTeacher = classInfo.teacherEn;
    data.koreanTeacher = classInfo.teacherKr;
    data.comments = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::Comments)
        );
    data.notes = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::Notes)
        );
    data.grade =
        speakingEvalElementaryGrade(
            classInfo.classGrade
            );
    data.reportTemplate = reportTemplateForClass(classInfo);
    data.date = speakingEvalReportDate(
        QDate::currentDate(),
        data.reportTemplate
        );

    const std::array<SpeakingEvalColumn, 6> scoreColumns{
        SpeakingEvalColumn::Grammar,
        SpeakingEvalColumn::Pronunciation,
        SpeakingEvalColumn::Fluency,
        SpeakingEvalColumn::Manner,
        SpeakingEvalColumn::Content,
        SpeakingEvalColumn::OverallEffort
    };
    for (int index = 0; index < scoreColumns.size(); ++index)
    {
        data.scores[index] = values.value(
            SpeakingEval::toInt(scoreColumns[index])
            );
    }

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
    QList<SpeakingEvalBatchReportService::StudentReport> reports;
    for (int row = 0; row < rows.size(); ++row)
    {
        const QString displayName = studentDisplayName(rows, row);
        if (displayName.isEmpty())
        {
            continue;
        }

        SpeakingEvalReportData report =
            reportDataForRow(rows, classInfo, row);
        report.signatureImage = signatureImage;
        reports.append({ displayName, report, row });
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
    : QDialog(parent)
    , m_reports(reports)
    , m_interactive(interactive)
{
    setWindowTitle(tr("Speaking Evaluation Reports"));
    resize(940, 900);

    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

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

    auto* buttonLayout =
        new QHBoxLayout;

    auto* printButton =
        new TextFitPushButton(
            tr("Print"),
            this
            );
    printButton->setObjectName(
        QStringLiteral("speakingEvalReportPrintButton")
        );

    auto* saveAsPdfButton =
        new TextFitPushButton(
            tr("Save As PDF"),
            this
            );
    saveAsPdfButton->setObjectName(
        QStringLiteral("speakingEvalReportSaveAsPdfButton")
        );

    auto* closeButton =
        new TextFitPushButton(
            tr("Close"),
            this
            );

    buttonLayout->addWidget(printButton);
    buttonLayout->addWidget(saveAsPdfButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

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
    connect(
        closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept
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
