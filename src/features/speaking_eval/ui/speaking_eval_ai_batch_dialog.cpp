#include "speaking_eval_ai_batch_dialog.h"

#include "core/settingsmanager.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/services/speaking_eval_ai_prompt.h"
#include "ui/shared/state/ai_comment_options.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace
{

enum SelectionColumn
{
    SelectionIncludeColumn = 0,
    SelectionStudentColumn,
    SelectionStatusColumn,
    SelectionColumnCount
};

enum ReviewColumn
{
    ReviewApplyColumn = 0,
    ReviewStudentColumn,
    ReviewStatusColumn,
    ReviewCharactersColumn,
    ReviewCommentColumn,
    ReviewColumnCount
};

enum ItemDataRole
{
    ReportIndexRole = Qt::UserRole,
    StudentIdRole,
    HadPlaceholderRole,
    ReviewValidRole
};

struct PrivateNotes
{
    QString didWell;
    QString needsImprovement;
};

PrivateNotes splitPrivateNotes(
    const QString& notes
    )
{
    const QString didWellMarker =
        QStringLiteral("[Did Well]\n");
    const QString needsImprovementMarker =
        QStringLiteral("\n[Needs Improvement]\n");

    if (!notes.startsWith(didWellMarker))
    {
        return { notes, {} };
    }

    const qsizetype separator =
        notes.indexOf(
            needsImprovementMarker,
            didWellMarker.size()
            );
    if (separator < 0)
    {
        return { notes, {} };
    }

    return {
        notes.mid(
            didWellMarker.size(),
            separator - didWellMarker.size()
            ),
        notes.mid(
            separator + needsImprovementMarker.size()
            )
    };
}

AiCommentProvider preferredProvider()
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

AiCommentVoice preferredVoice()
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

QUrl preferredProviderUrl()
{
    return aiCommentProviderUrl(
        preferredProvider(),
        SettingsManager::instance()
            .get(
                QString::fromUtf8(
                    OptionKeys::AiCommentCustomWebsiteUrl
                    )
                )
            .toString()
        );
}

QString studentId(
    int reportIndex
    )
{
    return QStringLiteral("STUDENT_%1")
        .arg(reportIndex + 1, 2, 10, QLatin1Char('0'));
}

QString preferredStudentName(
    const SpeakingEvalBatchReportService::StudentReport& report
    )
{
    const QString englishName =
        report.report.englishName.trimmed();
    return englishName.isEmpty()
        ? report.report.koreanName.trimmed()
        : englishName;
}

QString unavailableReason(
    const SpeakingEvalBatchReportService::StudentReport& report
    )
{
    if (
        report.report.englishName.trimmed().isEmpty()
        && report.report.koreanName.trimmed().isEmpty()
        )
    {
        return QCoreApplication::translate(
            "SpeakingEvalAiBatchDialog",
            "Student name is missing."
            );
    }
    if (report.report.grade < 4 || report.report.grade > 6)
    {
        return QCoreApplication::translate(
            "SpeakingEvalAiBatchDialog",
            "AI comments are available for grades E4 through E6."
            );
    }

    const PrivateNotes notes =
        splitPrivateNotes(report.report.notes);
    if (
        speakingEvalAiObservationItems(
            notes.didWell
            ).isEmpty()
        )
    {
        return QCoreApplication::translate(
            "SpeakingEvalAiBatchDialog",
            "Add at least one Did Well note."
            );
    }
    if (
        speakingEvalAiObservationItems(
            notes.needsImprovement
            ).isEmpty()
        )
    {
        return QCoreApplication::translate(
            "SpeakingEvalAiBatchDialog",
            "Add at least one Needs Improvement note."
            );
    }
    return {};
}

QTableWidgetItem* readOnlyItem(
    const QString& text
    )
{
    auto* item =
        new QTableWidgetItem(text);
    item->setFlags(
        item->flags() & ~Qt::ItemIsEditable
        );
    return item;
}

} // namespace

SpeakingEvalAiBatchDialog::SpeakingEvalAiBatchDialog(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    QWidget* parent
    )
    : QDialog(parent)
    , m_reports(reports)
{
    setObjectName(
        QStringLiteral("speakingEvalAiBatchDialog")
        );
    setWindowTitle(
        tr("Generate Class Comments")
        );
    resize(1100, 820);

    auto* layout =
        new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    auto* tabs =
        new QTabWidget(this);
    tabs->setObjectName(
        QStringLiteral("speakingEvalAiBatchTabs")
        );
    layout->addWidget(tabs, 1);

    auto* promptPage =
        new QWidget(tabs);
    auto* promptLayout =
        new QVBoxLayout(promptPage);
    promptLayout->setContentsMargins(12, 12, 12, 12);
    promptLayout->setSpacing(8);

    auto* selectionLabel =
        new QLabel(
            tr(
                "Select students. Eligible students without comments are "
                "selected automatically."
                ),
            promptPage
            );
    selectionLabel->setWordWrap(true);
    promptLayout->addWidget(selectionLabel);

    m_selectionTable =
        new QTableWidget(
            m_reports.size(),
            SelectionColumnCount,
            promptPage
            );
    m_selectionTable->setObjectName(
        QStringLiteral("speakingEvalAiBatchSelectionTable")
        );
    m_selectionTable->setHorizontalHeaderLabels(
        {
            tr("Include"),
            tr("Student"),
            tr("Status")
        }
        );
    m_selectionTable->verticalHeader()->setVisible(false);
    m_selectionTable->horizontalHeader()->setSectionResizeMode(
        SelectionIncludeColumn,
        QHeaderView::ResizeToContents
        );
    m_selectionTable->horizontalHeader()->setSectionResizeMode(
        SelectionStudentColumn,
        QHeaderView::ResizeToContents
        );
    m_selectionTable->horizontalHeader()->setSectionResizeMode(
        SelectionStatusColumn,
        QHeaderView::Stretch
        );
    m_selectionTable->setSelectionBehavior(
        QAbstractItemView::SelectRows
        );
    m_selectionTable->setMinimumHeight(230);
    promptLayout->addWidget(m_selectionTable);

    for (int row = 0; row < m_reports.size(); ++row)
    {
        const auto& report =
            m_reports.at(row);
        const QString reason =
            unavailableReason(report);
        const bool eligible =
            reason.isEmpty();
        const bool hasComment =
            !report.report.comments.trimmed().isEmpty();

        auto* includeItem =
            new QTableWidgetItem;
        includeItem->setData(
            ReportIndexRole,
            row
            );
        includeItem->setData(
            StudentIdRole,
            studentId(row)
            );
        includeItem->setFlags(
            Qt::ItemIsEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsUserCheckable
            );
        includeItem->setCheckState(
            eligible && !hasComment
                ? Qt::Checked
                : Qt::Unchecked
            );
        if (!eligible)
        {
            includeItem->setFlags(
                includeItem->flags()
                & ~Qt::ItemIsEnabled
                );
        }
        m_selectionTable->setItem(
            row,
            SelectionIncludeColumn,
            includeItem
            );
        m_selectionTable->setItem(
            row,
            SelectionStudentColumn,
            readOnlyItem(report.displayName)
            );
        m_selectionTable->setItem(
            row,
            SelectionStatusColumn,
            readOnlyItem(
                !eligible
                    ? reason
                    : hasComment
                        ? tr("Existing comment — select to regenerate")
                        : tr("Ready")
                )
            );
    }

    m_createPromptButton =
        new TextFitPushButton(
            tr("Create Class Prompt"),
            promptPage
            );
    m_createPromptButton->setObjectName(
        QStringLiteral("speakingEvalAiBatchCreatePrompt")
        );
    promptLayout->addWidget(
        m_createPromptButton,
        0,
        Qt::AlignRight
        );

    auto* privacyLabel =
        new QLabel(
            tr(
                "Real student names are removed from the prompt and are "
                "restored locally after the response is pasted."
                ),
            promptPage
            );
    privacyLabel->setWordWrap(true);
    promptLayout->addWidget(privacyLabel);

    m_promptEdit =
        new QPlainTextEdit(promptPage);
    m_promptEdit->setObjectName(
        QStringLiteral("speakingEvalAiBatchPrompt")
        );
    m_promptEdit->setReadOnly(true);
    m_promptEdit->setPlaceholderText(
        tr("Select students and create the class prompt.")
        );
    promptLayout->addWidget(m_promptEdit, 1);

    auto* promptButtons =
        new QHBoxLayout;
    promptButtons->addStretch();
    m_copyPromptButton =
        new TextFitPushButton(
            tr("Copy Prompt"),
            promptPage
            );
    m_copyPromptButton->setObjectName(
        QStringLiteral("speakingEvalAiBatchCopyPrompt")
        );
    m_copyOpenButton =
        new TextFitPushButton(
            tr("Copy Prompt and Open %1")
                .arg(
                    aiCommentProviderName(
                        preferredProvider()
                        )
                    ),
            promptPage
            );
    m_copyOpenButton->setObjectName(
        QStringLiteral("speakingEvalAiBatchCopyOpen")
        );
    promptButtons->addWidget(m_copyPromptButton);
    promptButtons->addWidget(m_copyOpenButton);
    promptLayout->addLayout(promptButtons);

    tabs->addTab(
        promptPage,
        tr("1. Create Prompt")
        );

    auto* reviewPage =
        new QWidget(tabs);
    auto* reviewLayout =
        new QVBoxLayout(reviewPage);
    reviewLayout->setContentsMargins(12, 12, 12, 12);
    reviewLayout->setSpacing(8);

    auto* responseLabel =
        new QLabel(
            tr("Paste the complete AI response below."),
            reviewPage
            );
    reviewLayout->addWidget(responseLabel);

    m_responseEdit =
        new QPlainTextEdit(reviewPage);
    m_responseEdit->setObjectName(
        QStringLiteral("speakingEvalAiBatchResponse")
        );
    m_responseEdit->setPlaceholderText(
        tr("Paste the response containing the STUDENT blocks here…")
        );
    m_responseEdit->setMaximumHeight(190);
    reviewLayout->addWidget(m_responseEdit);

    m_parseButton =
        new TextFitPushButton(
            tr("Parse Response"),
            reviewPage
            );
    m_parseButton->setObjectName(
        QStringLiteral("speakingEvalAiBatchParse")
        );
    reviewLayout->addWidget(
        m_parseButton,
        0,
        Qt::AlignRight
        );

    m_parseSummary =
        new QLabel(reviewPage);
    m_parseSummary->setObjectName(
        QStringLiteral("speakingEvalAiBatchParseSummary")
        );
    m_parseSummary->setWordWrap(true);
    reviewLayout->addWidget(m_parseSummary);

    m_reviewTable =
        new QTableWidget(
            0,
            ReviewColumnCount,
            reviewPage
            );
    m_reviewTable->setObjectName(
        QStringLiteral("speakingEvalAiBatchReviewTable")
        );
    m_reviewTable->setHorizontalHeaderLabels(
        {
            tr("Apply"),
            tr("Student"),
            tr("Status"),
            tr("Characters"),
            tr("Comment")
        }
        );
    m_reviewTable->verticalHeader()->setVisible(false);
    m_reviewTable->horizontalHeader()->setSectionResizeMode(
        ReviewApplyColumn,
        QHeaderView::ResizeToContents
        );
    m_reviewTable->horizontalHeader()->setSectionResizeMode(
        ReviewStudentColumn,
        QHeaderView::ResizeToContents
        );
    m_reviewTable->horizontalHeader()->setSectionResizeMode(
        ReviewStatusColumn,
        QHeaderView::ResizeToContents
        );
    m_reviewTable->horizontalHeader()->setSectionResizeMode(
        ReviewCharactersColumn,
        QHeaderView::ResizeToContents
        );
    m_reviewTable->horizontalHeader()->setSectionResizeMode(
        ReviewCommentColumn,
        QHeaderView::Stretch
        );
    m_reviewTable->setWordWrap(true);
    reviewLayout->addWidget(m_reviewTable, 1);

    tabs->addTab(
        reviewPage,
        tr("2. Paste and Review")
        );

    auto* buttons =
        new TextFitDialogButtonBox(
            QDialogButtonBox::Cancel,
            this
            );
    m_applyButton =
        new TextFitPushButton(
            tr("Apply Selected Comments"),
            this
            );
    m_applyButton->setObjectName(
        QStringLiteral("speakingEvalAiBatchApply")
        );
    buttons->addButton(
        m_applyButton,
        QDialogButtonBox::AcceptRole
        );
    layout->addWidget(buttons);

    connect(
        m_selectionTable,
        &QTableWidget::itemChanged,
        this,
        [this](QTableWidgetItem* item)
        {
            if (
                item
                && item->column() == SelectionIncludeColumn
                )
            {
                resetGeneratedContent();
                updateCreatePromptButton();
            }
        }
        );
    connect(
        m_createPromptButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalAiBatchDialog::createPrompt
        );
    connect(
        m_copyPromptButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            copyPrompt(false);
        }
        );
    connect(
        m_copyOpenButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            copyPrompt(true);
        }
        );
    connect(
        m_responseEdit,
        &QPlainTextEdit::textChanged,
        this,
        [this]()
        {
            m_parseButton->setEnabled(
                !m_selectedReportIndexes.isEmpty()
                && !m_responseEdit->toPlainText().trimmed().isEmpty()
                );
            m_reviewTable->setRowCount(0);
            m_parseSummary->clear();
            updateApplyButton();
        }
        );
    connect(
        m_parseButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalAiBatchDialog::parseResponse
        );
    connect(
        m_reviewTable,
        &QTableWidget::itemChanged,
        this,
        [this](QTableWidgetItem* item)
        {
            if (!item)
            {
                return;
            }
            if (item->column() == ReviewCommentColumn)
            {
                updateReviewRow(item->row());
            }
            updateApplyButton();
        }
        );
    connect(
        m_applyButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalAiBatchDialog::applyComments
        );
    connect(
        buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );

    updateCreatePromptButton();
    resetGeneratedContent();
}

QList<SpeakingEvalAiBatchAcceptedComment>
SpeakingEvalAiBatchDialog::acceptedComments() const
{
    return m_acceptedComments;
}

void SpeakingEvalAiBatchDialog::resetGeneratedContent()
{
    m_selectedReportIndexes.clear();
    m_promptEdit->clear();
    m_responseEdit->clear();
    m_reviewTable->setRowCount(0);
    m_parseSummary->clear();
    m_copyPromptButton->setEnabled(false);
    m_copyOpenButton->setEnabled(false);
    m_parseButton->setEnabled(false);
    updateApplyButton();
}

void SpeakingEvalAiBatchDialog::updateCreatePromptButton()
{
    bool hasSelection = false;
    for (int row = 0; row < m_selectionTable->rowCount(); ++row)
    {
        const QTableWidgetItem* item =
            m_selectionTable->item(
                row,
                SelectionIncludeColumn
                );
        if (
            item
            && item->checkState() == Qt::Checked
            )
        {
            hasSelection = true;
            break;
        }
    }
    m_createPromptButton->setEnabled(hasSelection);
}

void SpeakingEvalAiBatchDialog::createPrompt()
{
    SpeakingEvalAiBatchPromptInput input;
    input.voice = preferredVoice();

    for (const auto& report : m_reports)
    {
        input.additionalNamesToRedact.append(
            report.report.englishName
            );
        input.additionalNamesToRedact.append(
            report.report.koreanName
            );
    }

    m_selectedReportIndexes.clear();
    for (int row = 0; row < m_selectionTable->rowCount(); ++row)
    {
        const QTableWidgetItem* item =
            m_selectionTable->item(
                row,
                SelectionIncludeColumn
                );
        if (
            !item
            || item->checkState() != Qt::Checked
            )
        {
            continue;
        }

        const int reportIndex =
            item->data(ReportIndexRole).toInt();
        if (
            reportIndex < 0
            || reportIndex >= m_reports.size()
            )
        {
            continue;
        }
        const auto& report =
            m_reports.at(reportIndex);
        const PrivateNotes notes =
            splitPrivateNotes(report.report.notes);
        input.students.append(
            {
                item->data(StudentIdRole).toString(),
                report.report.grade,
                report.report.englishName,
                report.report.koreanName,
                notes.didWell,
                notes.needsImprovement
            }
            );
        m_selectedReportIndexes.append(reportIndex);
    }

    const QString prompt =
        buildSpeakingEvalAiBatchCommentPrompt(input);
    m_promptEdit->setPlainText(prompt);
    const bool ready =
        !prompt.isEmpty();
    m_copyPromptButton->setEnabled(ready);
    m_copyOpenButton->setEnabled(ready);
    m_parseButton->setEnabled(
        ready
        && !m_responseEdit->toPlainText().trimmed().isEmpty()
        );
}

void SpeakingEvalAiBatchDialog::copyPrompt(
    bool openProvider
    )
{
    const QString prompt =
        m_promptEdit->toPlainText();
    if (prompt.isEmpty())
    {
        return;
    }

    QApplication::clipboard()->setText(prompt);
    if (!openProvider)
    {
        return;
    }

    const QUrl url =
        preferredProviderUrl();
    if (
        url.isEmpty()
        || !QDesktopServices::openUrl(url)
        )
    {
        QMessageBox::warning(
            this,
            tr("Unable to Open AI Website"),
            tr(
                "The AI website could not be opened. "
                "The prompt is still available on the clipboard."
                )
            );
    }
}

void SpeakingEvalAiBatchDialog::parseResponse()
{
    QStringList expectedIds;
    for (const int reportIndex : m_selectedReportIndexes)
    {
        expectedIds.append(
            studentId(reportIndex)
            );
    }

    const SpeakingEvalAiBatchParseResult parsed =
        parseSpeakingEvalAiBatchResponse(
            m_responseEdit->toPlainText(),
            expectedIds
            );

    m_reviewTable->setRowCount(
        m_selectedReportIndexes.size()
        );
    const QSignalBlocker blocker(m_reviewTable);
    for (
        int row = 0;
        row < m_selectedReportIndexes.size();
        ++row
        )
    {
        const int reportIndex =
            m_selectedReportIndexes.at(row);
        const auto& report =
            m_reports.at(reportIndex);
        const QString id =
            studentId(reportIndex);

        auto* applyItem =
            new QTableWidgetItem;
        applyItem->setFlags(
            Qt::ItemIsEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsUserCheckable
            );
        applyItem->setCheckState(Qt::Unchecked);
        applyItem->setData(
            ReportIndexRole,
            reportIndex
            );
        m_reviewTable->setItem(
            row,
            ReviewApplyColumn,
            applyItem
            );
        m_reviewTable->setItem(
            row,
            ReviewStudentColumn,
            readOnlyItem(report.displayName)
            );
        m_reviewTable->setItem(
            row,
            ReviewStatusColumn,
            readOnlyItem({})
            );
        m_reviewTable->setItem(
            row,
            ReviewCharactersColumn,
            readOnlyItem({})
            );

        auto* commentItem =
            new QTableWidgetItem;
        commentItem->setData(
            HadPlaceholderRole,
            false
            );

        if (parsed.duplicateIds.contains(id))
        {
            m_reviewTable
                ->item(row, ReviewStatusColumn)
                ->setText(
                    tr("Duplicate response blocks")
                    );
        }
        else if (parsed.malformedIds.contains(id))
        {
            m_reviewTable
                ->item(row, ReviewStatusColumn)
                ->setText(
                    tr("Malformed response block")
                    );
        }
        else
        {
            const auto commentIt =
                std::ranges::find_if(
                    parsed.comments,
                    [&id](const SpeakingEvalAiBatchComment& comment)
                    {
                        return comment.id == id;
                    }
                    );
            if (commentIt == parsed.comments.cend())
            {
                m_reviewTable
                    ->item(row, ReviewStatusColumn)
                    ->setText(
                        tr("Missing response block")
                        );
            }
            else
            {
                QString comment =
                    commentIt->comment.simplified();
                comment.replace(
                    QStringLiteral("STD_NAME"),
                    preferredStudentName(report),
                    Qt::CaseSensitive
                    );
                commentItem->setText(comment);
                commentItem->setData(
                    HadPlaceholderRole,
                    commentIt->hadNamePlaceholder
                    );
            }
        }
        m_reviewTable->setItem(
            row,
            ReviewCommentColumn,
            commentItem
            );
        updateReviewRow(row);
    }

    m_reviewTable->resizeRowsToContents();

    QStringList summaryParts;
    summaryParts.append(
        tr("%1 of %2 selected students were parsed.")
            .arg(parsed.comments.size())
            .arg(expectedIds.size())
        );
    if (!parsed.unknownIds.isEmpty())
    {
        summaryParts.append(
            tr("Unknown IDs were ignored: %1")
                .arg(parsed.unknownIds.join(QStringLiteral(", ")))
            );
    }
    m_parseSummary->setText(
        summaryParts.join(QLatin1Char(' '))
        );
    updateApplyButton();
}

void SpeakingEvalAiBatchDialog::updateReviewRow(
    int row
    )
{
    QTableWidgetItem* applyItem =
        m_reviewTable->item(
            row,
            ReviewApplyColumn
            );
    QTableWidgetItem* statusItem =
        m_reviewTable->item(
            row,
            ReviewStatusColumn
            );
    QTableWidgetItem* charactersItem =
        m_reviewTable->item(
            row,
            ReviewCharactersColumn
            );
    QTableWidgetItem* commentItem =
        m_reviewTable->item(
            row,
            ReviewCommentColumn
            );
    if (
        !applyItem
        || !statusItem
        || !charactersItem
        || !commentItem
        )
    {
        return;
    }

    const QSignalBlocker blocker(m_reviewTable);
    const QString comment =
        commentItem->text().simplified();
    const int length =
        comment.size();
    charactersItem->setText(
        QString::number(length)
        );

    QString status;
    bool valid = false;
    if (comment.isEmpty())
    {
        if (statusItem->text().isEmpty())
        {
            status = tr("No comment");
        }
        else
        {
            status = statusItem->text();
        }
    }
    else if (length > SpeakingEval::CommentMaxLength)
    {
        status =
            tr("Too long — maximum %1 characters")
                .arg(SpeakingEval::CommentMaxLength);
    }
    else
    {
        valid = true;
        QStringList warnings;
        if (
            length < SpeakingEval::CommentMinLength
            || length > 420
            )
        {
            warnings.append(
                tr("outside preferred length")
                );
        }
        if (
            !commentItem
                ->data(HadPlaceholderRole)
                .toBool()
            )
        {
            warnings.append(
                tr("name placeholder was omitted")
                );
        }
        status = warnings.isEmpty()
            ? tr("Ready")
            : tr("Ready — %1")
                .arg(
                    warnings.join(
                        QStringLiteral(", ")
                        )
                    );
    }

    statusItem->setText(status);
    applyItem->setData(
        ReviewValidRole,
        valid
        );
    if (!valid)
    {
        applyItem->setCheckState(Qt::Unchecked);
    }
    else if (applyItem->checkState() == Qt::Unchecked)
    {
        applyItem->setCheckState(Qt::Checked);
    }
}

void SpeakingEvalAiBatchDialog::updateApplyButton()
{
    bool canApply = false;
    for (int row = 0; row < m_reviewTable->rowCount(); ++row)
    {
        const QTableWidgetItem* applyItem =
            m_reviewTable->item(
                row,
                ReviewApplyColumn
                );
        if (
            applyItem
            && applyItem
                ->data(ReviewValidRole)
                .toBool()
            && applyItem->checkState() == Qt::Checked
            )
        {
            canApply = true;
            break;
        }
    }
    m_applyButton->setEnabled(canApply);
}

void SpeakingEvalAiBatchDialog::applyComments()
{
    QList<SpeakingEvalAiBatchAcceptedComment> accepted;
    int overwriteCount = 0;
    for (int row = 0; row < m_reviewTable->rowCount(); ++row)
    {
        QTableWidgetItem* applyItem =
            m_reviewTable->item(
                row,
                ReviewApplyColumn
                );
        QTableWidgetItem* commentItem =
            m_reviewTable->item(
                row,
                ReviewCommentColumn
                );
        if (
            !applyItem
            || !commentItem
            || applyItem->checkState() != Qt::Checked
            || !applyItem
                ->data(ReviewValidRole)
                .toBool()
            )
        {
            continue;
        }

        const int reportIndex =
            applyItem->data(ReportIndexRole).toInt();
        if (
            reportIndex < 0
            || reportIndex >= m_reports.size()
            )
        {
            continue;
        }

        const QString newComment =
            commentItem->text().simplified();
        const QString oldComment =
            m_reports.at(reportIndex).report.comments;
        if (newComment == oldComment)
        {
            continue;
        }
        if (!oldComment.trimmed().isEmpty())
        {
            ++overwriteCount;
        }
        accepted.append(
            {
                m_reports.at(reportIndex).sourceRow,
                oldComment,
                newComment
            }
            );
    }

    if (accepted.isEmpty())
    {
        return;
    }

    if (overwriteCount > 0)
    {
        const QMessageBox::StandardButton answer =
            QMessageBox::question(
                this,
                tr("Replace Existing Comments?"),
                tr(
                    "%1 existing comment(s) will be replaced. "
                    "Do you want to continue?"
                    )
                    .arg(overwriteCount),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel
                );
        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    m_acceptedComments =
        std::move(accepted);
    accept();
}
