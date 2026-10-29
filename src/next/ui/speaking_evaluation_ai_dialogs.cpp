#include "next/ui/speaking_evaluation_ai_dialogs.h"

#include "domain/models/speaking_evaluation_comment_limits.h"
#include "next/application/speaking_evaluation_ai_batch_accepted_comment_plan.h"
#include "next/application/speaking_evaluation_ai_batch_comment_quality.h"
#include "next/application/speaking_evaluation_ai_comment_prompt.h"
#include "next/platform/speaking_evaluation_qt_text_match_port.h"
#include "ui/shared/qt_text_adapter.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTabWidget>
#include <QTableView>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

struct ParsedBatchComment final
{
    QString id;
    QString comment;
    bool hadNamePlaceholder = false;
};

struct ParsedBatchResponse final
{
    QList<ParsedBatchComment> comments;
    QStringList duplicateIds;
    QStringList malformedIds;
    QStringList unknownIds;
};

ParsedBatchResponse parseBatchResponse(
    const QString& response,
    const QStringList& expectedIds
    )
{
    ParsedBatchResponse result;
    const QSet<QString> expected(expectedIds.cbegin(), expectedIds.cend());
    const QRegularExpression completeBlock(
        QStringLiteral(R"(<<<(STUDENT_[0-9]{2})>>>\s*(.*?)\s*<<<END_\1>>>)"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression openingMarker(
        QStringLiteral(R"(<<<(STUDENT_[0-9]{2})>>>)"));

    QHash<QString, int> completeCounts;
    QHash<QString, ParsedBatchComment> parsedById;
    auto matches = completeBlock.globalMatch(response);
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        const QString id = match.captured(1);
        ++completeCounts[id];
        if (!expected.contains(id))
        {
            if (!result.unknownIds.contains(id))
            {
                result.unknownIds.append(id);
            }
            continue;
        }

        const QString comment = match.captured(2).trimmed();
        parsedById.insert(id, {
            id,
            comment,
            comment.contains(QStringLiteral("STD_NAME"), Qt::CaseSensitive)
        });
    }

    auto openings = openingMarker.globalMatch(response);
    QSet<QString> openedIds;
    while (openings.hasNext())
    {
        openedIds.insert(openings.next().captured(1));
    }

    for (const QString& id : expectedIds)
    {
        const int count = completeCounts.value(id);
        if (count > 1)
        {
            result.duplicateIds.append(id);
            continue;
        }
        if (count == 1)
        {
            result.comments.append(parsedById.value(id));
            continue;
        }
        if (openedIds.contains(id))
        {
            result.malformedIds.append(id);
        }
    }

    std::ranges::sort(result.unknownIds);
    return result;
}

QString batchText(const char* source)
{
    return QCoreApplication::translate("SpeakingEvalAiBatchDialog", source);
}

QString promptText(const char* source)
{
    return QCoreApplication::translate("SpeakingEvalAiPromptDialog", source);
}

QString studentDisplayName(
    const SpeakingEvaluationAiBatchStudent& student
    )
{
    const QString english = student.englishName.trimmed();
    return english.isEmpty() ? student.koreanName.trimmed() : english;
}

QString eligibilityText(
    const Application::SpeakingEvaluationAiBatchEligibilityReason reason
    )
{
    using Reason = Application::SpeakingEvaluationAiBatchEligibilityReason;
    switch (reason)
    {
    case Reason::Eligible:
        return batchText("Ready");
    case Reason::MissingName:
        return batchText("Student name is missing.");
    case Reason::UnsupportedGrade:
        return batchText("AI comments are available for grades E4 through E6.");
    case Reason::MissingDidWellItem:
        return batchText("Add at least one Did Well note.");
    case Reason::MissingNeedsImprovementItem:
        return batchText("Add at least one Needs Improvement note.");
    }
    return {};
}

QTableView* createTableView(
    const QString& objectName,
    QAbstractItemModel* model,
    QWidget* parent
    )
{
    auto* table = new QTableView(parent);
    table->setObjectName(objectName);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->verticalHeader()->hide();
    return table;
}

class SpeakingEvaluationCheckIndicatorDelegate final
    : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    [[nodiscard]] static QRect indicatorRect(const QRect& cellRect)
    {
        const int side = std::clamp(cellRect.height() - 8, 14, 20);
        return QRect(
            cellRect.center().x() - side / 2,
            cellRect.center().y() - side / 2,
            side,
            side);
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        const QVariant checkState = index.data(Qt::CheckStateRole);
        if (!checkState.isValid())
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const bool checked = checkState.toInt() == Qt::Checked;
        const bool enabled = option.state.testFlag(QStyle::State_Enabled)
            && index.flags().testFlag(Qt::ItemIsUserCheckable);
        const QRect indicator = indicatorRect(option.rect);
        const QColor cellBackground = option.state.testFlag(QStyle::State_Selected)
            ? option.palette.color(QPalette::Highlight)
            : option.backgroundBrush.style() != Qt::NoBrush
                ? option.backgroundBrush.color()
                : option.palette.color(option.features.testFlag(
                    QStyleOptionViewItem::Alternate)
                    ? QPalette::AlternateBase : QPalette::Base);
        const QColor accent = option.palette.color(QPalette::Highlight);
        const QColor outline = enabled
            ? (checked ? accent : option.palette.color(QPalette::Mid))
            : option.palette.color(QPalette::Disabled, QPalette::Mid);

        painter->save();
        painter->fillRect(option.rect, cellBackground);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(checked ? accent : cellBackground);
        painter->drawRoundedRect(indicator, 3, 3);
        painter->setPen(QPen(outline, 1.25));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(indicator, 3, 3);
        if (checked)
        {
            QPen checkPen(
                option.palette.color(QPalette::HighlightedText), 2.0);
            checkPen.setCapStyle(Qt::RoundCap);
            checkPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(checkPen);
            painter->drawLine(
                QPointF(indicator.left() + indicator.width() * 0.24,
                    indicator.top() + indicator.height() * 0.53),
                QPointF(indicator.left() + indicator.width() * 0.43,
                    indicator.top() + indicator.height() * 0.72));
            painter->drawLine(
                QPointF(indicator.left() + indicator.width() * 0.43,
                    indicator.top() + indicator.height() * 0.72),
                QPointF(indicator.left() + indicator.width() * 0.78,
                    indicator.top() + indicator.height() * 0.31));
        }
        if (option.state.testFlag(QStyle::State_HasFocus))
        {
            QStyleOptionFocusRect focusOption;
            focusOption.rect = option.rect.adjusted(1, 1, -1, -1);
            focusOption.palette = option.palette;
            focusOption.state = option.state;
            focusOption.backgroundColor = cellBackground;
            QStyle* const itemStyle = option.widget
                ? option.widget->style() : QApplication::style();
            itemStyle->drawPrimitive(
                QStyle::PE_FrameFocusRect,
                &focusOption,
                painter,
                option.widget);
        }
        painter->restore();
    }

    bool editorEvent(
        QEvent* event,
        QAbstractItemModel* model,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) override
    {
        const QVariant checkState = index.data(Qt::CheckStateRole);
        if (!checkState.isValid()
            || !(index.flags() & Qt::ItemIsUserCheckable)
            || !(index.flags() & Qt::ItemIsEnabled))
        {
            return false;
        }

        bool toggled = false;
        if (event->type() == QEvent::MouseButtonRelease)
        {
            const auto* const mouseEvent = static_cast<QMouseEvent*>(event);
            toggled = mouseEvent->button() == Qt::LeftButton
                && indicatorRect(option.rect).contains(
                    mouseEvent->position().toPoint());
        }
        else if (event->type() == QEvent::KeyPress)
        {
            const auto* const keyEvent = static_cast<QKeyEvent*>(event);
            toggled = keyEvent->key() == Qt::Key_Space
                && !keyEvent->isAutoRepeat();
        }
        if (!toggled)
        {
            return false;
        }

        const int nextState = checkState.toInt() == Qt::Checked
            ? Qt::Unchecked : Qt::Checked;
        return model->setData(index, nextState, Qt::CheckStateRole);
    }
};

} // namespace

SpeakingEvaluationAiBatchSelectionModel::SpeakingEvaluationAiBatchSelectionModel(
    std::vector<SpeakingEvaluationAiBatchStudent> students,
    QObject* parent
    )
    : QAbstractTableModel(parent)
    , m_students(std::move(students))
    , m_selected(m_students.size(), false)
{
    for (std::size_t index = 0; index < m_students.size(); ++index)
    {
        const auto& student = m_students[index];
        if (student.eligibilityReason ==
                Application::SpeakingEvaluationAiBatchEligibilityReason::Eligible
            && student.oldComment.trimmed().isEmpty())
        {
            m_selected[index] = true;
        }
    }
}

int SpeakingEvaluationAiBatchSelectionModel::rowCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_students.size());
}

int SpeakingEvaluationAiBatchSelectionModel::columnCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SpeakingEvaluationAiBatchSelectionModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_students.size())
    {
        return {};
    }

    const std::size_t row = static_cast<std::size_t>(index.row());
    const SpeakingEvaluationAiBatchStudent& student = m_students[row];
    if (role == SourceRowRole)
    {
        return student.sourceRow;
    }
    if (role == StudentIdRole)
    {
        return student.id;
    }
    if (role == Qt::CheckStateRole && index.column() == IncludeColumn)
    {
        return m_selected[row] ? Qt::Checked : Qt::Unchecked;
    }
    if (role == Qt::ToolTipRole && index.column() == StatusColumn)
    {
        return student.eligibilityReason ==
                Application::SpeakingEvaluationAiBatchEligibilityReason::Eligible
            ? QString{}
            : eligibilityText(student.eligibilityReason);
    }
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (index.column())
    {
    case IncludeColumn:
        return {};
    case StudentColumn:
        return studentDisplayName(student);
    case StatusColumn:
        if (student.eligibilityReason !=
            Application::SpeakingEvaluationAiBatchEligibilityReason::Eligible)
        {
            return eligibilityText(student.eligibilityReason);
        }
        return student.oldComment.trimmed().isEmpty()
            ? batchText("Ready")
            : batchText("Existing comment — select to regenerate");
    default:
        return {};
    }
}

bool SpeakingEvaluationAiBatchSelectionModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (role != Qt::CheckStateRole || !index.isValid()
        || index.column() != IncludeColumn || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_students.size()
        || !isEligible(index.row()))
    {
        return false;
    }

    const std::size_t row = static_cast<std::size_t>(index.row());
    const bool selected = value.toInt() == Qt::Checked;
    if (m_selected[row] == selected)
    {
        return true;
    }
    m_selected[row] = selected;
    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags SpeakingEvaluationAiBatchSelectionModel::flags(
    const QModelIndex& index
    ) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == IncludeColumn && isEligible(index.row()))
    {
        result |= Qt::ItemIsUserCheckable;
    }
    return result;
}

QVariant SpeakingEvaluationAiBatchSelectionModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= ColumnCount)
    {
        return {};
    }
    static const std::array<const char*, ColumnCount> headers{
        "Include", "Student", "Status"
    };
    return batchText(headers[static_cast<std::size_t>(section)]);
}

bool SpeakingEvaluationAiBatchSelectionModel::isEligible(const int row) const noexcept
{
    return row >= 0 && static_cast<std::size_t>(row) < m_students.size()
        && m_students[static_cast<std::size_t>(row)].eligibilityReason
            == Application::SpeakingEvaluationAiBatchEligibilityReason::Eligible;
}

bool SpeakingEvaluationAiBatchSelectionModel::isSelected(const int row) const noexcept
{
    return row >= 0 && static_cast<std::size_t>(row) < m_selected.size()
        && m_selected[static_cast<std::size_t>(row)];
}

const SpeakingEvaluationAiBatchStudent*
SpeakingEvaluationAiBatchSelectionModel::studentAt(const int row) const noexcept
{
    return row >= 0 && static_cast<std::size_t>(row) < m_students.size()
        ? &m_students[static_cast<std::size_t>(row)]
        : nullptr;
}

std::vector<int> SpeakingEvaluationAiBatchSelectionModel::selectedRows() const
{
    std::vector<int> result;
    result.reserve(m_students.size());
    for (std::size_t index = 0; index < m_selected.size(); ++index)
    {
        if (m_selected[index])
        {
            result.push_back(static_cast<int>(index));
        }
    }
    return result;
}

SpeakingEvaluationAiBatchReviewModel::SpeakingEvaluationAiBatchReviewModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
}

int SpeakingEvaluationAiBatchReviewModel::rowCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int SpeakingEvaluationAiBatchReviewModel::columnCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SpeakingEvaluationAiBatchReviewModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_entries.size())
    {
        return {};
    }
    const Entry& entry = m_entries[static_cast<std::size_t>(index.row())];
    if (role == ValidRole)
    {
        return entry.valid;
    }
    if (role == SourceRowRole)
    {
        return entry.sourceRow;
    }
    if (role == Qt::CheckStateRole && index.column() == ApplyColumn)
    {
        return entry.checked ? Qt::Checked : Qt::Unchecked;
    }
    if (role == Qt::TextAlignmentRole && index.column() == CharactersColumn)
    {
        return static_cast<int>(Qt::AlignCenter);
    }
    if (role == Qt::ToolTipRole && index.column() == StatusColumn)
    {
        return entry.responseStatus;
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
        return {};
    }
    switch (index.column())
    {
    case ApplyColumn:
        return {};
    case StudentColumn:
        return entry.studentName;
    case StatusColumn:
        return entry.responseStatus;
    case CharactersColumn:
        return entry.comment.simplified().size();
    case CommentColumn:
        return entry.comment;
    default:
        return {};
    }
}

bool SpeakingEvaluationAiBatchReviewModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (!index.isValid() || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_entries.size())
    {
        return false;
    }
    Entry& entry = m_entries[static_cast<std::size_t>(index.row())];
    if (index.column() == ApplyColumn && role == Qt::CheckStateRole)
    {
        if (!entry.valid)
        {
            return false;
        }
        entry.checked = value.toInt() == Qt::Checked;
        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }
    if (index.column() == CommentColumn && role == Qt::EditRole)
    {
        entry.comment = value.toString();
        entry.responseError.clear();
        updateCommentQuality(index.row());
        return true;
    }
    return false;
}

Qt::ItemFlags SpeakingEvaluationAiBatchReviewModel::flags(
    const QModelIndex& index
    ) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_entries.size())
    {
        return Qt::NoItemFlags;
    }
    const Entry& entry = m_entries[static_cast<std::size_t>(index.row())];
    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ApplyColumn && entry.valid)
    {
        result |= Qt::ItemIsUserCheckable;
    }
    if (index.column() == CommentColumn)
    {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

QVariant SpeakingEvaluationAiBatchReviewModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= ColumnCount)
    {
        return {};
    }
    static const std::array<const char*, ColumnCount> headers{
        "Apply", "Student", "Status", "Characters", "Comment"
    };
    return batchText(headers[static_cast<std::size_t>(section)]);
}

void SpeakingEvaluationAiBatchReviewModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    for (std::size_t row = 0; row < m_entries.size(); ++row)
    {
        updateCommentQuality(static_cast<int>(row));
    }
}

void SpeakingEvaluationAiBatchReviewModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

bool SpeakingEvaluationAiBatchReviewModel::hasCheckedValidComment() const noexcept
{
    return std::ranges::any_of(m_entries, [](const Entry& entry) {
        return entry.checked && entry.valid;
    });
}

std::vector<Application::SpeakingEvaluationAiBatchAcceptedCommentCandidate>
SpeakingEvaluationAiBatchReviewModel::acceptedCandidates() const
{
    std::vector<Application::SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        candidates;
    candidates.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
    {
        candidates.push_back({
            .checked = entry.checked,
            .valid = entry.valid,
            .reportIndex =
                Application::SpeakingEvaluationAiBatchReportIndex{
                    entry.reportIndex},
            .sourceRow =
                Application::SpeakingEvaluationAiBatchAcceptedCommentSourceRow{
                    entry.sourceRow},
            .oldComment = entry.oldComment.toStdU16String(),
            .newComment = entry.comment.simplified().toStdU16String()
        });
    }
    return candidates;
}

const SpeakingEvaluationAiBatchReviewModel::Entry*
SpeakingEvaluationAiBatchReviewModel::entryAt(const int row) const noexcept
{
    return row >= 0 && static_cast<std::size_t>(row) < m_entries.size()
        ? &m_entries[static_cast<std::size_t>(row)]
        : nullptr;
}

void SpeakingEvaluationAiBatchReviewModel::updateCommentQuality(const int row)
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_entries.size())
    {
        return;
    }
    Entry& entry = m_entries[static_cast<std::size_t>(row)];
    const QString normalized = entry.comment.simplified();
    const Application::SpeakingEvaluationAiBatchCommentQuality quality =
        Application::assessSpeakingEvaluationAiBatchCommentQuality({
            .normalizedCodeUnitLength = static_cast<std::size_t>(normalized.size()),
            .hadNamePlaceholder = entry.hadNamePlaceholder,
            .minimumCodeUnitLength = static_cast<std::size_t>(
                SpeakingEval::CommentMinLength),
            .preferredMaximumCodeUnitLength = static_cast<std::size_t>(
                SpeakingEval::CommentPreferredMaxLength),
            .maximumCodeUnitLength = static_cast<std::size_t>(
                SpeakingEval::CommentMaxLength)
        });

    entry.valid = quality.isValid() && entry.responseError.isEmpty();
    if (!entry.valid)
    {
        entry.checked = false;
    }

    if (!entry.responseError.isEmpty()
        && quality.rejection ==
            Application::SpeakingEvaluationAiBatchCommentRejection::Empty)
    {
        entry.responseStatus = entry.responseError;
    }
    else if (quality.rejection ==
        Application::SpeakingEvaluationAiBatchCommentRejection::Empty)
    {
        entry.responseStatus = batchText("No comment");
    }
    else if (quality.rejection ==
        Application::SpeakingEvaluationAiBatchCommentRejection::TooLong)
    {
        entry.responseStatus = batchText("Too long — maximum %1 characters")
            .arg(SpeakingEval::CommentMaxLength);
    }
    else
    {
        QStringList warnings;
        if (quality.outsidePreferredLength)
        {
            warnings.append(batchText("outside preferred length"));
        }
        if (quality.missingNamePlaceholder)
        {
            warnings.append(batchText("name placeholder was omitted"));
        }
        entry.responseStatus = warnings.isEmpty()
            ? batchText("Ready")
            : batchText("Ready — %1").arg(warnings.join(QStringLiteral(", ")));
    }

    const QModelIndex left = index(row, 0);
    const QModelIndex right = index(row, ColumnCount - 1);
    emit dataChanged(left, right,
        {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole, ValidRole});
}

SpeakingEvaluationAiPromptDialog::SpeakingEvaluationAiPromptDialog(
    QString prompt,
    QString providerName,
    QUrl providerUrl,
    std::function<bool(const QUrl&)> openExternalUrl,
    QWidget* parent
    )
    : QDialog(parent)
    , m_prompt(std::move(prompt))
    , m_providerUrl(std::move(providerUrl))
    , m_openExternalUrl(std::move(openExternalUrl))
{
    setObjectName(QStringLiteral("speakingEvaluationAiPromptDialog"));
    setWindowTitle(promptText("AI Comment Prompt"));
    resize(840, 600);

    auto* layout = new QVBoxLayout(this);
    auto* privacy = new QLabel(promptText(
        "Student names are removed from the prompt and restored locally."), this);
    privacy->setWordWrap(true);
    layout->addWidget(privacy);

    m_promptEditor = new QPlainTextEdit(this);
    m_promptEditor->setObjectName(QStringLiteral("speakingEvaluationAiPromptText"));
    m_promptEditor->setReadOnly(true);
    m_promptEditor->setPlainText(m_prompt);
    layout->addWidget(m_promptEditor, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    m_copyButton = new QPushButton(batchText("Copy Prompt"), this);
    m_copyButton->setObjectName(QStringLiteral("speakingEvaluationAiPromptCopy"));
    m_copyAndOpenButton = new QPushButton(
        batchText("Copy Prompt and Open %1").arg(providerName), this);
    m_copyAndOpenButton->setObjectName(
        QStringLiteral("speakingEvaluationAiPromptCopyOpen"));
    auto* closeButton = new QPushButton(promptText("Close"), this);
    closeButton->setObjectName(QStringLiteral("speakingEvaluationAiPromptClose"));
    buttons->addWidget(m_copyButton);
    buttons->addWidget(m_copyAndOpenButton);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        copyPrompt(false);
    });
    connect(m_copyAndOpenButton, &QPushButton::clicked, this, [this] {
        copyPrompt(true);
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

QPlainTextEdit* SpeakingEvaluationAiPromptDialog::promptEditor() const noexcept
{
    return m_promptEditor;
}

QPushButton* SpeakingEvaluationAiPromptDialog::copyButton() const noexcept
{
    return m_copyButton;
}

QPushButton* SpeakingEvaluationAiPromptDialog::copyAndOpenButton() const noexcept
{
    return m_copyAndOpenButton;
}

void SpeakingEvaluationAiPromptDialog::copyPrompt(const bool openProvider)
{
    if (m_prompt.isEmpty())
    {
        return;
    }
    QApplication::clipboard()->setText(m_prompt);
    if (!openProvider)
    {
        return;
    }
    const bool opened = !m_providerUrl.isEmpty()
        && (m_openExternalUrl
            ? m_openExternalUrl(m_providerUrl)
            : QDesktopServices::openUrl(m_providerUrl));
    if (!opened)
    {
        QMessageBox::warning(
            this,
            batchText("Unable to Open AI Website"),
            batchText("The AI website could not be opened. The prompt is still available on the clipboard."));
    }
}

SpeakingEvaluationAiBatchDialog::SpeakingEvaluationAiBatchDialog(
    std::vector<SpeakingEvaluationAiBatchStudent> students,
    const Application::AiCommentVoice voice,
    QString providerName,
    QUrl providerUrl,
    std::function<bool(const QUrl&)> openExternalUrl,
    QWidget* parent
    )
    : QDialog(parent)
    , m_voice(voice)
    , m_providerUrl(std::move(providerUrl))
    , m_openExternalUrl(std::move(openExternalUrl))
{
    m_inputWithinBatchBound = students.size()
        <= Application::kSpeakingEvaluationAiBatchMaximumStudents;
    if (m_inputWithinBatchBound)
    {
        m_students = std::move(students);
    }

    setObjectName(QStringLiteral("speakingEvaluationAiBatchDialog"));
    setWindowTitle(batchText("Generate Class Comments"));
    resize(1080, 760);

    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("speakingEvaluationAiBatchTabs"));
    layout->addWidget(tabs, 1);

    auto* promptPage = new QWidget(tabs);
    auto* promptLayout = new QVBoxLayout(promptPage);
    promptLayout->setContentsMargins(12, 12, 12, 12);
    promptLayout->setSpacing(8);
    auto* selectionInfo = new QLabel(batchText(
        "Select students. Eligible students without comments are selected automatically."),
        promptPage);
    selectionInfo->setWordWrap(true);
    promptLayout->addWidget(selectionInfo);

    m_selectionModel = new SpeakingEvaluationAiBatchSelectionModel(
        m_students, this);
    m_selectionTable = createTableView(
        QStringLiteral("speakingEvaluationAiBatchSelectionTable"),
        m_selectionModel,
        promptPage);
    m_selectionTable->setItemDelegateForColumn(
        SpeakingEvaluationAiBatchSelectionModel::IncludeColumn,
        new SpeakingEvaluationCheckIndicatorDelegate(m_selectionTable));
    m_selectionTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchSelectionModel::IncludeColumn,
        QHeaderView::ResizeToContents);
    m_selectionTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchSelectionModel::StudentColumn,
        QHeaderView::ResizeToContents);
    m_selectionTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchSelectionModel::StatusColumn,
        QHeaderView::Stretch);
    m_selectionTable->setMinimumHeight(185);
    promptLayout->addWidget(m_selectionTable, 1);

    m_batchStatus = new QLabel(promptPage);
    m_batchStatus->setObjectName(QStringLiteral("speakingEvaluationAiBatchStatus"));
    m_batchStatus->setProperty("role", QStringLiteral("status"));
    m_batchStatus->setWordWrap(true);
    if (!m_inputWithinBatchBound)
    {
        m_batchStatus->setText(batchText(
            "The AI comment batch supports up to %1 students.")
            .arg(Application::kSpeakingEvaluationAiBatchMaximumStudents));
    }
    else if (m_students.empty())
    {
        m_batchStatus->setText(batchText("No named students are available."));
    }
    promptLayout->addWidget(m_batchStatus);

    m_createPromptButton = new QPushButton(
        batchText("Create Class Prompt"), promptPage);
    m_createPromptButton->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchCreatePrompt"));
    promptLayout->addWidget(m_createPromptButton, 0, Qt::AlignRight);

    auto* privacy = new QLabel(batchText(
        "Real student names are removed from the prompt and are restored locally after the response is pasted."),
        promptPage);
    privacy->setWordWrap(true);
    promptLayout->addWidget(privacy);

    m_promptEditor = new QPlainTextEdit(promptPage);
    m_promptEditor->setObjectName(QStringLiteral("speakingEvaluationAiBatchPrompt"));
    m_promptEditor->setReadOnly(true);
    m_promptEditor->setPlaceholderText(
        batchText("Select students and create the class prompt."));
    promptLayout->addWidget(m_promptEditor, 1);

    auto* promptButtons = new QHBoxLayout();
    promptButtons->addStretch(1);
    m_copyPromptButton = new QPushButton(batchText("Copy Prompt"), promptPage);
    m_copyPromptButton->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchCopyPrompt"));
    m_copyAndOpenButton = new QPushButton(
        batchText("Copy Prompt and Open %1").arg(providerName), promptPage);
    m_copyAndOpenButton->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchCopyOpen"));
    promptButtons->addWidget(m_copyPromptButton);
    promptButtons->addWidget(m_copyAndOpenButton);
    promptLayout->addLayout(promptButtons);
    tabs->addTab(promptPage, batchText("1. Create Prompt"));

    auto* responsePage = new QWidget(tabs);
    auto* responseLayout = new QVBoxLayout(responsePage);
    responseLayout->setContentsMargins(12, 12, 12, 12);
    responseLayout->setSpacing(8);
    auto* responseInfo = new QLabel(
        batchText("Paste the complete AI response below."), responsePage);
    responseLayout->addWidget(responseInfo);
    m_responseEditor = new QPlainTextEdit(responsePage);
    m_responseEditor->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchResponse"));
    m_responseEditor->setPlaceholderText(
        batchText("Paste the response containing the STUDENT blocks here…"));
    m_responseEditor->setMaximumHeight(170);
    responseLayout->addWidget(m_responseEditor);
    m_parseButton = new QPushButton(batchText("Parse Response"), responsePage);
    m_parseButton->setObjectName(QStringLiteral("speakingEvaluationAiBatchParse"));
    responseLayout->addWidget(m_parseButton, 0, Qt::AlignRight);
    m_parseSummary = new QLabel(responsePage);
    m_parseSummary->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchParseSummary"));
    m_parseSummary->setWordWrap(true);
    responseLayout->addWidget(m_parseSummary);

    m_reviewModel = new SpeakingEvaluationAiBatchReviewModel(this);
    auto* reviewTable = createTableView(
        QStringLiteral("speakingEvaluationAiBatchReviewTable"),
        m_reviewModel,
        responsePage);
    reviewTable->setItemDelegateForColumn(
        SpeakingEvaluationAiBatchReviewModel::ApplyColumn,
        new SpeakingEvaluationCheckIndicatorDelegate(reviewTable));
    reviewTable->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    reviewTable->setWordWrap(true);
    reviewTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchReviewModel::ApplyColumn,
        QHeaderView::ResizeToContents);
    reviewTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchReviewModel::StudentColumn,
        QHeaderView::ResizeToContents);
    reviewTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchReviewModel::StatusColumn,
        QHeaderView::ResizeToContents);
    reviewTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchReviewModel::CharactersColumn,
        QHeaderView::ResizeToContents);
    reviewTable->horizontalHeader()->setSectionResizeMode(
        SpeakingEvaluationAiBatchReviewModel::CommentColumn,
        QHeaderView::Stretch);
    responseLayout->addWidget(reviewTable, 1);
    tabs->addTab(responsePage, batchText("2. Paste and Review"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_applyButton = new QPushButton(batchText("Apply Selected Comments"), this);
    m_applyButton->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchApply"));
    buttons->addButton(m_applyButton, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_selectionModel, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
            if (roles.contains(Qt::CheckStateRole))
            {
                resetGeneratedContent();
            }
            updateActions();
        });
    connect(m_createPromptButton, &QPushButton::clicked, this, [this] {
        createPrompt();
    });
    connect(m_copyPromptButton, &QPushButton::clicked, this, [this] {
        copyPrompt(false);
    });
    connect(m_copyAndOpenButton, &QPushButton::clicked, this, [this] {
        copyPrompt(true);
    });
    connect(m_responseEditor, &QPlainTextEdit::textChanged, this, [this] {
        m_reviewModel->clear();
        m_parseSummary->clear();
        updateActions();
    });
    connect(m_parseButton, &QPushButton::clicked, this, [this] {
        parseResponse();
    });
    connect(m_reviewModel, &QAbstractItemModel::dataChanged, this,
        [this] { updateActions(); });
    connect(m_reviewModel, &QAbstractItemModel::modelReset, this,
        [this] { updateActions(); });
    connect(m_applyButton, &QPushButton::clicked, this, [this] {
        applyComments();
    });
    updateActions();
}

SpeakingEvaluationAiBatchSelectionModel*
SpeakingEvaluationAiBatchDialog::selectionModel() const noexcept
{
    return m_selectionModel;
}

SpeakingEvaluationAiBatchReviewModel*
SpeakingEvaluationAiBatchDialog::reviewModel() const noexcept
{
    return m_reviewModel;
}

QPlainTextEdit* SpeakingEvaluationAiBatchDialog::promptEditor() const noexcept
{
    return m_promptEditor;
}

QPlainTextEdit* SpeakingEvaluationAiBatchDialog::responseEditor() const noexcept
{
    return m_responseEditor;
}

QLabel* SpeakingEvaluationAiBatchDialog::parseSummaryLabel() const noexcept
{
    return m_parseSummary;
}

QPushButton* SpeakingEvaluationAiBatchDialog::createPromptButton() const noexcept
{
    return m_createPromptButton;
}

QPushButton* SpeakingEvaluationAiBatchDialog::parseButton() const noexcept
{
    return m_parseButton;
}

QPushButton* SpeakingEvaluationAiBatchDialog::applyButton() const noexcept
{
    return m_applyButton;
}

const std::vector<SpeakingEvaluationAiBatchAcceptedComment>&
SpeakingEvaluationAiBatchDialog::acceptedComments() const noexcept
{
    return m_acceptedComments;
}

void SpeakingEvaluationAiBatchDialog::resetGeneratedContent()
{
    m_selectedRows.clear();
    m_promptEditor->clear();
    m_responseEditor->clear();
    m_reviewModel->clear();
    m_parseSummary->clear();
    if (m_batchStatus->text().isEmpty() && !m_students.empty())
    {
        m_batchStatus->clear();
    }
    updateActions();
}

void SpeakingEvaluationAiBatchDialog::updateActions()
{
    const std::vector<int> selected = m_selectionModel->selectedRows();
    const bool promptReady = m_inputWithinBatchBound
        && !m_promptEditor->toPlainText().isEmpty();
    m_createPromptButton->setEnabled(
        m_inputWithinBatchBound && !selected.empty());
    m_copyPromptButton->setEnabled(promptReady);
    m_copyAndOpenButton->setEnabled(promptReady);
    m_parseButton->setEnabled(!m_selectedRows.empty()
        && !m_responseEditor->toPlainText().trimmed().isEmpty());
    m_applyButton->setEnabled(m_reviewModel->hasCheckedValidComment());
}

void SpeakingEvaluationAiBatchDialog::createPrompt()
{
    m_selectedRows = m_selectionModel->selectedRows();
    if (m_selectedRows.empty() || !m_inputWithinBatchBound)
    {
        return;
    }

    Application::SpeakingEvaluationAiBatchCommentPromptRequest request;
    request.voice = m_voice;
    request.additionalNamesToRedact.reserve(m_students.size() * 2);
    for (const SpeakingEvaluationAiBatchStudent& student : m_students)
    {
        request.additionalNamesToRedact.push_back(
            student.englishName.toStdU16String());
        request.additionalNamesToRedact.push_back(
            student.koreanName.toStdU16String());
    }
    request.students.reserve(m_selectedRows.size());
    for (const int row : m_selectedRows)
    {
        const SpeakingEvaluationAiBatchStudent* const student =
            m_selectionModel->studentAt(row);
        if (!student)
        {
            resetGeneratedContent();
            return;
        }
        request.students.push_back({
            .id = student->id.toStdU16String(),
            .grade = student->grade,
            .englishName = student->englishName.toStdU16String(),
            .koreanName = student->koreanName.toStdU16String(),
            .didWell = student->didWell.toStdU16String(),
            .needsImprovement = student->needsImprovement.toStdU16String()
        });
    }

    const Platform::SpeakingEvaluationQtTextMatchPort textMatcher;
    const Application::SpeakingEvaluationAiCommentPromptResult result =
        Application::buildSpeakingEvaluationAiBatchCommentPrompt(
            request, textMatcher);
    if (!result.valid)
    {
        m_promptEditor->clear();
        m_batchStatus->setText(batchText(
            "The selected students could not be included in an AI prompt."));
        updateActions();
        return;
    }
    m_promptEditor->setPlainText(QString::fromStdU16String(result.prompt));
    m_batchStatus->clear();
    updateActions();
}

void SpeakingEvaluationAiBatchDialog::copyPrompt(const bool openProvider)
{
    const QString prompt = m_promptEditor->toPlainText();
    if (prompt.isEmpty())
    {
        return;
    }
    QApplication::clipboard()->setText(prompt);
    if (!openProvider)
    {
        return;
    }
    const bool opened = !m_providerUrl.isEmpty()
        && (m_openExternalUrl
            ? m_openExternalUrl(m_providerUrl)
            : QDesktopServices::openUrl(m_providerUrl));
    if (!opened)
    {
        QMessageBox::warning(
            this,
            batchText("Unable to Open AI Website"),
            batchText("The AI website could not be opened. The prompt is still available on the clipboard."));
    }
}

void SpeakingEvaluationAiBatchDialog::parseResponse()
{
    if (m_selectedRows.empty())
    {
        return;
    }
    QStringList expectedIds;
    expectedIds.reserve(static_cast<qsizetype>(m_selectedRows.size()));
    for (const int row : m_selectedRows)
    {
        if (const auto* student = m_selectionModel->studentAt(row))
        {
            expectedIds.append(student->id);
        }
    }
    if (expectedIds.size() != static_cast<qsizetype>(m_selectedRows.size()))
    {
        m_reviewModel->clear();
        m_parseSummary->setText(batchText(
            "The selected students changed. Create the prompt again."));
        updateActions();
        return;
    }

    const ParsedBatchResponse parsed =
        parseBatchResponse(
            m_responseEditor->toPlainText(), expectedIds);

    std::vector<SpeakingEvaluationAiBatchReviewModel::Entry> entries;
    entries.reserve(m_selectedRows.size());
    for (const int row : m_selectedRows)
    {
        const SpeakingEvaluationAiBatchStudent* const student =
            m_selectionModel->studentAt(row);
        if (!student)
        {
            continue;
        }
        SpeakingEvaluationAiBatchReviewModel::Entry entry{
            .sourceRow = student->sourceRow,
            .reportIndex = row,
            .studentName = studentDisplayName(*student),
            .oldComment = student->oldComment
        };

        if (parsed.duplicateIds.contains(student->id))
        {
            entry.responseError = batchText("Duplicate response blocks");
        }
        else if (parsed.malformedIds.contains(student->id))
        {
            entry.responseError = batchText("Malformed response block");
        }
        else
        {
            const auto comment = std::ranges::find_if(
                parsed.comments,
                [&student](const ParsedBatchComment& parsedComment) {
                    return parsedComment.id == student->id;
                });
            if (comment == parsed.comments.end())
            {
                entry.responseError = batchText("Missing response block");
            }
            else
            {
                entry.comment = comment->comment.simplified();
                entry.comment.replace(
                    QStringLiteral("STD_NAME"),
                    studentDisplayName(*student),
                    Qt::CaseSensitive);
                entry.hadNamePlaceholder = comment->hadNamePlaceholder;
            }
        }
        entries.push_back(std::move(entry));
    }
    m_reviewModel->setEntries(std::move(entries));
    QStringList summaryParts;
    summaryParts.append(batchText("%1 of %2 selected students were parsed.")
        .arg(parsed.comments.size())
        .arg(expectedIds.size()));
    if (!parsed.unknownIds.isEmpty())
    {
        summaryParts.append(batchText("Unknown IDs were ignored: %1")
            .arg(parsed.unknownIds.join(QStringLiteral(", "))));
    }
    m_parseSummary->setText(summaryParts.join(QLatin1Char(' ')));
    updateActions();
}

void SpeakingEvaluationAiBatchDialog::applyComments()
{
    const Application::SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        Application::planSpeakingEvaluationAiBatchAcceptedComments(
            m_reviewModel->acceptedCandidates(), m_students.size());
    if (plan.assignments.empty())
    {
        return;
    }

    if (plan.overwriteCount.rawValue() > 0)
    {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            batchText("Replace Existing Comments?"),
            batchText("%1 existing comment(s) will be replaced. Do you want to continue?")
                .arg(static_cast<qulonglong>(plan.overwriteCount.rawValue())),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    m_acceptedComments.clear();
    m_acceptedComments.reserve(plan.assignments.size());
    for (const auto& assignment : plan.assignments)
    {
        m_acceptedComments.push_back({
            .sourceRow = assignment.sourceRow.rawValue(),
            .oldComment = QString::fromStdU16String(assignment.oldComment),
            .newComment = QString::fromStdU16String(assignment.newComment)
        });
    }
    accept();
}

} // namespace ClassMngr::Next::Ui
