#include "next/ui/testing_classes_manager_page.h"

#include "domain/models/testing_class.h"

#include <QAbstractItemView>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int kAutosaveDelayMs = 750;
constexpr int kTeacherFallbackRole = Qt::UserRole + 38;

[[nodiscard]] QString managerText(const char* source)
{
    return QCoreApplication::translate("TestingClassesManagerPage", source);
}

[[nodiscard]] QString operationMessage(const Domain::OperationError& error)
{
    return QString::fromUtf8(
        error.message.data(), static_cast<qsizetype>(error.message.size()));
}

[[nodiscard]] QString weekdayName(const Domain::Weekday weekday)
{
    switch (weekday)
    {
    case Domain::Weekday::Monday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Monday");
    case Domain::Weekday::Tuesday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Tuesday");
    case Domain::Weekday::Wednesday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Wednesday");
    case Domain::Weekday::Thursday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Thursday");
    case Domain::Weekday::Friday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Friday");
    case Domain::Weekday::Saturday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Saturday");
    case Domain::Weekday::Sunday:
        return QCoreApplication::translate("TestingClassesManagerPage", "Sunday");
    }
    return {};
}

[[nodiscard]] QString startTimeText(const int minute)
{
    return QStringLiteral("%1:%2")
        .arg(minute / 60, 2, 10, QLatin1Char('0'))
        .arg(minute % 60, 2, 10, QLatin1Char('0'));
}

[[nodiscard]] std::optional<qint64> teacherIdFromChoice(
    const QVariant& value
    )
{
    if (!value.isValid() || value.toString().isEmpty())
    {
        return std::nullopt;
    }
    bool valid = false;
    const qint64 id = value.toString().toLongLong(&valid);
    if (!valid || id <= 0 || QString::number(id) != value.toString())
    {
        return std::nullopt;
    }
    return id;
}

} // namespace

TestingClassesManagerPage::TestingClassesManagerPage(
    const PageContext& context,
    TestingClassesManagerPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_model(new TestingClassesModel(this))
    , m_autosaveTimer(new QTimer(this))
    , m_saveMode(m_actions.saveMode)
{
    setObjectName(QStringLiteral("testingClassesManagerPage"));
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(kAutosaveDelayMs);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this] {
        static_cast<void>(saveChanges());
    });
    buildUi();
}

TestingClassesModel* TestingClassesManagerPage::model() const noexcept
{
    return m_model;
}

RosterEditorWidget* TestingClassesManagerPage::rosterEditor() const noexcept
{
    return m_rosterEditor;
}

QListView* TestingClassesManagerPage::classList() const noexcept
{
    return m_classList;
}

QTabWidget* TestingClassesManagerPage::sections() const noexcept
{
    return m_sections;
}

QWidget* TestingClassesManagerPage::editorWidget() const noexcept
{
    return m_editorWidget;
}

QLineEdit* TestingClassesManagerPage::nameEdit() const noexcept
{
    return m_nameEdit;
}

QComboBox* TestingClassesManagerPage::gradeChoice() const noexcept
{
    return m_gradeChoice;
}

QComboBox* TestingClassesManagerPage::levelChoice() const noexcept
{
    return m_levelChoice;
}

QLineEdit* TestingClassesManagerPage::roomEdit() const noexcept
{
    return m_roomEdit;
}

QComboBox* TestingClassesManagerPage::teacherChoice() const noexcept
{
    return m_teacherChoice;
}

QLineEdit* TestingClassesManagerPage::classColorEdit() const noexcept
{
    return m_classColorEdit;
}

QLineEdit* TestingClassesManagerPage::fontColorEdit() const noexcept
{
    return m_fontColorEdit;
}

QPlainTextEdit* TestingClassesManagerPage::notesEdit() const noexcept
{
    return m_notesEdit;
}

QPushButton* TestingClassesManagerPage::addButton() const noexcept
{
    return m_addButton;
}

QPushButton* TestingClassesManagerPage::deleteButton() const noexcept
{
    return m_deleteButton;
}

QPushButton* TestingClassesManagerPage::saveButton() const noexcept
{
    return m_saveButton;
}

QPushButton* TestingClassesManagerPage::retryButton() const noexcept
{
    return m_retryButton;
}

QLabel* TestingClassesManagerPage::statusLabel() const noexcept
{
    return m_statusLabel;
}

QLabel* TestingClassesManagerPage::pendingSlotLabel() const noexcept
{
    return m_pendingSlotLabel;
}

const std::optional<Domain::ClassId>&
TestingClassesManagerPage::selectedClassId() const noexcept
{
    return m_selectedClassId;
}

bool TestingClassesManagerPage::isNewDraft() const noexcept
{
    return m_isNewDraft;
}

bool TestingClassesManagerPage::hasLoadedDetails() const noexcept
{
    return m_savedDetails.has_value();
}

PageOperationResult<void> TestingClassesManagerPage::onActivating()
{
    m_pageActive = true;
    const QString identity = m_actions.workspaceIdentity
        ? m_actions.workspaceIdentity()
        : activePath().value_or(QString{});
    if (m_workspaceIdentity && *m_workspaceIdentity != identity)
    {
        clearForWorkspaceChange();
    }
    m_workspaceIdentity = identity;
    static_cast<void>(refreshFromStore());
    if (m_requestedNewDraft)
    {
        const auto slot = m_requestedPendingSlot;
        m_requestedNewDraft = false;
        m_requestedPendingSlot.reset();
        beginNewClass(slot);
    }
    return {};
}

void TestingClassesManagerPage::onSuspending() noexcept
{
    m_pageActive = false;
    m_autosaveTimer->stop();
    m_dirty = false;
    setHasUnsavedChanges(false);
    m_savedDetails.reset();
    m_pendingSlot.reset();
    m_isNewDraft = false;
    m_previousClassIdForDraft.reset();
    if (m_rosterEditor)
    {
        m_rosterEditor->clear();
    }
    m_loading = true;
    m_nameEdit->clear();
    m_gradeChoice->setCurrentIndex(-1);
    m_levelChoice->clear();
    m_roomEdit->clear();
    m_teacherChoice->setCurrentIndex(0);
    m_classColorEdit->clear();
    m_fontColorEdit->clear();
    m_notesEdit->clear();
    m_pendingSlotLabel->clear();
    m_loading = false;
    updateActions();
}

void TestingClassesManagerPage::setSaveMode(const Application::SaveMode mode)
{
    m_saveMode = mode;
    m_actions.saveMode = mode;
    m_actions.rosterEditor.saveMode = mode;
    if (m_rosterEditor)
    {
        m_rosterEditor->setSaveMode(mode);
    }
    scheduleAutomaticSave();
}

bool TestingClassesManagerPage::hasUnsavedChanges() const noexcept
{
    return m_dirty || (m_rosterEditor && m_rosterEditor->hasUnsavedChanges());
}

PageOperationResult<void> TestingClassesManagerPage::saveChanges()
{
    m_autosaveTimer->stop();
    const bool rosterDirty = m_rosterEditor && m_rosterEditor->hasUnsavedChanges();
    if (!m_dirty && !rosterDirty)
    {
        return {};
    }
    const std::optional<QString> path = activePath();
    if (!path)
    {
        const QString message = managerText("No workspace is open.");
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }
    QString validationError;
    if (m_dirty && !validateEditor(&validationError))
    {
        setStatus(validationError, true);
        return std::unexpected(PageOperationError{validationError});
    }

    if (m_isNewDraft)
    {
        if (!m_actions.createClass)
        {
            const QString message = managerText("Testing class creation is unavailable.");
            setStatus(message, true);
            return std::unexpected(PageOperationError{message});
        }
        const Application::TestingClassCreateResult created =
            m_actions.createClass(*path, createRequest());
        if (!created)
        {
            const QString message = operationMessage(created.error());
            setStatus(message, true);
            return std::unexpected(PageOperationError{message});
        }
        m_selectedClassId = created.value();
        m_isNewDraft = false;
        m_pendingSlot.reset();
        m_previousClassIdForDraft.reset();
        m_dirty = false;
        setHasUnsavedChanges(false);
        selectRow(m_selectedClassId);
        if (!refreshFromStore())
        {
            const QString message = managerText(
                "The testing class was saved, but its details could not be reloaded.");
            setStatus(message, true);
        }
        else
        {
            setStatus(managerText("Testing class saved."), false);
        }
        updateActions();
        return {};
    }

    if (!m_selectedClassId)
    {
        const QString message = managerText("Testing class details cannot be saved.");
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }
    if (m_dirty)
    {
        if (!m_actions.updateDetails)
        {
            const QString message = managerText("Testing class details cannot be saved.");
            setStatus(message, true);
            return std::unexpected(PageOperationError{message});
        }
        const Application::TestingClassDetailsUpdateResult saved =
            m_actions.updateDetails(*path, detailsRequest());
        if (!saved)
        {
            const QString message = operationMessage(saved.error());
            setStatus(message, true);
            return std::unexpected(PageOperationError{message});
        }
        m_dirty = false;
        setHasUnsavedChanges(hasUnsavedChanges());
    }

    if (rosterDirty && m_rosterEditor && !m_rosterEditor->saveChanges(true))
    {
        const QString message = managerText("The testing class roster could not be saved.");
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }

    if (m_dirty == false && !refreshFromStore())
    {
        setStatus(managerText("Testing class saved, but the list could not be refreshed."), true);
    }
    else
    {
        setHasUnsavedChanges(hasUnsavedChanges());
        setStatus(managerText("Testing class saved."), false);
    }
    updateActions();
    return {};
}

PageOperationResult<void> TestingClassesManagerPage::discardChanges()
{
    m_autosaveTimer->stop();
    if (m_rosterEditor && m_rosterEditor->hasUnsavedChanges()
        && !m_rosterEditor->discardChanges())
    {
        const QString message = managerText("Testing class roster changes could not be reloaded.");
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }
    m_dirty = false;
    setHasUnsavedChanges(false);
    if (m_isNewDraft)
    {
        const auto previous = m_previousClassIdForDraft;
        m_previousClassIdForDraft.reset();
        if (previous && loadClass(*previous))
        {
            return {};
        }
        beginNewClass();
        return {};
    }
    if (m_selectedClassId && loadClass(*m_selectedClassId))
    {
        return {};
    }
    beginNewClass();
    return {};
}

bool TestingClassesManagerPage::refreshFromStore()
{
    m_autosaveTimer->stop();
    const std::optional<QString> path = activePath();
    if (!path)
    {
        clearForClosedWorkspace();
        setStatus(managerText("No workspace is open."), true);
        return false;
    }
    if (!m_actions.readClasses)
    {
        setStatus(managerText("The testing class list could not be loaded."), true);
        return false;
    }

    const Application::ScheduleTestingClassChoicesReadResult loaded =
        m_actions.readClasses(*path);
    if (!loaded)
    {
        setStatus(managerText("The testing class list could not be loaded. Retry this page."), true);
        return false;
    }

    const auto preferred = m_selectedClassId;
    m_refreshing = true;
    m_model->setSnapshot(loaded.value());
    m_refreshing = false;
    loadTeacherChoices();

    if (m_requestedNewDraft)
    {
        return true;
    }
    if (preferred && m_model->indexForClassId(*preferred).isValid())
    {
        if (!loadClass(*preferred))
        {
            selectRow(preferred);
            return false;
        }
        return true;
    }
    if (m_model->classCount() > 0)
    {
        const auto first = m_model->classIdAt(m_model->index(0, 0));
        if (first && loadClass(*first))
        {
            return true;
        }
        return false;
    }
    beginNewClass();
    setStatus(managerText("No testing classes are available. Add a class to begin."), false);
    return true;
}

void TestingClassesManagerPage::openNewClassForPendingSlot(
    std::optional<Application::TestingClassPendingScheduleSlot> slot
    )
{
    if (!m_pageActive)
    {
        m_requestedNewDraft = true;
        m_requestedPendingSlot = std::move(slot);
        return;
    }
    if (!resolveDraft(managerText("starting a new testing class")))
    {
        return;
    }
    beginNewClass(std::move(slot));
}

void TestingClassesManagerPage::clearForWorkspaceChange()
{
    m_autosaveTimer->stop();
    m_model->clear();
    clearEditor();
    m_workspaceIdentity.reset();
    setStatus(managerText("Workspace changed. Retry to load Testing Classes."), true);
}

void TestingClassesManagerPage::clearForClosedWorkspace()
{
    m_autosaveTimer->stop();
    m_model->clear();
    clearEditor();
    m_workspaceIdentity.reset();
    setStatus(managerText("No workspace is open."), true);
}

void TestingClassesManagerPage::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TestingClassesManagerPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 10);
    root->setSpacing(10);

    auto* header = new QHBoxLayout;
    auto* titleColumn = new QVBoxLayout;
    m_pageTitle = new QLabel(this);
    m_pageTitle->setObjectName(QStringLiteral("testingClassesTitle"));
    QFont titleFont = m_pageTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    m_pageTitle->setFont(titleFont);
    m_pageSubtitle = new QLabel(this);
    m_pageSubtitle->setObjectName(QStringLiteral("testingClassesSubtitle"));
    titleColumn->addWidget(m_pageTitle);
    titleColumn->addWidget(m_pageSubtitle);
    header->addLayout(titleColumn, 1);
    root->addLayout(header);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("testingClassesSplitter"));
    splitter->setChildrenCollapsible(false);
    auto* listPane = new QWidget(splitter);
    auto* listLayout = new QVBoxLayout(listPane);
    listLayout->setContentsMargins(0, 0, 0, 0);
    m_classList = new QListView(listPane);
    m_classList->setObjectName(QStringLiteral("testingClassesList"));
    m_classList->setModel(m_model);
    m_classList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_classList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_classList->setTextElideMode(Qt::ElideNone);
    listLayout->addWidget(m_classList, 1);
    auto* listButtons = new QHBoxLayout;
    m_addButton = new QPushButton(this);
    m_addButton->setObjectName(QStringLiteral("testingClassesAdd"));
    m_deleteButton = new QPushButton(this);
    m_deleteButton->setObjectName(QStringLiteral("testingClassesDelete"));
    listButtons->addWidget(m_addButton);
    listButtons->addWidget(m_deleteButton);
    listLayout->addLayout(listButtons);

    m_editorWidget = new QWidget(splitter);
    m_editorWidget->setObjectName(QStringLiteral("testingClassesEditor"));
    auto* editorLayout = new QVBoxLayout(m_editorWidget);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    m_pendingSlotLabel = new QLabel(m_editorWidget);
    m_pendingSlotLabel->setObjectName(QStringLiteral("testingClassesPendingSlot"));
    m_pendingSlotLabel->setWordWrap(true);
    editorLayout->addWidget(m_pendingSlotLabel);
    m_sections = new QTabWidget(m_editorWidget);
    m_sections->setObjectName(QStringLiteral("testingClassesSections"));

    auto* detailsTab = new QWidget(m_sections);
    auto* detailsLayout = new QVBoxLayout(detailsTab);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_nameEdit = new QLineEdit(detailsTab);
    m_nameEdit->setObjectName(QStringLiteral("testingClassName"));
    m_gradeChoice = new QComboBox(detailsTab);
    m_gradeChoice->setObjectName(QStringLiteral("testingClassGrade"));
    m_gradeChoice->addItems(testingClassGrades());
    m_levelChoice = new QComboBox(detailsTab);
    m_levelChoice->setObjectName(QStringLiteral("testingClassLevel"));
    m_roomEdit = new QLineEdit(detailsTab);
    m_roomEdit->setObjectName(QStringLiteral("testingClassRoom"));
    m_teacherChoice = new QComboBox(detailsTab);
    m_teacherChoice->setObjectName(QStringLiteral("testingClassTeacher"));
    m_classColorEdit = new QLineEdit(detailsTab);
    m_classColorEdit->setObjectName(QStringLiteral("testingClassColor"));
    m_fontColorEdit = new QLineEdit(detailsTab);
    m_fontColorEdit->setObjectName(QStringLiteral("testingClassFontColor"));
    m_classColorPreview = new QLabel(detailsTab);
    m_classColorPreview->setObjectName(QStringLiteral("testingClassColorPreview"));
    m_classColorPreview->setFixedSize(24, 24);
    m_fontColorPreview = new QLabel(detailsTab);
    m_fontColorPreview->setObjectName(QStringLiteral("testingClassFontColorPreview"));
    m_fontColorPreview->setFixedSize(24, 24);
    auto* classColorRow = new QWidget(detailsTab);
    auto* classColorLayout = new QHBoxLayout(classColorRow);
    classColorLayout->setContentsMargins(0, 0, 0, 0);
    auto* classColorButton = new QPushButton(detailsTab);
    classColorButton->setObjectName(QStringLiteral("testingClassChooseColor"));
    classColorLayout->addWidget(m_classColorEdit, 1);
    classColorLayout->addWidget(m_classColorPreview);
    classColorLayout->addWidget(classColorButton);
    auto* fontColorRow = new QWidget(detailsTab);
    auto* fontColorLayout = new QHBoxLayout(fontColorRow);
    fontColorLayout->setContentsMargins(0, 0, 0, 0);
    auto* fontColorButton = new QPushButton(detailsTab);
    fontColorButton->setObjectName(QStringLiteral("testingClassChooseFontColor"));
    fontColorLayout->addWidget(m_fontColorEdit, 1);
    fontColorLayout->addWidget(m_fontColorPreview);
    fontColorLayout->addWidget(fontColorButton);

    m_nameLabel = new QLabel(detailsTab);
    m_nameLabel->setObjectName(QStringLiteral("testingClassNameLabel"));
    m_gradeLabel = new QLabel(detailsTab);
    m_gradeLabel->setObjectName(QStringLiteral("testingClassGradeLabel"));
    m_levelLabel = new QLabel(detailsTab);
    m_levelLabel->setObjectName(QStringLiteral("testingClassLevelLabel"));
    m_roomLabel = new QLabel(detailsTab);
    m_roomLabel->setObjectName(QStringLiteral("testingClassRoomLabel"));
    m_teacherLabel = new QLabel(detailsTab);
    m_teacherLabel->setObjectName(QStringLiteral("testingClassTeacherLabel"));
    m_classColorLabel = new QLabel(detailsTab);
    m_classColorLabel->setObjectName(QStringLiteral("testingClassColorLabel"));
    m_fontColorLabel = new QLabel(detailsTab);
    m_fontColorLabel->setObjectName(QStringLiteral("testingClassFontColorLabel"));
    form->addRow(m_nameLabel, m_nameEdit);
    form->addRow(m_gradeLabel, m_gradeChoice);
    form->addRow(m_levelLabel, m_levelChoice);
    form->addRow(m_roomLabel, m_roomEdit);
    form->addRow(m_teacherLabel, m_teacherChoice);
    form->addRow(m_classColorLabel, classColorRow);
    form->addRow(m_fontColorLabel, fontColorRow);
    detailsLayout->addLayout(form);
    detailsLayout->addStretch(1);

    auto* notesTab = new QWidget(m_sections);
    auto* notesLayout = new QVBoxLayout(notesTab);
    m_notesEdit = new QPlainTextEdit(notesTab);
    m_notesEdit->setObjectName(QStringLiteral("testingClassNotes"));
    notesLayout->addWidget(m_notesEdit, 1);

    auto* rosterTab = new QWidget(m_sections);
    auto* rosterLayout = new QVBoxLayout(rosterTab);
    rosterLayout->setContentsMargins(0, 0, 0, 0);
    m_actions.rosterEditor.saveMode = m_saveMode;
    m_rosterEditor = new RosterEditorWidget(m_actions.rosterEditor, rosterTab);
    m_rosterEditor->setObjectName(QStringLiteral("testingClassRosterEditor"));
    rosterLayout->addWidget(m_rosterEditor);
    m_rosterEditor->setDirtyChangedCallback([this](const bool) {
        setHasUnsavedChanges(hasUnsavedChanges());
        updateActions();
    });

    m_sections->addTab(detailsTab, QString{});
    m_sections->addTab(notesTab, QString{});
    m_sections->addTab(rosterTab, QString{});
    editorLayout->addWidget(m_sections, 1);
    splitter->addWidget(listPane);
    splitter->addWidget(m_editorWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 750});
    root->addWidget(splitter, 1);

    auto* footer = new QHBoxLayout;
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("testingClassesStatus"));
    m_statusLabel->setWordWrap(true);
    footer->addWidget(m_statusLabel, 1);
    m_retryButton = new QPushButton(this);
    m_retryButton->setObjectName(QStringLiteral("testingClassesRetry"));
    footer->addWidget(m_retryButton);
    m_saveButton = new QPushButton(this);
    m_saveButton->setObjectName(QStringLiteral("testingClassesSave"));
    footer->addWidget(m_saveButton);
    root->addLayout(footer);

    connect(m_classList->selectionModel(),
        &QItemSelectionModel::currentChanged, this,
        [this](const QModelIndex& current, const QModelIndex&) {
            handleSelectionChange(current);
        });
    connect(m_sections, &QTabWidget::currentChanged, this, [this](const int index) {
        if (m_loading)
        {
            return;
        }
        const int previousIndex = m_previousSectionIndex;
        if (previousIndex == 2 && index != 2 && m_rosterEditor)
        {
            m_rosterEditor->finishEditing();
        }
        if (previousIndex == 2 && index != 2
            && !resolveRosterChangesBeforeLeaving())
        {
            const QSignalBlocker blocker(m_sections);
            m_sections->setCurrentIndex(previousIndex);
            return;
        }
        if (previousIndex == 2 && index != 2 && m_rosterEditor)
        {
            m_rosterEditor->clear();
        }
        m_previousSectionIndex = index;
        if (index == 2)
        {
            loadRosterIfNeeded();
        }
    });
    connect(m_addButton, &QPushButton::clicked, this,
        [this] { addClass(); });
    connect(m_deleteButton, &QPushButton::clicked, this,
        [this] { deleteSelectedClass(); });
    connect(m_saveButton, &QPushButton::clicked, this,
        [this] { static_cast<void>(saveChanges()); });
    connect(m_retryButton, &QPushButton::clicked, this, [this] {
        if (m_dirty || (m_rosterEditor && m_rosterEditor->hasUnsavedChanges()))
        {
            static_cast<void>(saveChanges());
        }
        else
        {
            static_cast<void>(refreshFromStore());
        }
    });
    connect(m_nameEdit, &QLineEdit::textEdited, this,
        [this] { markDirty(); });
    connect(m_gradeChoice, &QComboBox::currentTextChanged, this,
        [this](const QString&) {
            updateLevelChoices();
            markDirty();
        });
    connect(m_levelChoice, &QComboBox::currentTextChanged, this,
        [this](const QString&) { markDirty(); });
    connect(m_roomEdit, &QLineEdit::textEdited, this,
        [this] { markDirty(); });
    connect(m_teacherChoice, &QComboBox::currentIndexChanged,
        this, [this](int) { markDirty(); });
    connect(m_classColorEdit, &QLineEdit::textEdited, this, [this] {
        updateColors();
        markDirty();
    });
    connect(m_fontColorEdit, &QLineEdit::textEdited, this, [this] {
        updateColors();
        markDirty();
    });
    connect(m_notesEdit, &QPlainTextEdit::textChanged, this,
        [this] { markDirty(); });
    connect(classColorButton, &QPushButton::clicked, this, [this] {
        const QColor current(m_classColorEdit->text());
        const QColor chosen = QColorDialog::getColor(current, this,
            managerText("Choose Class Color"));
        if (chosen.isValid())
        {
            m_classColorEdit->setText(chosen.name(QColor::HexRgb).toUpper());
            updateColors();
            markDirty();
        }
    });
    connect(fontColorButton, &QPushButton::clicked, this, [this] {
        const QColor current(m_fontColorEdit->text());
        const QColor chosen = QColorDialog::getColor(current, this,
            managerText("Choose Font Color"));
        if (chosen.isValid())
        {
            m_fontColorEdit->setText(chosen.name(QColor::HexRgb).toUpper());
            updateColors();
            markDirty();
        }
    });

    retranslateUi();
    clearEditor();
    updateActions();
}

void TestingClassesManagerPage::retranslateUi()
{
    m_pageTitle->setText(managerText("Testing Classes"));
    m_pageSubtitle->setText(managerText("Manage reusable classes for the Testing schedule."));
    m_nameLabel->setText(managerText("Class Name"));
    m_gradeLabel->setText(managerText("Grade"));
    m_levelLabel->setText(managerText("Level"));
    m_roomLabel->setText(managerText("Room"));
    m_teacherLabel->setText(managerText("Korean Teacher"));
    m_classColorLabel->setText(managerText("Class Color"));
    m_fontColorLabel->setText(managerText("Font Color"));
    m_sections->setTabText(0, managerText("Details"));
    m_sections->setTabText(1, managerText("Notes"));
    m_sections->setTabText(2,
        QCoreApplication::translate("NextRosterEditor", "Roster"));
    m_addButton->setText(managerText("Add Class"));
    m_deleteButton->setText(managerText("Delete Class"));
    m_saveButton->setText(managerText("Save Changes"));
    m_retryButton->setText(managerText("Retry"));
    if (auto* const button = findChild<QPushButton*>(
            QStringLiteral("testingClassChooseColor")))
    {
        button->setText(managerText("Choose Color"));
    }
    if (auto* const button = findChild<QPushButton*>(
            QStringLiteral("testingClassChooseFontColor")))
    {
        button->setText(managerText("Choose Color"));
    }
    m_nameEdit->setPlaceholderText(managerText("Shown on Testing schedules"));
    m_roomEdit->setPlaceholderText(managerText("Room"));
    m_notesEdit->setPlaceholderText(managerText("Instructions and other notes"));
    for (int index = 0; index < m_teacherChoice->count(); ++index)
    {
        const QString teacherId = m_teacherChoice->itemData(index).toString();
        if (teacherId.isEmpty())
        {
            m_teacherChoice->setItemText(index, managerText("No Teacher"));
        }
        else if (m_teacherChoice->itemData(index, kTeacherFallbackRole).toBool())
        {
            m_teacherChoice->setItemText(
                index, managerText("Teacher %1").arg(teacherId));
        }
    }
    if (m_pendingSlot)
    {
        m_pendingSlotLabel->setText(managerText("New class for %1 at %2")
            .arg(weekdayName(m_pendingSlot->weekday),
                startTimeText(m_pendingSlot->startMinute.rawValue())));
    }
}

void TestingClassesManagerPage::clearEditor()
{
    m_loading = true;
    m_autosaveTimer->stop();
    m_selectedClassId.reset();
    m_savedDetails.reset();
    m_pendingSlot.reset();
    m_previousClassIdForDraft.reset();
    m_isNewDraft = false;
    m_dirty = false;
    setHasUnsavedChanges(false);
    if (m_rosterEditor)
    {
        m_rosterEditor->clear();
    }
    m_nameEdit->clear();
    m_gradeChoice->setCurrentIndex(-1);
    m_levelChoice->clear();
    m_roomEdit->clear();
    m_teacherChoice->clear();
    m_classColorEdit->clear();
    m_fontColorEdit->clear();
    m_notesEdit->clear();
    m_pendingSlotLabel->clear();
    m_classList->clearSelection();
    m_loading = false;
    updateActions();
}

void TestingClassesManagerPage::beginNewClass(
    std::optional<Application::TestingClassPendingScheduleSlot> slot
    )
{
    m_autosaveTimer->stop();
    m_previousClassIdForDraft = m_selectedClassId;
    m_selectedClassId.reset();
    m_savedDetails.reset();
    m_isNewDraft = true;
    m_pendingSlot = std::move(slot);
    m_dirty = false;
    setHasUnsavedChanges(false);
    if (m_rosterEditor)
    {
        m_rosterEditor->clear();
    }
    m_loading = true;
    m_classList->clearSelection();
    m_nameEdit->setText(QStringLiteral("Testing Class"));
    m_gradeChoice->setCurrentText(QStringLiteral("M1"));
    updateLevelChoices(QStringLiteral("Mixed (All)"));
    m_roomEdit->clear();
    m_teacherChoice->setCurrentIndex(0);
    m_classColorEdit->setText(QStringLiteral("#FFFFFF"));
    m_fontColorEdit->setText(QStringLiteral("#000000"));
    m_notesEdit->clear();
    m_loading = false;
    if (m_pendingSlot)
    {
        m_pendingSlotLabel->setText(managerText("New class for %1 at %2")
            .arg(weekdayName(m_pendingSlot->weekday),
                startTimeText(m_pendingSlot->startMinute.rawValue())));
    }
    else
    {
        m_pendingSlotLabel->clear();
    }
    setStatus({}, false);
    updateColors();
    updateActions();
}

bool TestingClassesManagerPage::loadClass(const Domain::ClassId& classId)
{
    const auto path = activePath();
    if (!path || !m_actions.readDetails)
    {
        setStatus(managerText("Testing class details cannot be loaded."), true);
        return false;
    }
    const Application::TestingClassDetailsReadResult loaded =
        m_actions.readDetails(*path, classId);
    if (!loaded)
    {
        setStatus(operationMessage(loaded.error()), true);
        return false;
    }
    if (loaded.value().classId != classId)
    {
        setStatus(managerText("Testing class details returned a different class."), true);
        return false;
    }

    const bool selectedClassChanged = m_selectedClassId != classId;
    if (selectedClassChanged && m_rosterEditor)
    {
        m_rosterEditor->clear();
    }

    const Application::TestingClassDetailsSnapshot& details = loaded.value();
    m_loading = true;
    m_selectedClassId = classId;
    m_savedDetails = details;
    m_isNewDraft = false;
    m_pendingSlot.reset();
    m_previousClassIdForDraft.reset();
    m_nameEdit->setText(QString::fromStdU16String(details.name));
    m_gradeChoice->setCurrentText(QString::fromStdU16String(details.grade));
    updateLevelChoices(QString::fromStdU16String(details.level));
    m_roomEdit->setText(QString::fromStdU16String(details.room));
    if (details.teacherId)
    {
        const QString teacherId = QString::fromStdString(
            details.teacherId->value());
        int index = m_teacherChoice->findData(teacherId);
        if (index < 0)
        {
            m_teacherChoice->addItem(managerText("Teacher %1").arg(teacherId), teacherId);
            index = m_teacherChoice->count() - 1;
            m_teacherChoice->setItemData(index, true, kTeacherFallbackRole);
        }
        m_teacherChoice->setCurrentIndex(index);
    }
    else
    {
        m_teacherChoice->setCurrentIndex(0);
    }
    m_classColorEdit->setText(QString::fromStdU16String(details.classColor));
    m_fontColorEdit->setText(QString::fromStdU16String(details.fontColor));
    m_notesEdit->setPlainText(QString::fromStdU16String(details.notes));
    m_loading = false;
    m_dirty = false;
    setHasUnsavedChanges(false);
    m_pendingSlotLabel->clear();
    selectRow(classId);
    updateColors();
    setStatus(m_teacherChoicesUnavailable
        ? managerText("Teacher choices could not be loaded. Retry to update choices.")
        : QString{}, m_teacherChoicesUnavailable);
    updateActions();
    if (m_sections->currentIndex() == 2)
    {
        loadRosterIfNeeded();
    }
    return true;
}

void TestingClassesManagerPage::loadRosterIfNeeded()
{
    const auto path = activePath();
    if (path && m_selectedClassId && !m_isNewDraft && m_rosterEditor)
    {
        m_rosterEditor->enterClassSection(*path, *m_selectedClassId, true);
    }
}

void TestingClassesManagerPage::selectRow(
    const std::optional<Domain::ClassId>& classId
    )
{
    const QSignalBlocker blocker(m_classList->selectionModel());
    if (!classId)
    {
        m_classList->clearSelection();
        m_classList->setCurrentIndex({});
        return;
    }
    const QModelIndex index = m_model->indexForClassId(*classId);
    if (index.isValid())
    {
        m_classList->setCurrentIndex(index);
        m_classList->selectionModel()->select(index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

void TestingClassesManagerPage::handleSelectionChange(
    const QModelIndex& current
    )
{
    if (m_refreshing || m_loading || !current.isValid())
    {
        return;
    }
    const auto requested = m_model->classIdAt(current);
    if (!requested || (m_selectedClassId == requested && !m_isNewDraft))
    {
        return;
    }
    const auto prior = m_selectedClassId;
    if (!resolveDraft(managerText("changing the selected testing class")))
    {
        selectRow(prior);
        return;
    }
    if (!loadClass(*requested))
    {
        selectRow(prior);
    }
}

bool TestingClassesManagerPage::resolveDraft(const QString& reason)
{
    if (!m_dirty && !(m_rosterEditor && m_rosterEditor->hasUnsavedChanges()))
    {
        return true;
    }
    switch (m_actions.resolveUnsavedChanges
        ? m_actions.resolveUnsavedChanges(reason)
        : askUnsavedChanges(reason))
    {
    case TestingClassesUnsavedChoice::Save:
        return static_cast<bool>(saveChanges());
    case TestingClassesUnsavedChoice::Discard:
        return static_cast<bool>(discardChanges());
    case TestingClassesUnsavedChoice::Cancel:
        return false;
    }
    return false;
}

bool TestingClassesManagerPage::resolveRosterChangesBeforeLeaving()
{
    if (!m_rosterEditor || !m_rosterEditor->hasUnsavedChanges())
    {
        return true;
    }

    const QString reason = managerText("switching testing class sections");
    const TestingClassesUnsavedChoice choice = m_actions.resolveUnsavedChanges
        ? m_actions.resolveUnsavedChanges(reason)
        : askUnsavedChanges(reason);
    switch (choice)
    {
    case TestingClassesUnsavedChoice::Save:
        if (m_rosterEditor->saveChanges(true))
        {
            return true;
        }
        setStatus(managerText("The testing class roster could not be saved."), true);
        return false;
    case TestingClassesUnsavedChoice::Discard:
        if (m_rosterEditor->discardChanges())
        {
            return true;
        }
        setStatus(managerText(
            "Testing class roster changes could not be reloaded."), true);
        return false;
    case TestingClassesUnsavedChoice::Cancel:
        return false;
    }
    return false;
}

void TestingClassesManagerPage::markDirty()
{
    if (m_loading || (!m_isNewDraft && !m_selectedClassId))
    {
        return;
    }
    m_dirty = true;
    setHasUnsavedChanges(true);
    setStatus({}, false);
    updateActions();
    scheduleAutomaticSave();
}

void TestingClassesManagerPage::scheduleAutomaticSave()
{
    m_autosaveTimer->stop();
    if (m_saveMode == Application::SaveMode::Automatic
        && m_selectedClassId && !m_isNewDraft && m_dirty)
    {
        m_autosaveTimer->start();
    }
}

void TestingClassesManagerPage::updateLevelChoices(const QString& selected)
{
    const QString preserve = selected.isEmpty()
        ? m_levelChoice->currentText() : selected;
    const QSignalBlocker blocker(m_levelChoice);
    m_levelChoice->clear();
    m_levelChoice->addItems(testingClassLevelsForGrade(
        m_gradeChoice->currentText()));
    const int index = m_levelChoice->findText(preserve);
    if (index >= 0)
    {
        m_levelChoice->setCurrentIndex(index);
    }
    else if (m_levelChoice->count() > 0)
    {
        m_levelChoice->setCurrentIndex(0);
    }
}

void TestingClassesManagerPage::updateColors()
{
    const QColor classColor(m_classColorEdit->text());
    const QColor fontColor(m_fontColorEdit->text());
    m_classColorPreview->setStyleSheet(classColor.isValid()
        ? QStringLiteral("background:%1;border:1px solid #888;")
            .arg(classColor.name())
        : QStringLiteral("background:transparent;border:1px dashed #c00;"));
    m_fontColorPreview->setStyleSheet(fontColor.isValid()
        ? QStringLiteral("background:%1;border:1px solid #888;")
            .arg(fontColor.name())
        : QStringLiteral("background:transparent;border:1px dashed #c00;"));
}

void TestingClassesManagerPage::loadTeacherChoices()
{
    m_teacherChoicesUnavailable = false;
    m_loading = true;
    m_teacherChoice->clear();
    m_teacherChoice->addItem(managerText("No Teacher"), QString{});
    const auto path = activePath();
    if (!path || !m_actions.readTeacherChoices)
    {
        m_teacherChoicesUnavailable = true;
        m_loading = false;
        return;
    }
    const Application::TestingTeacherChoicesReadResult loaded =
        m_actions.readTeacherChoices(*path);
    if (!loaded)
    {
        m_teacherChoicesUnavailable = true;
        m_loading = false;
        return;
    }
    for (const Application::TestingTeacherChoice& teacher : loaded.value().choices)
    {
        const QString id = QString::fromStdString(teacher.teacherId.value());
        QString name = QString::fromStdU16String(teacher.name).trimmed();
        if (name.isEmpty())
        {
            name = managerText("Teacher %1").arg(id);
        }
        m_teacherChoice->addItem(name, id);
        if (QString::fromStdU16String(teacher.name).trimmed().isEmpty())
        {
            m_teacherChoice->setItemData(
                m_teacherChoice->count() - 1, true, kTeacherFallbackRole);
        }
    }
    m_loading = false;
}

void TestingClassesManagerPage::setStatus(
    const QString& message,
    const bool isError
    )
{
    m_lastError = isError ? message : QString{};
    m_statusLabel->setText(message);
    m_statusLabel->setProperty("error", isError);
    m_retryButton->setVisible(isError);
}

void TestingClassesManagerPage::updateActions()
{
    const bool hasWorkspace = activePath().has_value();
    QString ignored;
    const bool valid = validateEditor(&ignored);
    m_saveButton->setEnabled(hasWorkspace && (m_dirty
        || (m_rosterEditor && m_rosterEditor->hasUnsavedChanges())) && valid);
    m_addButton->setEnabled(hasWorkspace);
    m_deleteButton->setEnabled(hasWorkspace && m_selectedClassId.has_value());
    m_classList->setEnabled(hasWorkspace && m_model->classCount() > 0);
    m_editorWidget->setEnabled(hasWorkspace);
    if (m_sections)
    {
        m_sections->setTabEnabled(2, hasWorkspace
            && m_selectedClassId.has_value() && !m_isNewDraft);
    }
}

std::optional<QString> TestingClassesManagerPage::activePath() const
{
    if (!m_actions.activeWorkspacePath)
    {
        return std::nullopt;
    }
    const auto path = m_actions.activeWorkspacePath();
    return path && !path->trimmed().isEmpty() ? path : std::nullopt;
}

Application::TestingClassDetailsUpdateRequest
TestingClassesManagerPage::detailsRequest() const
{
    const auto teacher = teacherIdFromChoice(m_teacherChoice->currentData());
    std::optional<Domain::TeacherId> teacherId;
    if (teacher)
    {
        teacherId = Domain::TeacherId::fromString(std::to_string(*teacher));
    }
    return {
        .classId = *m_selectedClassId,
        .name = m_nameEdit->text().trimmed().toStdU16String(),
        .grade = m_gradeChoice->currentText().trimmed().toStdU16String(),
        .level = m_levelChoice->currentText().trimmed().toStdU16String(),
        .room = m_roomEdit->text().trimmed().toStdU16String(),
        .teacherId = teacherId,
        .classColor = QColor(m_classColorEdit->text())
            .name(QColor::HexRgb).toUpper().toStdU16String(),
        .fontColor = QColor(m_fontColorEdit->text())
            .name(QColor::HexRgb).toUpper().toStdU16String(),
        .notes = m_notesEdit->toPlainText().toStdU16String()
    };
}

Application::TestingClassCreateRequest
TestingClassesManagerPage::createRequest() const
{
    const auto teacher = teacherIdFromChoice(m_teacherChoice->currentData());
    std::optional<Domain::TeacherId> teacherId;
    if (teacher)
    {
        teacherId = Domain::TeacherId::fromString(std::to_string(*teacher));
    }
    return {
        .name = m_nameEdit->text().trimmed().toStdU16String(),
        .grade = m_gradeChoice->currentText().trimmed().toStdU16String(),
        .level = m_levelChoice->currentText().trimmed().toStdU16String(),
        .room = m_roomEdit->text().trimmed().toStdU16String(),
        .teacherId = teacherId,
        .classColor = QColor(m_classColorEdit->text())
            .name(QColor::HexRgb).toUpper().toStdU16String(),
        .fontColor = QColor(m_fontColorEdit->text())
            .name(QColor::HexRgb).toUpper().toStdU16String(),
        .notes = m_notesEdit->toPlainText().toStdU16String(),
        .pendingScheduleSlot = m_pendingSlot
    };
}

bool TestingClassesManagerPage::validateEditor(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error)
        {
            *error = message;
        }
        return false;
    };
    if (m_nameEdit->text().trimmed().isEmpty())
    {
        return fail(managerText("Class name is required."));
    }
    if (m_gradeChoice->currentText().trimmed().isEmpty()
        || m_levelChoice->currentText().trimmed().isEmpty())
    {
        return fail(managerText("Choose a grade and level."));
    }
    if (m_roomEdit->text().trimmed().isEmpty())
    {
        return fail(managerText("Room is required."));
    }
    if (!QColor(m_classColorEdit->text()).isValid()
        || !QColor(m_fontColorEdit->text()).isValid())
    {
        return fail(managerText("Enter valid class and font colors."));
    }
    if (error)
    {
        error->clear();
    }
    return true;
}

TestingClassesUnsavedChoice TestingClassesManagerPage::askUnsavedChanges(
    const QString& reason
    ) const
{
    QMessageBox prompt(QMessageBox::Warning, managerText("Unsaved Changes"),
        managerText("Save testing class changes before %1?").arg(reason),
        QMessageBox::NoButton, const_cast<TestingClassesManagerPage*>(this));
    QPushButton* const save = prompt.addButton(managerText("Save"), QMessageBox::AcceptRole);
    QPushButton* const discard = prompt.addButton(managerText("Discard"), QMessageBox::DestructiveRole);
    prompt.addButton(managerText("Cancel"), QMessageBox::RejectRole);
    prompt.setDefaultButton(save);
    prompt.exec();
    if (prompt.clickedButton() == save)
    {
        return TestingClassesUnsavedChoice::Save;
    }
    if (prompt.clickedButton() == discard)
    {
        return TestingClassesUnsavedChoice::Discard;
    }
    return TestingClassesUnsavedChoice::Cancel;
}

bool TestingClassesManagerPage::askDeleteConfirmation(
    const QString& className
    ) const
{
    const QString message = managerText(
        "Delete %1? This also removes its roster, notes, speaking evaluations, "
        "regular and intensive class times, and Testing schedule assignments.")
        .arg(className);
    if (m_actions.confirmDelete)
    {
        return m_actions.confirmDelete(message);
    }
    return QMessageBox::warning(
        const_cast<TestingClassesManagerPage*>(this),
        managerText("Delete Testing Class?"), message,
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        == QMessageBox::Yes;
}

void TestingClassesManagerPage::addClass()
{
    if (!resolveDraft(managerText("adding a testing class")))
    {
        return;
    }
    beginNewClass();
    m_nameEdit->setFocus();
}

void TestingClassesManagerPage::deleteSelectedClass()
{
    if (!m_selectedClassId
        || !resolveDraft(managerText("deleting this testing class")))
    {
        return;
    }
    const QString className = m_nameEdit->text().trimmed();
    if (!askDeleteConfirmation(className))
    {
        return;
    }
    const auto path = activePath();
    if (!path || !m_actions.deleteClass)
    {
        setStatus(managerText("Testing class deletion is unavailable."), true);
        return;
    }
    const Application::TestingClassDeleteResult deleted = m_actions.deleteClass(
        *path, {.classId = *m_selectedClassId});
    if (!deleted)
    {
        setStatus(operationMessage(deleted.error()), true);
        return;
    }
    m_selectedClassId.reset();
    m_savedDetails.reset();
    m_dirty = false;
    setHasUnsavedChanges(false);
    if (!refreshFromStore())
    {
        setStatus(managerText("Testing class deleted, but the list could not be refreshed."), true);
    }
    else if (m_model->classCount() == 0)
    {
        beginNewClass();
        setStatus(managerText("No testing classes are available. Add a class to begin."), false);
    }
    else
    {
        setStatus(managerText("Testing class deleted."), false);
    }
}

} // namespace ClassMngr::Next::Ui
