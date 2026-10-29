#include "next/ui/speaking_evaluation_page.h"

#include "next/application/speaking_evaluation_private_notes_split.h"
#include "next/application/speaking_evaluation_ai_comment_prompt.h"
#include "next/application/speaking_evaluation_ai_batch_eligibility.h"
#include "next/application/speaking_evaluation_roster_name_import_plan.h"
#include "next/application/speaking_evaluation_roster_names_read_query.h"
#include "next/application/selected_class_grade_read_query.h"
#include "next/platform/speaking_evaluation_qt_text_match_port.h"
#include "next/ui/speaking_evaluation_ai_dialogs.h"
#include "shared/speaking_evaluation/report_projection.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QSplitter>
#include <QTableView>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString pageText(const char* source)
{
    return QCoreApplication::translate("SpeakingEvalPage", source);
}

QString notesText(const char* source)
{
    return QCoreApplication::translate("SpeakingEvalPrivateNotesEditor", source);
}

QString fromUtf16(const std::u16string& value)
{
    return QString::fromUtf16(value.data(), static_cast<qsizetype>(value.size()));
}

QString errorText(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString aiProviderDisplayName(
    const Application::AiCommentProvider provider
    )
{
    switch (provider)
    {
    case Application::AiCommentProvider::Gemini:
        return QStringLiteral("Gemini");
    case Application::AiCommentProvider::Claude:
        return QStringLiteral("Claude");
    case Application::AiCommentProvider::MicrosoftCopilot:
        return QStringLiteral("Microsoft Copilot");
    case Application::AiCommentProvider::CustomWebsite:
        return QStringLiteral("Custom AI Website");
    case Application::AiCommentProvider::ChatGPT:
    default:
        return QStringLiteral("ChatGPT");
    }
}

QUrl aiProviderWebsiteUrl(
    const Application::AiCommentProvider provider,
    const QString& customWebsiteUrl
    )
{
    switch (provider)
    {
    case Application::AiCommentProvider::Gemini:
        return QUrl(QStringLiteral("https://gemini.google.com/app"));
    case Application::AiCommentProvider::Claude:
        return QUrl(QStringLiteral("https://claude.ai/"));
    case Application::AiCommentProvider::MicrosoftCopilot:
        return QUrl(QStringLiteral("https://copilot.microsoft.com/"));
    case Application::AiCommentProvider::CustomWebsite:
    {
        const QUrl customUrl(customWebsiteUrl.trimmed());
        return customUrl.isValid()
                && customUrl.scheme().compare(
                    QStringLiteral("https"), Qt::CaseInsensitive) == 0
                && !customUrl.host().isEmpty()
            ? customUrl
            : QUrl{};
    }
    case Application::AiCommentProvider::ChatGPT:
    default:
        return QUrl(QStringLiteral("https://chatgpt.com/"));
    }
}

QString aiEligibilityMessage(
    const Application::SpeakingEvaluationAiBatchEligibilityReason reason
    )
{
    using Reason = Application::SpeakingEvaluationAiBatchEligibilityReason;
    switch (reason)
    {
    case Reason::Eligible:
        return {};
    case Reason::MissingName:
        return pageText("Enter the student's name to create an AI prompt.");
    case Reason::UnsupportedGrade:
        return pageText("AI prompts are available for grades E4 through E6.");
    case Reason::MissingDidWellItem:
        return pageText("Add at least one Did Well note.");
    case Reason::MissingNeedsImprovementItem:
        return pageText("Add at least one Needs Improvement note.");
    }
    return {};
}

std::u16string storedEvaluationName(const int index)
{
    return std::u16string(Application::kStoredEvaluationNames[
        static_cast<std::size_t>(index)]);
}

QString evaluationLabel(const std::u16string& name)
{
    if (name == u"Winter") return pageText("Winter");
    if (name == u"Speech Contest") return pageText("Speech Contest");
    if (name == u"Summer") return pageText("Summer");
    if (name == u"Fall") return pageText("Fall");
    return fromUtf16(name);
}

class ActionClassesListReadPort final : public Application::ClassesListReadPort
{
public:
    ActionClassesListReadPort(
        QString path,
        const std::function<Application::ClassesListReadResult(const QString&)>& read
        )
        : m_path(std::move(path))
        , m_read(read)
    {
    }

    [[nodiscard]] Application::ClassesListReadResult readClassesList()
        const override
    {
        return m_read ? m_read(m_path)
            : Application::ClassesListReadResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The classes list reader is unavailable.",
                .recoverable = true
            });
    }

private:
    QString m_path;
    const std::function<Application::ClassesListReadResult(const QString&)>& m_read;
};

class ScoreComboDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        static_cast<void>(option);
        if (index.column() < 3 || index.column() > 8)
        {
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
        auto* editor = new QComboBox(parent);
        editor->setEditable(true);
        editor->addItem(QString{});
        editor->addItems({
            QStringLiteral("A+"), QStringLiteral("A"), QStringLiteral("B+"),
            QStringLiteral("B"), QStringLiteral("C")
        });
        editor->setInsertPolicy(QComboBox::NoInsert);
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto* combo = qobject_cast<QComboBox*>(editor))
        {
            combo->setCurrentText(index.data(Qt::EditRole).toString());
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index
        ) const override
    {
        if (auto* combo = qobject_cast<QComboBox*>(editor))
        {
            static_cast<void>(model->setData(index, combo->currentText(), Qt::EditRole));
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
};

class PrivateNotesDialog final : public QDialog
{
public:
    explicit PrivateNotesDialog(const QString& notes, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(pageText("Private Notes"));
        resize(620, 420);

        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout();
        m_didWell = new QPlainTextEdit(this);
        m_didWell->setObjectName(QStringLiteral("speakingEvaluationDidWellNotes"));
        m_needsImprovement = new QPlainTextEdit(this);
        m_needsImprovement->setObjectName(
            QStringLiteral("speakingEvaluationNeedsImprovementNotes"));
        const auto parsed = Application::splitSpeakingEvaluationPrivateNotes(
            notes.toStdU16String());
        m_didWell->setPlainText(fromUtf16(parsed.didWell));
        m_didWell->setPlaceholderText(
            notesText("Add positive observations…"));
        m_needsImprovement->setPlainText(fromUtf16(parsed.needsImprovement));
        m_needsImprovement->setPlaceholderText(
            notesText("Add areas for improvement…"));
        form->addRow(notesText("Did Well"), m_didWell);
        form->addRow(notesText("Needs Improvement"), m_needsImprovement);
        layout->addLayout(form);

        auto* buttons = new QDialogButtonBox(this);
        QPushButton* const save = buttons->addButton(
            pageText("Save Changes"), QDialogButtonBox::AcceptRole);
        QPushButton* const cancel = buttons->addButton(
            pageText("Cancel"), QDialogButtonBox::RejectRole);
        connect(save, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    [[nodiscard]] QString notes() const
    {
        const QString didWell = m_didWell->toPlainText();
        const QString needsImprovement = m_needsImprovement->toPlainText();
        if (didWell.isEmpty() && needsImprovement.isEmpty())
        {
            return {};
        }
        return QStringLiteral("[Did Well]\n%1\n[Needs Improvement]\n%2")
            .arg(didWell, needsImprovement);
    }

private:
    QPlainTextEdit* m_didWell = nullptr;
    QPlainTextEdit* m_needsImprovement = nullptr;
};

} // namespace

SpeakingEvaluationPage::SpeakingEvaluationPage(
    const PageContext& context,
    SpeakingEvaluationPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_classModel(new SpeakingEvaluationClassListModel(this))
    , m_matrixModel(new SpeakingEvaluationTableModel(this))
    , m_autoSaveTimer(new QTimer(this))
    , m_undoStack(new QUndoStack(this))
{
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(850);
    m_undoStack->setUndoLimit(100);
    m_matrixModel->setUndoStack(m_undoStack);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    auto* heading = new QHBoxLayout();
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("speakingEvaluationTitle"));
    QFont titleFont = m_title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setWeight(QFont::DemiBold);
    m_title->setFont(titleFont);
    m_classSummary = new QLabel(this);
    m_classSummary->setObjectName(QStringLiteral("speakingEvaluationClassSummary"));
    m_classSummary->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_evaluationChoice = new QComboBox(this);
    m_evaluationChoice->setObjectName(QStringLiteral("speakingEvaluationChoice"));
    for (int index = 0; index < static_cast<int>(Application::kStoredEvaluationNames.size());
         ++index)
    {
        const std::u16string name = storedEvaluationName(index);
        m_evaluationChoice->addItem(evaluationLabel(name), fromUtf16(name));
    }
    heading->addWidget(m_title);
    heading->addStretch(1);
    heading->addWidget(m_classSummary);
    heading->addWidget(m_evaluationChoice);
    root->addLayout(heading);

    auto* stateRow = new QHBoxLayout();
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("speakingEvaluationStatus"));
    m_status->setProperty("role", QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_dirtyIndicator = new QLabel(this);
    m_dirtyIndicator->setObjectName(QStringLiteral("speakingEvaluationDirtyState"));
    m_retryButton = new QPushButton(this);
    m_retryButton->setObjectName(QStringLiteral("speakingEvaluationRetry"));
    stateRow->addWidget(m_status, 1);
    stateRow->addWidget(m_dirtyIndicator);
    stateRow->addWidget(m_retryButton);
    root->addLayout(stateRow);

    auto* actionsRow = new QHBoxLayout();
    m_importNamesButton = new QPushButton(this);
    m_importNamesButton->setObjectName(QStringLiteral("speakingEvaluationImportNames"));
    m_editNotesButton = new QPushButton(this);
    m_editNotesButton->setObjectName(QStringLiteral("speakingEvaluationEditNotes"));
    m_aiCommentButton = new QPushButton(this);
    m_aiCommentButton->setObjectName(QStringLiteral("speakingEvaluationAiComment"));
    m_aiBatchCommentsButton = new QPushButton(this);
    m_aiBatchCommentsButton->setObjectName(
        QStringLiteral("speakingEvaluationAiBatchComments"));
    m_undoButton = new QPushButton(this);
    m_undoButton->setObjectName(QStringLiteral("speakingEvaluationUndo"));
    m_saveButton = new QPushButton(this);
    m_saveButton->setObjectName(QStringLiteral("speakingEvaluationSave"));
    actionsRow->addWidget(m_importNamesButton);
    actionsRow->addWidget(m_editNotesButton);
    actionsRow->addWidget(m_aiCommentButton);
    actionsRow->addWidget(m_aiBatchCommentsButton);
    actionsRow->addWidget(m_undoButton);
    actionsRow->addStretch(1);
    actionsRow->addWidget(m_saveButton);
    root->addLayout(actionsRow);

    auto* outputRow = new QHBoxLayout;
    m_reportsButton = new QPushButton(this);
    m_reportsButton->setObjectName(QStringLiteral("speakingEvaluationReports"));
    m_saveReportsButton = new QPushButton(this);
    m_saveReportsButton->setObjectName(QStringLiteral("speakingEvaluationSaveReports"));
    m_printReportsButton = new QPushButton(this);
    m_printReportsButton->setObjectName(QStringLiteral("speakingEvaluationPrintReports"));
    outputRow->addWidget(m_reportsButton); outputRow->addWidget(m_saveReportsButton);
    outputRow->addWidget(m_printReportsButton); outputRow->addStretch();
    root->addLayout(outputRow);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("speakingEvaluationContentSplitter"));
    m_classList = new QListView(splitter);
    m_classList->setObjectName(QStringLiteral("speakingEvaluationClassList"));
    m_classList->setModel(m_classModel);
    m_classList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_classList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_classList->setUniformItemSizes(true);
    m_classList->setMinimumWidth(145);
    m_classList->setMaximumWidth(300);

    m_table = new QTableView(splitter);
    m_table->setObjectName(QStringLiteral("speakingEvaluationMatrix"));
    m_table->setModel(m_matrixModel);
    m_table->setItemDelegate(new ScoreComboDelegate(m_table));
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(true);
    m_table->verticalHeader()->setDefaultSectionSize(34);
    m_table->verticalHeader()->setMinimumWidth(34);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 38);
    m_table->setColumnWidth(1, 130);
    m_table->setColumnWidth(2, 122);
    for (int column = 3; column <= 8; ++column)
    {
        m_table->setColumnWidth(column, 82);
    }
    m_table->setColumnWidth(9, 220);
    m_table->setColumnWidth(10, 180);
    splitter->addWidget(m_classList);
    splitter->addWidget(m_table);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({205, 1000});
    root->addWidget(splitter, 1);

    connect(m_classList->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this](const QModelIndex& current, const QModelIndex&) {
            if (m_syncingControls)
            {
                return;
            }
            const auto classId = m_classModel->classIdAt(current.row());
            if (classId)
            {
                static_cast<void>(selectClass(*classId));
            }
        });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this](const QModelIndex&, const QModelIndex&) {
            updateActions();
        });
    connect(m_evaluationChoice,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](const int index) {
            if (m_syncingControls || index < 0)
            {
                return;
            }
            static_cast<void>(selectEvaluation(m_evaluationChoice->itemData(index).toString()));
        });
    connect(m_matrixModel, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
            if (!m_refreshingValidation)
            {
                refreshValidation();
                updateDirtyState(true);
            }
        });
    connect(m_autoSaveTimer, &QTimer::timeout, this, [this] {
        static_cast<void>(saveCurrentEvaluation(false));
    });
    connect(m_retryButton, &QPushButton::clicked, this, [this] {
        retryCurrentOperation();
    });
    connect(m_importNamesButton, &QPushButton::clicked, this, [this] {
        importRosterNames();
    });
    connect(m_editNotesButton, &QPushButton::clicked, this, [this] {
        editPrivateNotes();
    });
    connect(m_aiCommentButton, &QPushButton::clicked, this, [this] {
        generateAiCommentPrompt();
    });
    connect(m_aiBatchCommentsButton, &QPushButton::clicked, this, [this] {
        generateClassAiComments();
    });
    connect(m_reportsButton, &QPushButton::clicked, this, [this] { openReports(); });
    connect(m_saveReportsButton, &QPushButton::clicked, this, [this] { openReports(1); });
    connect(m_printReportsButton, &QPushButton::clicked, this, [this] { openReports(2); });
    connect(m_undoButton, &QPushButton::clicked, m_undoStack, &QUndoStack::undo);
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(saveCurrentEvaluation(true));
    });
    connect(m_undoStack, &QUndoStack::canUndoChanged, m_undoButton,
        &QPushButton::setEnabled);

    retranslateUi();
    updateActions();
}

SpeakingEvaluationClassListModel* SpeakingEvaluationPage::classModel() const noexcept
{
    return m_classModel;
}

SpeakingEvaluationTableModel* SpeakingEvaluationPage::matrixModel() const noexcept
{
    return m_matrixModel;
}

QListView* SpeakingEvaluationPage::classList() const noexcept
{
    return m_classList;
}

QComboBox* SpeakingEvaluationPage::evaluationChoice() const noexcept
{
    return m_evaluationChoice;
}

QTableView* SpeakingEvaluationPage::tableView() const noexcept
{
    return m_table;
}

QLabel* SpeakingEvaluationPage::statusLabel() const noexcept
{
    return m_status;
}

QPushButton* SpeakingEvaluationPage::retryButton() const noexcept
{
    return m_retryButton;
}

QPushButton* SpeakingEvaluationPage::aiCommentButton() const noexcept
{
    return m_aiCommentButton;
}

QPushButton* SpeakingEvaluationPage::aiBatchCommentsButton() const noexcept
{
    return m_aiBatchCommentsButton;
}

std::optional<Domain::ClassId> SpeakingEvaluationPage::selectedClassId() const
{
    return m_selectedClassId;
}

QString SpeakingEvaluationPage::selectedEvaluationName() const
{
    return m_evaluationName;
}

std::size_t SpeakingEvaluationPage::residentMatrixCount() const noexcept
{
    return m_matrixModel && m_matrixModel->hasMatrix() ? 1U : 0U;
}

bool SpeakingEvaluationPage::isActive() const noexcept
{
    return m_active;
}

bool SpeakingEvaluationPage::hasUnsavedChanges() const noexcept
{
    return m_matrixModel && m_matrixModel->isDirty();
}

PageOperationResult<void> SpeakingEvaluationPage::onActivating()
{
    m_active = true;
    const auto path = activePath();
    if (!path)
    {
        if (hasUnsavedChanges())
        {
            return std::unexpected(PageOperationError{
                pageText("This speaking evaluation has unsaved changes.")});
        }
        clearForClosedWorkspace();
        setStatus(Status::NoWorkspace);
        return {};
    }

    const QString identity = currentWorkspaceIdentity();
    if (!m_workspaceIdentity.isEmpty() && m_workspaceIdentity != identity)
    {
        if (hasUnsavedChanges())
        {
            m_active = false;
            return std::unexpected(PageOperationError{
                pageText("This speaking evaluation has unsaved changes.")});
        }
        clearForWorkspaceChange();
    }
    m_workspaceIdentity = identity;

    if (m_evaluationName.isEmpty())
    {
        m_evaluationName = fromUtf16(storedEvaluationName(0));
    }

    if (!m_summariesLoaded && !loadClassSummaries())
    {
        return {};
    }
    if (m_classModel->summaryCount() == 0)
    {
        m_selectedClassId.reset();
        m_evaluationLoaded = false;
        m_matrixModel->clear();
        setStatus(Status::Empty);
        syncSelectionControls();
        updateActions();
        return {};
    }

    if (!m_selectedClassId
        || m_classModel->rowForClassId(*m_selectedClassId) < 0)
    {
        const auto preferred = m_actions.preferredClassSelection
            ? m_actions.preferredClassSelection()
            : std::optional<Domain::ClassId>{};
        m_selectedClassId = preferred
                && m_classModel->rowForClassId(*preferred) >= 0
            ? preferred
            : m_classModel->classIdAt(0);
    }
    if (m_selectedClassId && m_actions.setActiveClassSelection)
    {
        try
        {
            m_actions.setActiveClassSelection(m_selectedClassId);
        }
        catch (...)
        {
        }
    }
    syncSelectionControls();
    if (!m_evaluationLoaded)
    {
        static_cast<void>(loadEvaluation());
    }
    return {};
}

void SpeakingEvaluationPage::onSuspending() noexcept
{
    m_active = false;
    m_autoSaveTimer->stop();
    // The host resolves a dirty page before suspension. Retain the draft if a
    // caller bypasses that contract; clean matrices are released immediately.
    if (!hasUnsavedChanges())
    {
        releaseActiveDraft();
    }
}

PageOperationResult<void> SpeakingEvaluationPage::saveChanges()
{
    m_autoSaveTimer->stop();
    if (!saveCurrentEvaluation(true))
    {
        return std::unexpected(PageOperationError{
            m_status->text().isEmpty()
                ? pageText("The speaking evaluation could not be saved.")
                : m_status->text()
        });
    }
    return {};
}

PageOperationResult<void> SpeakingEvaluationPage::discardChanges()
{
    m_autoSaveTimer->stop();
    if (!hasUnsavedChanges())
    {
        return {};
    }
    if (!reloadCurrentEvaluation())
    {
        return std::unexpected(PageOperationError{
            pageText("Evaluation data could not be loaded. Please try again.")});
    }
    return {};
}

bool SpeakingEvaluationPage::refreshFromStore()
{
    if (hasUnsavedChanges() && !prepareForSelectionChange(
            pageText("refreshing the class list")))
    {
        return false;
    }
    m_summariesLoaded = false;
    m_selectedClassGrade.reset();
    return loadClassSummaries();
}

void SpeakingEvaluationPage::clearForWorkspaceChange()
{
    if (hasUnsavedChanges())
    {
        setStatus(Status::SaveError,
            pageText("This speaking evaluation has unsaved changes."));
        return;
    }
    m_workspaceIdentity.clear();
    m_selectedClassGrade.reset();
    m_summariesLoaded = false;
    m_classModel->clear();
    m_selectedClassId.reset();
    m_evaluationName.clear();
    m_evaluationLoaded = false;
    m_evaluationReadFailed = false;
    m_matrixModel->clear();
    m_undoStack->clear();
    setHasUnsavedChanges(false);
    setStatus(Status::NoWorkspace);
    syncSelectionControls();
    updateActions();
}

void SpeakingEvaluationPage::clearForClosedWorkspace()
{
    clearForWorkspaceChange();
}

void SpeakingEvaluationPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
        m_matrixModel->retranslate();
        setStatus(m_statusKind);
        syncSelectionControls();
    }
    else if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::FontChange)
    {
        updateActionButtonWidths();
    }
    Page::changeEvent(event);
}

std::optional<QString> SpeakingEvaluationPage::activePath() const
{
    if (!m_actions.activeWorkspacePath)
    {
        return std::nullopt;
    }
    try
    {
        const auto path = m_actions.activeWorkspacePath();
        if (!path || path->trimmed().isEmpty())
        {
            return std::nullopt;
        }
        return path;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

QString SpeakingEvaluationPage::currentWorkspaceIdentity() const
{
    if (m_actions.workspaceIdentity)
    {
        try
        {
            const QString identity = m_actions.workspaceIdentity();
            if (!identity.isEmpty())
            {
                return identity;
            }
        }
        catch (...)
        {
        }
    }
    const auto path = activePath();
    return path.value_or(QString{});
}

bool SpeakingEvaluationPage::loadClassSummaries()
{
    const auto path = activePath();
    if (!path)
    {
        m_classModel->clear();
        m_matrixModel->clear();
        m_summariesLoaded = false;
        setStatus(Status::NoWorkspace);
        return false;
    }
    try
    {
        ActionClassesListReadPort readPort(*path, m_actions.readClasses);
        Application::ClassesListReadQuery query(readPort);
        const Application::ClassesListReadResult loaded = query.execute();
        if (!loaded)
        {
            m_classModel->clear();
            m_matrixModel->clear();
            m_summariesLoaded = false;
            m_evaluationLoaded = false;
            m_selectedClassId.reset();
            setStatus(Status::ClassListError, errorText(loaded.error().message));
            syncSelectionControls();
            updateActions();
            return false;
        }

        const std::optional<Domain::ClassId> previous = m_selectedClassId;
        m_classModel->setClasses(std::move(loaded.value()));
        m_summariesLoaded = true;
        const auto preferred = !previous && m_actions.preferredClassSelection
            ? m_actions.preferredClassSelection()
            : std::optional<Domain::ClassId>{};
        m_selectedClassId = previous
                && m_classModel->rowForClassId(*previous) >= 0
            ? previous
            : preferred && m_classModel->rowForClassId(*preferred) >= 0
                ? preferred
                : m_classModel->classIdAt(0);
        if (previous != m_selectedClassId)
        {
            m_selectedClassGrade.reset();
        }
        syncSelectionControls();
        if (m_classModel->summaryCount() == 0)
        {
            m_matrixModel->clear();
            m_evaluationLoaded = false;
            setStatus(Status::Empty);
            updateActions();
            return true;
        }
        if (m_selectedClassId)
        {
            m_evaluationLoaded = false;
            static_cast<void>(loadEvaluation());
        }
        return true;
    }
    catch (...)
    {
        m_classModel->clear();
        m_matrixModel->clear();
        m_summariesLoaded = false;
        setStatus(Status::ClassListError);
        syncSelectionControls();
        updateActions();
        return false;
    }
}

bool SpeakingEvaluationPage::loadEvaluation()
{
    m_autoSaveTimer->stop();
    m_matrixModel->clear();
    m_undoStack->clear();
    m_evaluationLoaded = false;
    m_evaluationReadFailed = false;
    if (!m_selectedClassId || m_evaluationName.isEmpty())
    {
        setStatus(Status::Empty);
        updateActions();
        return false;
    }
    const auto path = activePath();
    if (!path)
    {
        setStatus(Status::NoWorkspace);
        updateActions();
        return false;
    }
    if (!m_actions.evaluationReadPortFactory)
    {
        m_evaluationReadFailed = true;
        setStatus(Status::EvaluationReadError);
        updateActions();
        return false;
    }

    try
    {
        const Application::SpeakingEvaluationReadQuery query{
            .classId = *m_selectedClassId,
            .evaluationName = m_evaluationName.toStdU16String()
        };
        std::unique_ptr<Application::SpeakingEvaluationReadPort> port =
            m_actions.evaluationReadPortFactory(*path);
        if (!port)
        {
            m_evaluationReadFailed = true;
            setStatus(Status::EvaluationReadError);
            updateActions();
            return false;
        }
        const Application::SpeakingEvaluationReadResult loaded =
            Application::SpeakingEvaluationQuery::execute(query, *port);
        if (!loaded || !m_matrixModel->loadRows(loaded.value().rows))
        {
            m_evaluationReadFailed = true;
            setStatus(Status::EvaluationReadError,
                loaded ? pageText("Evaluation data could not be loaded. Please try again.")
                       : errorText(loaded.error().message));
            updateActions();
            return false;
        }
        m_matrixModel->markSaved();
        m_evaluationLoaded = true;
        setStatus(Status::Ready);
        refreshValidation();
        updateActions();
        return true;
    }
    catch (...)
    {
        m_evaluationReadFailed = true;
        setStatus(Status::EvaluationReadError);
        updateActions();
        return false;
    }
}

bool SpeakingEvaluationPage::selectClass(const Domain::ClassId& classId)
{
    if (m_selectedClassId && *m_selectedClassId == classId)
    {
        return true;
    }
    if (!prepareForSelectionChange(pageText("changing classes")))
    {
        syncSelectionControls();
        return false;
    }
    m_selectedClassId = classId;
    m_selectedClassGrade.reset();
    if (m_actions.setActiveClassSelection)
    {
        try
        {
            m_actions.setActiveClassSelection(m_selectedClassId);
        }
        catch (...)
        {
        }
    }
    syncSelectionControls();
    return loadEvaluation();
}

bool SpeakingEvaluationPage::selectEvaluation(const QString& evaluationName)
{
    if (m_evaluationName == evaluationName)
    {
        return true;
    }
    if (!prepareForSelectionChange(pageText("changing evaluations")))
    {
        syncSelectionControls();
        return false;
    }
    m_evaluationName = evaluationName;
    syncSelectionControls();
    return loadEvaluation();
}

bool SpeakingEvaluationPage::prepareForSelectionChange(const QString& reason)
{
    if (!hasUnsavedChanges())
    {
        return true;
    }
    switch (askUnsavedChanges(reason))
    {
    case SpeakingEvaluationUnsavedChoice::Cancel:
        return false;
    case SpeakingEvaluationUnsavedChoice::Save:
        return saveCurrentEvaluation(true);
    case SpeakingEvaluationUnsavedChoice::Discard:
        return dropDraftForSelectionChange();
    }
    return false;
}

bool SpeakingEvaluationPage::dropDraftForSelectionChange()
{
    m_autoSaveTimer->stop();
    m_matrixModel->clear();
    m_undoStack->clear();
    m_evaluationLoaded = false;
    m_evaluationReadFailed = false;
    setHasUnsavedChanges(false);
    updateActions();
    return true;
}

bool SpeakingEvaluationPage::saveCurrentEvaluation(const bool interactive)
{
    m_autoSaveTimer->stop();
    if (!hasUnsavedChanges())
    {
        updateActions();
        return true;
    }
    if (!m_evaluationLoaded || !m_selectedClassId || m_evaluationName.isEmpty())
    {
        setStatus(Status::EvaluationReadError);
        updateActions();
        return false;
    }
    const auto path = activePath();
    if (!path || !m_actions.evaluationSavePortFactory)
    {
        setStatus(Status::SaveError);
        updateActions();
        return false;
    }

    Application::SpeakingEvaluationSaveRequest request{
        .classId = *m_selectedClassId,
        .evaluationName = m_evaluationName.toStdU16String(),
        .evaluation = m_matrixModel->snapshot(),
        .allowQuestionableKoreanNameLengths = false
    };
    Application::SpeakingEvaluationValidationResult validation =
        Application::validateAndNormalizeSpeakingEvaluation(request);
    if (validation.hasErrors())
    {
        if (Application::hasOnlyQuestionableKoreanNameLengthErrors(validation)
            && interactive && askQuestionableKoreanNameLengths())
        {
            request.allowQuestionableKoreanNameLengths = true;
            validation = Application::validateAndNormalizeSpeakingEvaluation(request);
        }
        if (validation.hasErrors())
        {
            m_matrixModel->setValidationIssues(std::move(validation.issues));
            setStatus(Status::ValidationError);
            updateActions();
            return false;
        }
    }

    try
    {
        std::unique_ptr<Application::SpeakingEvaluationSavePort> port =
            m_actions.evaluationSavePortFactory(*path);
        if (!port)
        {
            setStatus(Status::SaveError);
            updateActions();
            return false;
        }
        const Domain::Result<void> saved =
            Application::SpeakingEvaluationSaveUseCase::execute(request, *port);
        if (!saved)
        {
            setStatus(Status::SaveError, errorText(saved.error().message));
            updateActions();
            return false;
        }
        m_matrixModel->markSaved();
        setHasUnsavedChanges(false);
        setStatus(Status::Ready, pageText("Speaking evaluation saved."));
        refreshValidation();
        updateActions();
        return true;
    }
    catch (...)
    {
        setStatus(Status::SaveError);
        updateActions();
        return false;
    }
}

bool SpeakingEvaluationPage::reloadCurrentEvaluation()
{
    if (!m_selectedClassId || m_evaluationName.isEmpty()
        || !m_actions.evaluationReadPortFactory)
    {
        setStatus(Status::EvaluationReadError);
        return false;
    }
    const auto path = activePath();
    if (!path)
    {
        setStatus(Status::NoWorkspace);
        return false;
    }
    try
    {
        const Application::SpeakingEvaluationReadQuery query{
            .classId = *m_selectedClassId,
            .evaluationName = m_evaluationName.toStdU16String()
        };
        auto port = m_actions.evaluationReadPortFactory(*path);
        if (!port)
        {
            setStatus(Status::EvaluationReadError);
            return false;
        }
        const auto loaded = Application::SpeakingEvaluationQuery::execute(query, *port);
        if (!loaded || loaded.value().rows.size()
                > static_cast<std::size_t>(Application::SpeakingEvaluationRowCount)
            || !std::ranges::all_of(loaded.value().rows, [](const auto& row) {
                return row.size()
                    <= static_cast<std::size_t>(Application::SpeakingEvaluationColumnCount);
            }))
        {
            setStatus(Status::EvaluationReadError);
            return false;
        }
        if (!m_matrixModel->loadRows(loaded.value().rows))
        {
            setStatus(Status::EvaluationReadError);
            return false;
        }
        m_undoStack->clear();
        m_matrixModel->markSaved();
        m_evaluationLoaded = true;
        m_evaluationReadFailed = false;
        setHasUnsavedChanges(false);
        refreshValidation();
        setStatus(Status::Ready);
        updateActions();
        return true;
    }
    catch (...)
    {
        setStatus(Status::EvaluationReadError);
        return false;
    }
}

void SpeakingEvaluationPage::retryCurrentOperation()
{
    if (m_statusKind == Status::ClassListError)
    {
        static_cast<void>(loadClassSummaries());
    }
    else if (m_evaluationReadFailed)
    {
        static_cast<void>(loadEvaluation());
    }
    else if (m_statusKind == Status::AiPreparationError)
    {
        if (loadSelectedClassGrade())
        {
            setStatus(Status::Ready);
        }
    }
    else if (m_statusKind == Status::ReportPreparationError)
    {
        openReports(m_reportRetryMode);
    }
    else
    {
        static_cast<void>(saveCurrentEvaluation(true));
    }
}

void SpeakingEvaluationPage::importRosterNames()
{
    if (!m_evaluationLoaded || !m_selectedClassId
        || !m_actions.rosterNamesReadPortFactory)
    {
        return;
    }
    const auto path = activePath();
    if (!path)
    {
        setStatus(Status::ImportError);
        return;
    }
    try
    {
        auto port = m_actions.rosterNamesReadPortFactory(*path);
        if (!port)
        {
            setStatus(Status::ImportError);
            return;
        }
        const Application::SpeakingEvaluationRosterNamesReadRequest request{
            .classId = *m_selectedClassId
        };
        const auto loaded = Application::SpeakingEvaluationRosterNamesReadQuery::
            execute(request, *port);
        if (!loaded)
        {
            setStatus(Status::ImportError, errorText(loaded.error().message));
            return;
        }
        if (!loaded.value().hasEnglishColumn || !loaded.value().hasKoreanColumn)
        {
            setStatus(Status::ImportError,
                pageText("Roster must contain 'English' and 'Korean' columns."));
            return;
        }
        if (loaded.value().rows.empty())
        {
            setStatus(Status::ImportError, pageText("No roster data found."));
            return;
        }

        std::vector<Application::SpeakingEvaluationRosterNamePair> rosterNames;
        rosterNames.reserve(loaded.value().rows.size());
        for (const auto& names : loaded.value().rows)
        {
            rosterNames.push_back({names.englishName, names.koreanName});
        }
        std::vector<Application::SpeakingEvaluationRosterNameEvaluationRow>
            evaluationRows;
        evaluationRows.reserve(Application::SpeakingEvaluationRowCount);
        for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
        {
            evaluationRows.push_back({
                Application::SpeakingEvaluationRosterNameImportTargetIndex{row},
                {
                    m_matrixModel->cellText(row, 1).toStdU16String(),
                    m_matrixModel->cellText(row, 2).toStdU16String()
                }
            });
        }
        const auto assignments = Application::planSpeakingEvaluationRosterNameImport(
            rosterNames, evaluationRows);
        if (assignments.empty())
        {
            setStatus(Status::Ready, pageText("Names are already up to date."));
            return;
        }

        m_undoStack->beginMacro(pageText("Import Names"));
        for (const auto& assignment : assignments)
        {
            const int row = assignment.targetRow.rawValue();
            static_cast<void>(m_matrixModel->setData(
                m_matrixModel->index(row, 1), fromUtf16(assignment.names.englishName)));
            static_cast<void>(m_matrixModel->setData(
                m_matrixModel->index(row, 2), fromUtf16(assignment.names.koreanName)));
        }
        m_undoStack->endMacro();
        setStatus(Status::Ready, pageText("Roster names imported successfully."));
        updateDirtyState(true);
    }
    catch (...)
    {
        setStatus(Status::ImportError);
    }
}

void SpeakingEvaluationPage::editPrivateNotes()
{
    if (!m_evaluationLoaded || !m_matrixModel->hasMatrix())
    {
        return;
    }
    const int row = m_table->currentIndex().row();
    if (row < 0 || row >= Application::SpeakingEvaluationRowCount)
    {
        setStatus(Status::Ready, pageText("Select a student row first."));
        return;
    }
    PrivateNotesDialog dialog(
        m_matrixModel->cellText(row, 10), this);
    if (dialog.exec() == QDialog::Accepted)
    {
        static_cast<void>(m_matrixModel->setData(
            m_matrixModel->index(row, 10), dialog.notes(), Qt::EditRole));
    }
}

bool SpeakingEvaluationPage::loadSelectedClassGrade()
{
    if (m_selectedClassGrade)
    {
        return true;
    }
    if (!m_selectedClassId || !m_actions.classGradeReadPortFactory)
    {
        setStatus(Status::AiPreparationError,
            pageText("The class grade could not be loaded. Retry to use AI comments."));
        return false;
    }
    const auto path = activePath();
    if (!path)
    {
        setStatus(Status::AiPreparationError,
            pageText("The class grade could not be loaded. Retry to use AI comments."));
        return false;
    }

    try
    {
        std::unique_ptr<Application::SelectedClassGradeReadPort> port =
            m_actions.classGradeReadPortFactory(*path);
        if (!port)
        {
            setStatus(Status::AiPreparationError,
                pageText("The class grade could not be loaded. Retry to use AI comments."));
            return false;
        }
        const Application::SelectedClassGradeReadQuery query(*port);
        const Application::SelectedClassGradeReadResult loaded =
            query.execute(*m_selectedClassId);
        if (!loaded)
        {
            setStatus(Status::AiPreparationError,
                errorText(loaded.error().message));
            return false;
        }

        const std::string& storedGrade = loaded.value().classGrade.value();
        const QString classGrade = QString::fromUtf8(
            storedGrade.data(), static_cast<qsizetype>(storedGrade.size()));
        const int elementaryGrade = Application::speakingEvaluationElementaryGrade(
            classGrade.toStdU16String());
        m_selectedClassGrade =
            Application::speakingEvaluationCourseGradeBandFromInteger(
                elementaryGrade);
        return true;
    }
    catch (...)
    {
        setStatus(Status::AiPreparationError,
            pageText("The class grade could not be loaded. Retry to use AI comments."));
        return false;
    }
}

Application::SpeakingEvaluationAiBatchEligibilityReason
SpeakingEvaluationPage::aiEligibilityForRow(const int row) const
{
    const QString englishName = m_matrixModel->cellText(row, 1).trimmed();
    const QString koreanName = m_matrixModel->cellText(row, 2).trimmed();
    const Application::SpeakingEvaluationPrivateNotes notes =
        Application::splitSpeakingEvaluationPrivateNotes(
            m_matrixModel->cellText(row, 10).toStdU16String());
    return Application::speakingEvaluationAiBatchEligibilityReason({
        .hasTrimmedStudentName =
            !englishName.isEmpty() || !koreanName.isEmpty(),
        .grade = m_selectedClassGrade.value_or(Domain::CourseGradeBand::Other),
        .hasDidWellItem = !Application::speakingEvaluationAiObservationItems(
            notes.didWell).empty(),
        .hasNeedsImprovementItem = !Application::speakingEvaluationAiObservationItems(
            notes.needsImprovement).empty()
    });
}

QString SpeakingEvaluationPage::aiProviderName() const
{
    Application::AiCommentProvider provider =
        Application::AiCommentProvider::ChatGPT;
    if (m_actions.preferredAiCommentProvider)
    {
        try
        {
            provider = m_actions.preferredAiCommentProvider();
        }
        catch (...)
        {
        }
    }
    return aiProviderDisplayName(provider);
}

QUrl SpeakingEvaluationPage::aiProviderUrl() const
{
    Application::AiCommentProvider provider =
        Application::AiCommentProvider::ChatGPT;
    if (m_actions.preferredAiCommentProvider)
    {
        try
        {
            provider = m_actions.preferredAiCommentProvider();
        }
        catch (...)
        {
        }
    }
    QString customWebsiteUrl;
    if (m_actions.customAiCommentWebsiteUrl)
    {
        try
        {
            customWebsiteUrl = m_actions.customAiCommentWebsiteUrl();
        }
        catch (...)
        {
        }
    }
    return aiProviderWebsiteUrl(provider, customWebsiteUrl);
}

void SpeakingEvaluationPage::generateAiCommentPrompt(const bool copyAndOpen)
{
    if (!m_evaluationLoaded || !m_matrixModel->hasMatrix())
    {
        return;
    }
    const int row = m_table->currentIndex().row();
    if (row < 0 || row >= Application::SpeakingEvaluationRowCount)
    {
        setStatus(Status::Ready, pageText("Select a student row first."));
        return;
    }
    if (!loadSelectedClassGrade())
    {
        return;
    }

    const auto reason = aiEligibilityForRow(row);
    if (reason != Application::SpeakingEvaluationAiBatchEligibilityReason::Eligible)
    {
        setStatus(Status::Ready, aiEligibilityMessage(reason));
        return;
    }

    const Application::SpeakingEvaluationPrivateNotes notes =
        Application::splitSpeakingEvaluationPrivateNotes(
            m_matrixModel->cellText(row, 10).toStdU16String());
    Application::AiCommentVoice voice = Application::AiCommentVoice::DirectToStudent;
    if (m_actions.preferredAiCommentVoice)
    {
        try
        {
            voice = m_actions.preferredAiCommentVoice();
        }
        catch (...)
        {
        }
    }
    const Application::SpeakingEvaluationAiCommentPromptRequest request{
        .grade = m_selectedClassGrade.value_or(Domain::CourseGradeBand::Other),
        .englishName = m_matrixModel->cellText(row, 1).toStdU16String(),
        .koreanName = m_matrixModel->cellText(row, 2).toStdU16String(),
        .didWell = notes.didWell,
        .needsImprovement = notes.needsImprovement,
        .voice = voice
    };
    const Platform::SpeakingEvaluationQtTextMatchPort textMatcher;
    const Application::SpeakingEvaluationAiCommentPromptResult result =
        Application::buildSpeakingEvaluationAiCommentPrompt(request, textMatcher);
    if (!result.valid)
    {
        setStatus(Status::Ready,
            pageText("An AI comment prompt could not be created from these notes."));
        return;
    }

    if (copyAndOpen)
    {
        QApplication::clipboard()->setText(QString::fromStdU16String(result.prompt));
        const QUrl url = aiProviderUrl();
        if (url.scheme() != QStringLiteral("https") || !(m_actions.openExternalUrl
                ? m_actions.openExternalUrl(url) : QDesktopServices::openUrl(url)))
            QMessageBox::warning(this, pageText("Unable to Open AI Website"),
                pageText("The AI website could not be opened. The prompt is still available on the clipboard."));
        return;
    }
    SpeakingEvaluationAiPromptDialog dialog(
        QString::fromStdU16String(result.prompt),
        aiProviderName(),
        aiProviderUrl(),
        m_actions.openExternalUrl,
        this);
    static_cast<void>(dialog.exec());
}

void SpeakingEvaluationPage::openReports(const int mode)
{
    m_reportRetryMode = mode;
    if (!m_evaluationLoaded || !m_selectedClassId || !m_matrixModel->hasMatrix()) return;
    const auto path = activePath();
    if (!path || !m_actions.reportContextReadPortFactory)
    { setStatus(Status::ReportPreparationError); return; }
    try
    {
        auto port = m_actions.reportContextReadPortFactory(*path);
        if (!port) { setStatus(Status::ReportPreparationError); return; }
        const auto context = port->readReportContext(*m_selectedClassId);
        if (!context)
        { setStatus(Status::ReportPreparationError, errorText(context.error().message)); return; }
        ClassInfo classInfo;
        classInfo.classGrade = fromUtf16(context.value().classValues.grade);
        classInfo.classLevel = fromUtf16(context.value().classValues.level);
        classInfo.teacherEn = fromUtf16(context.value().classValues.nativeTeacher);
        classInfo.teacherKr = fromUtf16(context.value().classValues.koreanTeacher);
        for (const auto& time : context.value().regularTimes)
            classInfo.classTimes.append({fromUtf16(time.day), fromUtf16(time.startTime), fromUtf16(time.endTime)});
        SpeakingEvalRows rows;
        for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
        {
            QStringList values;
            for (int column = 0; column < m_matrixModel->columnCount(); ++column)
                values.append(m_matrixModel->cellText(row, column));
            rows.append(std::move(values));
        }
        auto reports = buildSpeakingEvalStudentReports(rows, classInfo,
            QByteArray::fromStdString(context.value().signatureImage));
        rows.clear(); rows.squeeze();
        if (reports.isEmpty()) { setStatus(Status::Ready); return; }
        auto actions = m_actions.reports;
        auto& loader = resourceContext().resourceLoader();
        const auto assets = Output::resolveSpeakingEvaluationReportAssets(loader, reports.constFirst().report.reportTemplate);
        if (!assets) { setStatus(Status::ReportPreparationError, assets.error()); return; }
        const QString latin = actions.assets.latinFamily, korean = actions.assets.koreanFamily;
        actions.assets = *assets; actions.assets.latinFamily = latin; actions.assets.koreanFamily = korean;
        if (!actions.run.resolvePowerPointRoot) actions.run.resolvePowerPointRoot = [&loader](SpeakingEvalReportTemplate kind) {
            return Output::resolveSpeakingEvaluationPowerPointRoot(loader, kind);
        };
        actions.valueEdited = [this](int row, int column, const QString& value) {
            static_cast<void>(m_matrixModel->setData(m_matrixModel->index(row, column), value));
        };
        actions.previewAiPrompt = [this](int row) {
            m_table->setCurrentIndex(m_matrixModel->index(row, 9)); generateAiCommentPrompt();
        };
        actions.copyAndOpenAiPrompt = [this](int row) {
            m_table->setCurrentIndex(m_matrixModel->index(row, 9)); generateAiCommentPrompt(true);
        };
        const int current = std::max(0, speakingEvalReportIndexForSourceRow(reports, m_table->currentIndex().row()));
        setStatus(Status::Ready);
        if (mode == 0)
        {
            SpeakingEvaluationReportsDialog dialog(std::move(reports), current, std::move(actions), this);
            dialog.exec();
        }
        else
        {
            SpeakingEvaluationReportBatchDialog dialog(reports, current,
                SpeakingEvalBatchReportService::defaultOutputDirectory(classInfo, m_evaluationName),
                mode == 1 ? SpeakingEvaluationReportBatchDialog::Mode::SaveAs : SpeakingEvaluationReportBatchDialog::Mode::Print,
                std::move(actions), this);
            dialog.exec();
        }
    }
    catch (...) { setStatus(Status::ReportPreparationError); }
}

void SpeakingEvaluationPage::generateClassAiComments()
{
    if (!m_evaluationLoaded || !m_selectedClassId || !m_matrixModel->hasMatrix())
    {
        return;
    }
    if (!loadSelectedClassGrade())
    {
        return;
    }

    std::vector<SpeakingEvaluationAiBatchStudent> students;
    students.reserve(Application::SpeakingEvaluationRowCount);
    for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
    {
        const QString englishName = m_matrixModel->cellText(row, 1).trimmed();
        const QString koreanName = m_matrixModel->cellText(row, 2).trimmed();
        if (englishName.isEmpty() && koreanName.isEmpty())
        {
            continue;
        }
        const Application::SpeakingEvaluationPrivateNotes notes =
            Application::splitSpeakingEvaluationPrivateNotes(
                m_matrixModel->cellText(row, 10).toStdU16String());
        const int id = static_cast<int>(students.size()) + 1;
        students.push_back({
            .sourceRow = row,
            .id = QStringLiteral("STUDENT_%1")
                .arg(id, 2, 10, QLatin1Char('0')),
            .englishName = m_matrixModel->cellText(row, 1),
            .koreanName = m_matrixModel->cellText(row, 2),
            .oldComment = m_matrixModel->cellText(row, 9),
            .didWell = fromUtf16(notes.didWell),
            .needsImprovement = fromUtf16(notes.needsImprovement),
            .grade = m_selectedClassGrade.value_or(Domain::CourseGradeBand::Other),
            .eligibilityReason = aiEligibilityForRow(row)
        });
    }

    Application::AiCommentVoice voice = Application::AiCommentVoice::DirectToStudent;
    if (m_actions.preferredAiCommentVoice)
    {
        try
        {
            voice = m_actions.preferredAiCommentVoice();
        }
        catch (...)
        {
        }
    }
    SpeakingEvaluationAiBatchDialog dialog(
        std::move(students),
        voice,
        aiProviderName(),
        aiProviderUrl(),
        m_actions.openExternalUrl,
        this);
    if (dialog.exec() != QDialog::Accepted
        || dialog.acceptedComments().empty())
    {
        return;
    }

    std::vector<SpeakingEvaluationAiBatchAcceptedComment> changes;
    changes.reserve(dialog.acceptedComments().size());
    std::size_t staleCount = 0;
    for (const SpeakingEvaluationAiBatchAcceptedComment& accepted :
         dialog.acceptedComments())
    {
        if (accepted.sourceRow < 0
            || accepted.sourceRow >= Application::SpeakingEvaluationRowCount)
        {
            continue;
        }
        if (m_matrixModel->cellText(accepted.sourceRow, 9) != accepted.oldComment)
        {
            ++staleCount;
            continue;
        }
        if (accepted.oldComment != accepted.newComment)
        {
            changes.push_back(accepted);
        }
    }
    if (changes.empty())
    {
        if (staleCount > 0)
        {
            setStatus(Status::Ready,
                pageText("Some comments changed while they were being reviewed."));
        }
        return;
    }

    m_undoStack->beginMacro(pageText("Apply AI Comments"));
    for (const SpeakingEvaluationAiBatchAcceptedComment& change : changes)
    {
        static_cast<void>(m_matrixModel->setData(
            m_matrixModel->index(change.sourceRow, 9),
            change.newComment,
            Qt::EditRole));
    }
    m_undoStack->endMacro();
    setStatus(Status::Ready, staleCount == 0
        ? pageText("AI comments were added to the evaluation.")
        : pageText("Some AI comments were added; other comments had changed."));
    updateDirtyState(true);
}

void SpeakingEvaluationPage::refreshValidation()
{
    if (!m_matrixModel->hasMatrix() || !m_selectedClassId
        || m_evaluationName.isEmpty())
    {
        return;
    }
    m_refreshingValidation = true;
    const Application::SpeakingEvaluationSaveRequest request{
        .classId = *m_selectedClassId,
        .evaluationName = m_evaluationName.toStdU16String(),
        .evaluation = m_matrixModel->snapshot(),
        .allowQuestionableKoreanNameLengths = false
    };
    const auto validation = Application::validateAndNormalizeSpeakingEvaluation(request);
    m_matrixModel->setValidationIssues(validation.issues);
    m_refreshingValidation = false;
    if (!validation.hasErrors() && m_statusKind == Status::ValidationError)
    {
        setStatus(Status::Ready);
    }
}

void SpeakingEvaluationPage::updateDirtyState(const bool scheduleSave)
{
    const bool dirty = hasUnsavedChanges();
    setHasUnsavedChanges(dirty);
    if (scheduleSave && dirty && m_active && m_evaluationLoaded
        && m_actions.saveMode == Application::SaveMode::Automatic)
    {
        m_autoSaveTimer->start();
    }
    else if (!dirty || m_actions.saveMode == Application::SaveMode::Manual)
    {
        m_autoSaveTimer->stop();
    }
    updateActions();
}

void SpeakingEvaluationPage::updateActions()
{
    const bool matrixReady = m_evaluationLoaded && m_matrixModel->hasMatrix();
    m_importNamesButton->setEnabled(matrixReady);
    m_editNotesButton->setEnabled(matrixReady);
    const int currentStudentRow = m_table->currentIndex().row();
    const bool studentRowSelected = currentStudentRow >= 0
        && currentStudentRow < Application::SpeakingEvaluationRowCount;
    m_aiCommentButton->setEnabled(matrixReady && studentRowSelected);
    m_aiCommentButton->setToolTip(studentRowSelected
        ? QString{}
        : pageText("Select a student row to create an AI comment prompt."));
    m_aiBatchCommentsButton->setEnabled(matrixReady);
    bool hasReportStudent = false;
    if (matrixReady) for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
        if (!m_matrixModel->cellText(row, 1).trimmed().isEmpty() || !m_matrixModel->cellText(row, 2).trimmed().isEmpty())
        { hasReportStudent = true; break; }
    m_reportsButton->setEnabled(matrixReady && hasReportStudent);
    m_saveReportsButton->setEnabled(matrixReady && hasReportStudent);
    m_printReportsButton->setEnabled(matrixReady && hasReportStudent);
    m_undoButton->setEnabled(m_undoStack->canUndo());
    m_saveButton->setEnabled(matrixReady && hasUnsavedChanges());
    m_retryButton->setVisible(m_statusKind == Status::ClassListError
        || m_statusKind == Status::EvaluationReadError
        || m_statusKind == Status::SaveError
        || m_statusKind == Status::AiPreparationError
        || m_statusKind == Status::ReportPreparationError);
    m_evaluationChoice->setEnabled(m_classModel->summaryCount() > 0);
    m_classList->setEnabled(m_classModel->summaryCount() > 0);
    m_dirtyIndicator->setText(hasUnsavedChanges()
        ? pageText("Unsaved changes")
        : QString{});
}

void SpeakingEvaluationPage::setStatus(
    const Status status,
    const QString& message
    )
{
    m_statusKind = status;
    m_status->setToolTip({});
    if (!message.isEmpty())
    {
        m_status->setText(message);
        if (status == Status::EvaluationReadError
            || status == Status::ClassListError
            || status == Status::SaveError
            || status == Status::ImportError
            || status == Status::AiPreparationError
            || status == Status::ReportPreparationError)
        {
            m_status->setToolTip(message);
        }
    }
    else
    {
        switch (status)
        {
        case Status::Ready:
            m_status->clear();
            break;
        case Status::NoWorkspace:
            m_status->setText(pageText("No Teacher Profile is open."));
            break;
        case Status::Empty:
            m_status->setText(pageText("No classes available"));
            break;
        case Status::ClassListError:
            m_status->setText(pageText("Evaluation data could not be loaded. Please try again."));
            break;
        case Status::EvaluationReadError:
            m_status->setText(pageText("Evaluation data could not be loaded. Please try again."));
            break;
        case Status::SaveError:
            m_status->setText(pageText("The speaking evaluation could not be saved."));
            break;
        case Status::ValidationError:
            m_status->setText(pageText("Fix validation errors before saving."));
            break;
        case Status::ImportError:
            m_status->setText(pageText("Roster names could not be imported."));
            break;
        case Status::AiPreparationError:
            m_status->setText(pageText(
                "The class grade could not be loaded. Retry to use AI comments."));
            break;
        case Status::ReportPreparationError:
            m_status->setText(pageText("Report data could not be loaded. Please try again."));
            break;
        }
    }
    m_retryButton->setText(pageText("Retry"));
    updateActions();
}

void SpeakingEvaluationPage::syncSelectionControls()
{
    m_syncingControls = true;
    if (m_selectedClassId)
    {
        const int row = m_classModel->rowForClassId(*m_selectedClassId);
        if (row >= 0)
        {
            m_classList->setCurrentIndex(m_classModel->index(row, 0));
            const auto& entry = m_classModel->index(row, 0)
                .data(Qt::DisplayRole).toString();
            m_classSummary->setText(pageText("Class %1").arg(entry));
        }
        else
        {
            m_classSummary->clear();
        }
    }
    else
    {
        m_classList->setCurrentIndex({});
        m_classSummary->clear();
    }

    if (!m_evaluationName.isEmpty())
    {
        const int evaluationIndex = m_evaluationChoice->findData(m_evaluationName);
        if (evaluationIndex >= 0)
        {
            m_evaluationChoice->setCurrentIndex(evaluationIndex);
        }
    }
    m_syncingControls = false;
    updateActions();
}

void SpeakingEvaluationPage::retranslateUi()
{
    m_title->setText(pageText("Speaking Evaluation"));
    m_importNamesButton->setText(pageText("Import Names"));
    m_editNotesButton->setText(pageText("Edit Private Notes"));
    m_aiCommentButton->setText(pageText("AI Comment"));
    m_aiBatchCommentsButton->setText(pageText("Class AI Comments"));
    m_reportsButton->setText(pageText("Reports"));
    m_saveReportsButton->setText(pageText("Save Reports As…"));
    m_printReportsButton->setText(pageText("Print Reports"));
    m_undoButton->setText(pageText("Undo"));
    m_saveButton->setText(pageText("Save Changes"));
    m_retryButton->setText(pageText("Retry"));
    updateActionButtonWidths();
    for (int index = 0; index < m_evaluationChoice->count(); ++index)
    {
        const QString storedName = m_evaluationChoice->itemData(index).toString();
        m_evaluationChoice->setItemText(index,
            evaluationLabel(storedName.toStdU16String()));
    }
}

void SpeakingEvaluationPage::updateActionButtonWidths()
{
    const std::array<QPushButton*, 10> buttons{
        m_importNamesButton,
        m_editNotesButton,
        m_aiCommentButton,
        m_aiBatchCommentsButton,
        m_reportsButton,
        m_saveReportsButton,
        m_printReportsButton,
        m_undoButton,
        m_saveButton,
        m_retryButton
    };
    for (QPushButton* const button : buttons)
    {
        if (!button)
        {
            continue;
        }
        QStyleOptionButton option;
        option.initFrom(button);
        option.text = button->text();
        const QSize textSize = button->fontMetrics().size(
            Qt::TextShowMnemonic, button->text());
        const QSize required = button->style()->sizeFromContents(
            QStyle::CT_PushButton, &option, textSize, button);
        button->setMinimumWidth(required.width());
    }
}

void SpeakingEvaluationPage::releaseActiveDraft()
{
    m_matrixModel->clear();
    m_undoStack->clear();
    m_evaluationLoaded = false;
    m_evaluationReadFailed = false;
    setHasUnsavedChanges(false);
    updateActions();
}

SpeakingEvaluationUnsavedChoice SpeakingEvaluationPage::askUnsavedChanges(
    const QString& reason
    ) const
{
    if (m_actions.resolveUnsavedChanges)
    {
        try
        {
            return m_actions.resolveUnsavedChanges(reason);
        }
        catch (...)
        {
            return SpeakingEvaluationUnsavedChoice::Cancel;
        }
    }
    QMessageBox prompt(QMessageBox::Warning,
        pageText("Unsaved Speaking Evaluation Changes"),
        pageText("This speaking evaluation has unsaved changes.")
            + QStringLiteral("\n") + reason,
        QMessageBox::NoButton,
        const_cast<SpeakingEvaluationPage*>(this));
    QPushButton* const save = prompt.addButton(
        pageText("Save Changes"), QMessageBox::AcceptRole);
    QPushButton* const discard = prompt.addButton(
        pageText("Discard"), QMessageBox::DestructiveRole);
    QPushButton* const cancel = prompt.addButton(
        pageText("Cancel"), QMessageBox::RejectRole);
    prompt.setDefaultButton(save);
    prompt.exec();
    if (prompt.clickedButton() == save)
    {
        return SpeakingEvaluationUnsavedChoice::Save;
    }
    if (prompt.clickedButton() == discard)
    {
        return SpeakingEvaluationUnsavedChoice::Discard;
    }
    static_cast<void>(cancel);
    return SpeakingEvaluationUnsavedChoice::Cancel;
}

bool SpeakingEvaluationPage::askQuestionableKoreanNameLengths() const
{
    if (m_actions.confirmQuestionableKoreanNameLengths)
    {
        try
        {
            return m_actions.confirmQuestionableKoreanNameLengths();
        }
        catch (...)
        {
            return false;
        }
    }
    return QMessageBox::question(
        const_cast<SpeakingEvaluationPage*>(this),
        pageText("Questionable Korean Name Lengths"),
        pageText("Some Korean names are only one or five or more syllables. Save anyway?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) == QMessageBox::Yes;
}

} // namespace ClassMngr::Next::Ui
