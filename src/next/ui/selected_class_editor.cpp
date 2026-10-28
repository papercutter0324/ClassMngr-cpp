#include "next/ui/selected_class_editor.h"

#include "domain/config/class_details_catalog.h"
#include "next/application/classes_list_read_snapshot.h"
#include "next/application/class_details_validation_policy.h"
#include "next/ui/selected_class_analytics_model.h"

#include <QColor>
#include <QColorDialog>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QTabWidget>
#include <QTimer>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <variant>

namespace ClassMngr::Next::Ui
{
namespace
{

QString translated(const char* source)
{
    return QCoreApplication::translate("NextClassesPage", source);
}

QString text(const std::string& source)
{
    return QString::fromUtf8(source.data(), static_cast<qsizetype>(source.size()));
}

QString text(const std::u16string& source)
{
    return QString::fromStdU16String(source);
}

QString errorText(const Domain::OperationError& error)
{
    return text(error.message);
}

QString teacherName(
    const Application::ClassCoTeacherTeacherChoice& teacher,
    const bool korean
    )
{
    const std::u16string& selected = korean ? teacher.teacherKr : teacher.teacherEn;
    const std::u16string& fallback = korean ? teacher.teacherEn : teacher.teacherKr;
    return !selected.empty() ? text(selected) : text(fallback);
}

QString workflowError(const Application::ClassDetailsSaveWorkflowResult& result)
{
    if (const auto* const validation =
            std::get_if<Application::ClassDetailsValidationOutput>(&result))
    {
        return translated("Class details have %1 validation error(s).")
            .arg(static_cast<qulonglong>(validation->issues.size()));
    }
    if (const auto* const conflict =
            std::get_if<Application::ClassDetailsSaveWorkflowConflict>(&result))
    {
        const QString conflictingClass = conflict->conflicts.empty()
            ? QString{}
            : text(conflict->conflicts.front().conflictingClassName);
        return translated("The class schedule conflicts with %1.")
            .arg(conflictingClass);
    }
    if (const auto* const failure =
            std::get_if<Application::ClassDetailsSaveWorkflowFailure>(&result))
    {
        return errorText(failure->error);
    }
    return {};
}

QWidget* createScheduleEditor(
    const QString& title,
    const QString& prefix,
    QTableWidget** table,
    QPushButton** addButton,
    const std::function<void(QTableWidget*)>& addRow,
    const std::function<void(QTableWidget*)>& removeRow,
    QWidget* parent
    )
{
    auto* const group = new QGroupBox(title, parent);
    group->setObjectName(prefix + QStringLiteral("Group"));
    auto* const layout = new QVBoxLayout(group);
    auto* const editor = new QTableWidget(0, 3, group);
    editor->setObjectName(prefix + QStringLiteral("Table"));
    editor->setHorizontalHeaderLabels({
        translated("Day"), translated("Start"), translated("End")
    });
    editor->horizontalHeader()->setStretchLastSection(true);
    editor->setSelectionBehavior(QAbstractItemView::SelectRows);
    editor->setSelectionMode(QAbstractItemView::SingleSelection);
    editor->setAlternatingRowColors(true);
    editor->setMinimumHeight(84);
    layout->addWidget(editor);
    auto* const actions = new QHBoxLayout();
    auto* const add = new QPushButton(translated("Add Time"), group);
    add->setObjectName(prefix + QStringLiteral("Add"));
    auto* const remove = new QPushButton(translated("Remove Time"), group);
    remove->setObjectName(prefix + QStringLiteral("Remove"));
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch(1);
    layout->addLayout(actions);
    QObject::connect(add, &QPushButton::clicked, group,
        [editor, addRow] { addRow(editor); });
    QObject::connect(remove, &QPushButton::clicked, group,
        [editor, removeRow] { removeRow(editor); });
    *table = editor;
    *addButton = add;
    return group;
}

} // namespace

SelectedClassEditor::SelectedClassEditor(
    SelectedClassEditorActions actions,
    QWidget* parent
    )
    : QWidget(parent)
    , m_actions(std::move(actions))
    , m_saveMode(m_actions.saveMode)
    , m_detailsAutosave(new QTimer(this))
    , m_notesAutosave(new QTimer(this))
{
    m_actions.rosterEditor.saveMode = m_saveMode;
    setObjectName(QStringLiteral("selectedClassEditor"));
    auto* const layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 8, 8);
    m_emptyState = new QLabel(translated("Select a class to edit its details."), this);
    m_emptyState->setObjectName(QStringLiteral("selectedClassEditorEmptyState"));
    m_emptyState->setWordWrap(true);
    m_emptyState->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyState, 1);
    m_detailsAutosave->setSingleShot(true);
    m_detailsAutosave->setInterval(750);
    connect(m_detailsAutosave, &QTimer::timeout, this, [this] {
        if (m_detailsDirty && m_saveMode == Application::SaveMode::Automatic)
        {
            QString ignored;
            static_cast<void>(saveDetails(&ignored));
        }
    });
    m_notesAutosave->setSingleShot(true);
    m_notesAutosave->setInterval(750);
    connect(m_notesAutosave, &QTimer::timeout, this, [this] {
        if (m_notesDirty && m_saveMode == Application::SaveMode::Automatic)
        {
            QString ignored;
            static_cast<void>(saveNotes(&ignored));
        }
    });
}

bool SelectedClassEditor::hasUnsavedChanges() const noexcept
{
    return m_detailsDirty || m_coTeacherDirty || m_notesDirty
        || (m_rosterEditor && m_rosterEditor->hasUnsavedChanges());
}

bool SelectedClassEditor::isLoaded() const noexcept
{
    return m_classId.has_value() && m_detailsLoaded;
}

std::optional<Domain::ClassId> SelectedClassEditor::classId() const
{
    return m_classId;
}

QTabWidget* SelectedClassEditor::sections() const noexcept
{
    return m_sections;
}

QComboBox* SelectedClassEditor::gradeChoice() const noexcept
{
    return m_grade;
}

QComboBox* SelectedClassEditor::levelChoice() const noexcept
{
    return m_level;
}

QComboBox* SelectedClassEditor::coTeacherChoice() const noexcept
{
    return m_coTeacher;
}

QTableWidget* SelectedClassEditor::regularSchedule() const noexcept
{
    return m_regularSchedule;
}

QTableWidget* SelectedClassEditor::intensiveSchedule() const noexcept
{
    return m_intensiveSchedule;
}

QComboBox* SelectedClassEditor::analyticsEvaluationChoice() const noexcept
{
    return m_analyticsEvaluation;
}

QTableView* SelectedClassEditor::analyticsRankingTable() const noexcept
{
    return m_analyticsRankingTable;
}

QLabel* SelectedClassEditor::analyticsStatusLabel() const noexcept
{
    return m_analyticsStatus;
}

RosterEditorWidget* SelectedClassEditor::rosterEditor() const noexcept
{
    return m_rosterEditor;
}

void SelectedClassEditor::ensureUi()
{
    if (m_sections)
    {
        return;
    }
    auto* const layout = qobject_cast<QVBoxLayout*>(this->layout());
    m_emptyState->hide();
    m_sections = new QTabWidget(this);
    m_sections->setObjectName(QStringLiteral("selectedClassEditorSections"));
    m_detailsTab = buildDetailsTab();
    m_coTeacherTab = buildCoTeacherTab();
    m_notesTab = buildNotesTab();
    m_analyticsTab = new QWidget(this);
    m_analyticsTab->setObjectName(QStringLiteral("selectedClassAnalyticsTab"));
    m_analyticsTab->setLayout(new QVBoxLayout(m_analyticsTab));
    m_rosterTab = new QWidget(this);
    m_rosterTab->setObjectName(QStringLiteral("selectedClassRosterTab"));
    auto* const rosterLayout = new QVBoxLayout(m_rosterTab);
    rosterLayout->setContentsMargins(0, 0, 0, 0);
    m_rosterEditor = new RosterEditorWidget(m_actions.rosterEditor, m_rosterTab);
    rosterLayout->addWidget(m_rosterEditor);
    m_rosterEditor->setDirtyChangedCallback([this](const bool) {
        updateDirtyState();
    });
    m_sections->addTab(m_detailsTab, translated("Class Details and Times"));
    m_sections->addTab(m_coTeacherTab, translated("Co-Teacher"));
    m_sections->addTab(m_notesTab, translated("Notes"));
    m_sections->addTab(m_analyticsTab, translated("Analytics"));
    m_sections->addTab(m_rosterTab,
        QCoreApplication::translate("NextRosterEditor", "Roster"));
    layout->addWidget(m_sections, 1);
    connect(m_sections, &QTabWidget::currentChanged, this, [this](const int index) {
        if (m_loading)
        {
            return;
        }
        const int previousIndex = m_previousSectionIndex;
        if (previousIndex == 4 && index != 4 && m_rosterEditor)
        {
            m_rosterEditor->finishEditing();
        }
        if (hasUnsavedChanges())
        {
            if (!resolveUnsavedChanges(translated("switching class sections")))
            {
                const QSignalBlocker blocker(m_sections);
                m_sections->setCurrentIndex(m_previousSectionIndex);
                return;
            }
        }
        if (previousIndex == 4 && index != 4 && m_rosterEditor)
        {
            m_rosterEditor->clear();
        }
        m_previousSectionIndex = index;
        if (previousIndex == 3 && index != 3)
        {
            clearAnalyticsDisplay();
            m_analyticsReadAttempted = false;
        }
        if (index == 1)
        {
            static_cast<void>(loadCoTeacherIfNeeded());
        }
        else if (index == 2)
        {
            static_cast<void>(loadNotesIfNeeded());
        }
        else if (index == 3)
        {
            ensureAnalyticsUi();
            static_cast<void>(loadAnalyticsIfNeeded());
        }
        else if (index == 4)
        {
            loadRosterIfNeeded();
        }
    });
    retranslateUi();
}

void SelectedClassEditor::ensureAnalyticsUi()
{
    if (m_analyticsUiBuilt)
    {
        return;
    }
    m_analyticsUiBuilt = true;
    auto* const scroll = buildAnalyticsTab();
    qobject_cast<QVBoxLayout*>(m_analyticsTab->layout())->addWidget(scroll);
    retranslateUi();
}

QWidget* SelectedClassEditor::buildDetailsTab()
{
    auto* const scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* const body = new QWidget(scroll);
    auto* const bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(4, 8, 4, 8);
    bodyLayout->setSpacing(12);

    auto* const details = new QGroupBox(translated("Class Details"), body);
    details->setObjectName(QStringLiteral("selectedClassDetailsGroup"));
    auto* const form = new QFormLayout(details);
    m_grade = new QComboBox(details);
    m_grade->setObjectName(QStringLiteral("classEditorClassGrade"));
    m_grade->setEditable(true);
    m_level = new QComboBox(details);
    m_level->setObjectName(QStringLiteral("classEditorClassLevel"));
    m_level->setEditable(true);
    m_readingBook = new QComboBox(details);
    m_readingBook->setObjectName(QStringLiteral("classEditorReadingBook"));
    m_readingBook->setEditable(true);
    m_essayBook = new QComboBox(details);
    m_essayBook->setObjectName(QStringLiteral("classEditorEssayBook"));
    m_essayBook->setEditable(true);
    m_classColor = new QLineEdit(details);
    m_classColor->setObjectName(QStringLiteral("classEditorClassColor"));
    m_fontColor = new QLineEdit(details);
    m_fontColor->setObjectName(QStringLiteral("classEditorFontColor"));
    m_chooseColor = new QPushButton(translated("Choose Color"), details);
    m_chooseColor->setObjectName(QStringLiteral("classEditorChooseColor"));
    auto* const colorRow = new QWidget(details);
    auto* const colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(m_classColor, 1);
    colorLayout->addWidget(m_chooseColor);
    form->addRow(translated("Grade"), m_grade);
    form->addRow(translated("Level"), m_level);
    form->addRow(translated("Reading Book"), m_readingBook);
    form->addRow(translated("Essay Book"), m_essayBook);
    form->addRow(translated("Class Color"), colorRow);
    form->addRow(translated("Font Color"), m_fontColor);
    m_teacherLabel = new QLabel(details);
    m_teacherLabel->setObjectName(QStringLiteral("classEditorMainTeacher"));
    m_studentCountLabel = new QLabel(details);
    m_studentCountLabel->setObjectName(QStringLiteral("classEditorStudentCount"));
    form->addRow(translated("Teacher"), m_teacherLabel);
    form->addRow(translated("# of Students"), m_studentCountLabel);
    bodyLayout->addWidget(details);

    bodyLayout->addWidget(createScheduleEditor(
        translated("Regular Schedule"), QStringLiteral("classEditorRegularSchedule"),
        &m_regularSchedule,
        &m_regularScheduleAdd,
        [this](QTableWidget* table) { appendScheduleRow(table); },
        [this](QTableWidget* table) { removeSelectedScheduleRows(table); }, body));
    bodyLayout->addWidget(createScheduleEditor(
        translated("Intensive Schedule"), QStringLiteral("classEditorIntensiveSchedule"),
        &m_intensiveSchedule,
        &m_intensiveScheduleAdd,
        [this](QTableWidget* table) { appendScheduleRow(table); },
        [this](QTableWidget* table) { removeSelectedScheduleRows(table); }, body));
    m_detailsStatus = new QLabel(body);
    m_detailsStatus->setObjectName(QStringLiteral("classEditorDetailsStatus"));
    m_detailsStatus->setProperty("role", QStringLiteral("status"));
    m_detailsStatus->setWordWrap(true);
    bodyLayout->addWidget(m_detailsStatus);
    m_retryDetailsButton = new QPushButton(translated("Retry"), body);
    m_retryDetailsButton->setObjectName(QStringLiteral("classEditorRetryDetails"));
    m_retryDetailsButton->hide();
    bodyLayout->addWidget(m_retryDetailsButton, 0, Qt::AlignRight);
    m_saveDetailsButton = new QPushButton(translated("Save Details"), body);
    m_saveDetailsButton->setObjectName(QStringLiteral("classEditorSaveDetails"));
    bodyLayout->addWidget(m_saveDetailsButton, 0, Qt::AlignRight);
    bodyLayout->addStretch(1);
    scroll->setWidget(body);

    const auto detailsChanged = [this] {
        if (m_loading)
        {
            return;
        }
        m_detailsDirty = true;
        updateDirtyState();
        scheduleDetailsSave();
    };
    connect(m_grade, &QComboBox::currentTextChanged, this,
        [this, detailsChanged](const QString&) {
            if (!m_loading)
            {
                updateGradeChoices(true);
                detailsChanged();
            }
        });
    connect(m_level, &QComboBox::currentTextChanged, this,
        [this, detailsChanged](const QString&) {
            if (!m_loading)
            {
                updateBookChoices(true);
                detailsChanged();
            }
        });
    for (QComboBox* const choice : {m_readingBook, m_essayBook})
    {
        connect(choice, &QComboBox::currentTextChanged, this, detailsChanged);
    }
    for (QLineEdit* const field : {m_classColor, m_fontColor})
    {
        connect(field, &QLineEdit::textChanged, this, detailsChanged);
    }
    for (QTableWidget* const table : {m_regularSchedule, m_intensiveSchedule})
    {
        connect(table, &QTableWidget::itemChanged, this, [detailsChanged] {
            detailsChanged();
        });
    }
    connect(m_chooseColor, &QPushButton::clicked, this, [this] {
        const QColor existing(m_classColor->text());
        const QColor selected = QColorDialog::getColor(
            existing.isValid() ? existing : QColor(QStringLiteral("#FFFFFF")), this,
            translated("Choose Class Color"));
        if (!selected.isValid())
        {
            return;
        }
        m_classColor->setText(selected.name().toUpper());
        const int luminance = 299 * selected.red() + 587 * selected.green()
            + 114 * selected.blue();
        m_fontColor->setText(luminance >= 128000
            ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF"));
    });
    connect(m_saveDetailsButton, &QPushButton::clicked, this, [this] {
        QString message;
        if (!saveDetails(&message) && !message.isEmpty())
        {
            setStatus(message, true);
        }
    });
    connect(m_retryDetailsButton, &QPushButton::clicked, this, [this] {
        readClassDetails();
        updateDirtyState();
    });
    return scroll;
}

QWidget* SelectedClassEditor::buildCoTeacherTab()
{
    auto* const body = new QWidget(this);
    auto* const layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 8, 4, 8);
    auto* const form = new QFormLayout();
    m_coTeacherLocale = new QComboBox(body);
    m_coTeacherLocale->setObjectName(QStringLiteral("classEditorCoTeacherLocale"));
    m_coTeacherLocale->addItem(translated("English"), QStringLiteral("en"));
    m_coTeacherLocale->addItem(translated("Korean"), QStringLiteral("ko"));
    m_coTeacher = new QComboBox(body);
    m_coTeacher->setObjectName(QStringLiteral("classEditorCoTeacherChoice"));
    form->addRow(translated("Language"), m_coTeacherLocale);
    form->addRow(translated("Name"), m_coTeacher);
    layout->addLayout(form);
    auto* const info = new QGroupBox(translated("Teacher Details"), body);
    auto* const infoForm = new QFormLayout(info);
    m_coTeacherNameLabel = new QLabel(info);
    m_coTeacherNameLabel->setObjectName(QStringLiteral("classEditorCoTeacherName"));
    m_coTeacherRoomLabel = new QLabel(info);
    m_coTeacherRoomLabel->setObjectName(QStringLiteral("classEditorCoTeacherRoom"));
    m_coTeacherInternetLabel = new QLabel(info);
    m_coTeacherInternetLabel->setObjectName(QStringLiteral("classEditorCoTeacherInternet"));
    m_coTeacherWifiNameLabel = new QLabel(info);
    m_coTeacherWifiNameLabel->setObjectName(QStringLiteral("classEditorCoTeacherWifiName"));
    m_coTeacherWifiPasswordLabel = new QLabel(info);
    m_coTeacherWifiPasswordLabel->setObjectName(QStringLiteral("classEditorCoTeacherWifiPassword"));
    m_coTeacherProjectionLabel = new QLabel(info);
    m_coTeacherProjectionLabel->setObjectName(QStringLiteral("classEditorCoTeacherProjection"));
    m_coTeacherZoomIdLabel = new QLabel(info);
    m_coTeacherZoomIdLabel->setObjectName(QStringLiteral("classEditorCoTeacherZoomId"));
    m_coTeacherZoomPasswordLabel = new QLabel(info);
    m_coTeacherZoomPasswordLabel->setObjectName(QStringLiteral("classEditorCoTeacherZoomPassword"));
    infoForm->addRow(translated("Name"), m_coTeacherNameLabel);
    infoForm->addRow(translated("Room"), m_coTeacherRoomLabel);
    infoForm->addRow(translated("Internet Type"), m_coTeacherInternetLabel);
    infoForm->addRow(translated("WiFi Name"), m_coTeacherWifiNameLabel);
    infoForm->addRow(translated("WiFi Password"), m_coTeacherWifiPasswordLabel);
    infoForm->addRow(translated("Projection Type"), m_coTeacherProjectionLabel);
    infoForm->addRow(translated("Zoom ID"), m_coTeacherZoomIdLabel);
    infoForm->addRow(translated("Zoom Password"), m_coTeacherZoomPasswordLabel);
    layout->addWidget(info);
    m_coTeacherStatus = new QLabel(body);
    m_coTeacherStatus->setObjectName(QStringLiteral("classEditorCoTeacherStatus"));
    m_coTeacherStatus->setProperty("role", QStringLiteral("status"));
    m_coTeacherStatus->setWordWrap(true);
    layout->addWidget(m_coTeacherStatus);
    auto* const actions = new QHBoxLayout();
    m_retryCoTeacherButton = new QPushButton(translated("Retry"), body);
    m_retryCoTeacherButton->setObjectName(QStringLiteral("classEditorRetryCoTeacher"));
    m_saveCoTeacherButton = new QPushButton(translated("Save Changes"), body);
    m_saveCoTeacherButton->setObjectName(QStringLiteral("classEditorSaveCoTeacher"));
    actions->addWidget(m_retryCoTeacherButton);
    actions->addStretch(1);
    actions->addWidget(m_saveCoTeacherButton);
    layout->addLayout(actions);
    layout->addStretch(1);
    connect(m_coTeacherLocale, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this] { updateTeacherDisplay(); });
    connect(m_coTeacher, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this] {
            if (!m_loading && !m_rebuildingChoices)
            {
                m_coTeacherDirty = true;
                updateDirtyState();
                updateCoTeacherDisplay();
            }
        });
    connect(m_retryCoTeacherButton, &QPushButton::clicked, this, [this] {
        m_coTeacherReadAttempted = false;
        m_coTeacherChoicesReadAttempted = false;
        m_selectedTeacherDetails.reset();
        m_selectedTeacherDetailsId.reset();
        static_cast<void>(loadCoTeacherIfNeeded());
    });
    connect(m_saveCoTeacherButton, &QPushButton::clicked, this, [this] {
        QString message;
        if (!saveCoTeacher(&message) && !message.isEmpty())
        {
            setCoTeacherStatus(message, true);
        }
    });
    return body;
}

QWidget* SelectedClassEditor::buildNotesTab()
{
    auto* const body = new QWidget(this);
    auto* const layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(8);

    m_notesTitleLabel = new QLabel(translated("Class Notes"), body);
    m_notesTitleLabel->setObjectName(QStringLiteral("classEditorNotesTitle"));
    QFont titleFont = m_notesTitleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_notesTitleLabel->setFont(titleFont);
    layout->addWidget(m_notesTitleLabel);
    m_notesSubtitleLabel = new QLabel(body);
    m_notesSubtitleLabel->setObjectName(QStringLiteral("classEditorNotesSubtitle"));
    m_notesSubtitleLabel->setWordWrap(true);
    layout->addWidget(m_notesSubtitleLabel);

    m_notesLabel = new QLabel(translated("Notes"), body);
    m_notesLabel->setObjectName(QStringLiteral("classEditorNotesLabel"));
    layout->addWidget(m_notesLabel);
    m_notesEdit = new QTextEdit(body);
    m_notesEdit->setObjectName(QStringLiteral("classEditorClassNotes"));
    m_notesEdit->setAcceptRichText(false);
    m_notesEdit->setMinimumHeight(140);
    layout->addWidget(m_notesEdit, 1);

    m_timeFillerActivitiesLabel = new QLabel(
        translated("Time Filler Activities"), body);
    m_timeFillerActivitiesLabel->setObjectName(
        QStringLiteral("classEditorTimeFillerLabel"));
    layout->addWidget(m_timeFillerActivitiesLabel);
    m_timeFillerActivitiesEdit = new QTextEdit(body);
    m_timeFillerActivitiesEdit->setObjectName(
        QStringLiteral("classEditorTimeFillerActivities"));
    m_timeFillerActivitiesEdit->setAcceptRichText(false);
    m_timeFillerActivitiesEdit->setMinimumHeight(140);
    layout->addWidget(m_timeFillerActivitiesEdit, 1);

    m_notesStatus = new QLabel(body);
    m_notesStatus->setObjectName(QStringLiteral("classEditorNotesStatus"));
    m_notesStatus->setProperty("role", QStringLiteral("status"));
    m_notesStatus->setWordWrap(true);
    layout->addWidget(m_notesStatus);
    auto* const actions = new QHBoxLayout();
    m_retryNotesButton = new QPushButton(translated("Retry"), body);
    m_retryNotesButton->setObjectName(QStringLiteral("classEditorRetryNotes"));
    m_saveNotesButton = new QPushButton(translated("Save Changes"), body);
    m_saveNotesButton->setObjectName(QStringLiteral("classEditorSaveNotes"));
    actions->addWidget(m_retryNotesButton);
    actions->addStretch(1);
    actions->addWidget(m_saveNotesButton);
    layout->addLayout(actions);
    layout->addStretch(1);

    connect(m_notesEdit, &QTextEdit::textChanged, this, [this] {
        if (m_loading)
        {
            return;
        }
        m_notesDirty = m_notesEdit->toPlainText().trimmed() != m_savedNotes
            || m_timeFillerActivitiesEdit->toPlainText().trimmed()
                != m_savedTimeFillerActivities;
        updateDirtyState();
        scheduleNotesSave();
    });
    connect(m_timeFillerActivitiesEdit, &QTextEdit::textChanged, this, [this] {
        if (m_loading)
        {
            return;
        }
        m_notesDirty = m_notesEdit->toPlainText().trimmed() != m_savedNotes
            || m_timeFillerActivitiesEdit->toPlainText().trimmed()
                != m_savedTimeFillerActivities;
        updateDirtyState();
        scheduleNotesSave();
    });
    connect(m_retryNotesButton, &QPushButton::clicked, this, [this] {
        m_notesReadAttempted = false;
        readNotes();
        updateDirtyState();
    });
    connect(m_saveNotesButton, &QPushButton::clicked, this, [this] {
        QString message;
        if (!saveNotes(&message) && !message.isEmpty())
        {
            setNotesStatus(message, true, false);
        }
    });
    for (QWidget* const control : {static_cast<QWidget*>(m_notesEdit),
             static_cast<QWidget*>(m_timeFillerActivitiesEdit)})
    {
        control->setEnabled(false);
    }
    m_retryNotesButton->hide();
    m_saveNotesButton->setEnabled(false);
    return body;
}

QWidget* SelectedClassEditor::buildAnalyticsTab()
{
    auto* const scroll = new QScrollArea(m_analyticsTab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* const body = new QWidget(scroll);
    auto* const layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(10);

    auto* const headingRow = new QWidget(body);
    auto* const headingLayout = new QHBoxLayout(headingRow);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    auto* const heading = new QLabel(translated("Class Analytics"), headingRow);
    heading->setObjectName(QStringLiteral("classEditorAnalyticsHeading"));
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 2);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    headingLayout->addWidget(heading);
    headingLayout->addStretch(1);
    auto* const evaluationLabel = new QLabel(
        translated("Evaluation"), headingRow);
    evaluationLabel->setObjectName(QStringLiteral("classEditorAnalyticsEvaluationLabel"));
    headingLayout->addWidget(evaluationLabel);
    m_analyticsEvaluation = new QComboBox(headingRow);
    m_analyticsEvaluation->setObjectName(
        QStringLiteral("classEditorAnalyticsEvaluation"));
    m_analyticsEvaluation->addItem(translated("All"), QString{});
    m_analyticsEvaluation->addItem(translated("Winter"), QStringLiteral("Winter"));
    m_analyticsEvaluation->addItem(
        translated("Speech Contest"), QStringLiteral("Speech Contest"));
    m_analyticsEvaluation->addItem(translated("Summer"), QStringLiteral("Summer"));
    m_analyticsEvaluation->addItem(translated("Fall"), QStringLiteral("Fall"));
    headingLayout->addWidget(m_analyticsEvaluation);
    layout->addWidget(headingRow);

    m_analyticsStatus = new QLabel(body);
    m_analyticsStatus->setObjectName(QStringLiteral("classEditorAnalyticsStatus"));
    m_analyticsStatus->setProperty("role", QStringLiteral("status"));
    m_analyticsStatus->setWordWrap(true);
    layout->addWidget(m_analyticsStatus);
    m_retryAnalyticsButton = new QPushButton(translated("Retry"), body);
    m_retryAnalyticsButton->setObjectName(
        QStringLiteral("classEditorRetryAnalytics"));
    m_retryAnalyticsButton->hide();
    layout->addWidget(m_retryAnalyticsButton, 0, Qt::AlignRight);
    m_analyticsEmptyState = new QLabel(
        translated("No analytics data is available for this class."), body);
    m_analyticsEmptyState->setObjectName(QStringLiteral("classEditorAnalyticsEmpty"));
    m_analyticsEmptyState->setAlignment(Qt::AlignCenter);
    m_analyticsEmptyState->setWordWrap(true);
    m_analyticsEmptyState->hide();
    layout->addWidget(m_analyticsEmptyState);
    m_analyticsSummary = new SelectedClassAnalyticsSummaryWidget(body);
    layout->addWidget(m_analyticsSummary);

    m_analyticsRankingTable = new QTableView(body);
    m_analyticsRankingTable->setObjectName(
        QStringLiteral("classEditorAnalyticsRankingTable"));
    m_analyticsRankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_analyticsRankingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_analyticsRankingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_analyticsRankingTable->setAlternatingRowColors(true);
    m_analyticsRankingTable->setWordWrap(false);
    m_analyticsRankingTable->verticalHeader()->hide();
    m_analyticsRankingTable->horizontalHeader()->setStretchLastSection(true);
    m_analyticsRankingModel = new SelectedClassAnalyticsModel(
        m_analyticsRankingTable);
    m_analyticsRankingTable->setModel(m_analyticsRankingModel);
    layout->addWidget(m_analyticsRankingTable, 1);
    layout->addStretch(1);
    scroll->setWidget(body);

    connect(m_analyticsEvaluation,
        qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](const int index) {
            if (m_loading || !m_classId || index < 0)
            {
                return;
            }
            const QString selected = m_analyticsEvaluation->itemData(index).toString();
            m_analyticsSelectedEvaluationName = selected.isEmpty()
                ? std::nullopt
                : std::optional<std::u16string>(selected.toStdU16String());
            m_analyticsDefaultSelectionPending = false;
            readAnalytics(false);
        });
    connect(m_retryAnalyticsButton, &QPushButton::clicked, this, [this] {
        readAnalytics(m_analyticsDefaultSelectionPending);
    });
    m_analyticsEvaluation->setEnabled(false);
    m_analyticsStatus->hide();
    m_retryAnalyticsButton->hide();
    return scroll;
}

void SelectedClassEditor::loadClass(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const QString& classNameFallback
    )
{
    if (m_classId == classId && m_databasePath == databasePath && m_detailsLoaded)
    {
        return;
    }
    m_detailsAutosave->stop();
    m_notesAutosave->stop();
    m_classId = classId;
    m_databasePath = databasePath;
    m_classNameFallback = classNameFallback.trimmed();
    m_detailsSnapshot.reset();
    m_coTeacherSnapshot.reset();
    m_teacherChoices.reset();
    m_selectedTeacherDetails.reset();
    m_selectedTeacherDetailsId.reset();
    m_savedCoTeacherId.reset();
    if (m_coTeacher)
    {
        const QSignalBlocker blocker(m_coTeacher);
        m_coTeacher->clear();
        m_coTeacherNameLabel->clear();
        m_coTeacherRoomLabel->clear();
        m_coTeacherInternetLabel->clear();
        m_coTeacherWifiNameLabel->clear();
        m_coTeacherWifiPasswordLabel->clear();
        m_coTeacherProjectionLabel->clear();
        m_coTeacherZoomIdLabel->clear();
        m_coTeacherZoomPasswordLabel->clear();
        setCoTeacherStatus({}, false);
    }
    m_detailsLoaded = false;
    m_coTeacherReadAttempted = false;
    m_coTeacherChoicesReadAttempted = false;
    m_detailsDirty = false;
    m_coTeacherDirty = false;
    m_notesLoaded = false;
    m_notesReadAttempted = false;
    m_notesDirty = false;
    m_analyticsReadAttempted = false;
    m_analyticsDefaultSelectionPending = true;
    m_analyticsSelectedEvaluationName.reset();
    m_analyticsSnapshot.reset();
    m_savedNotes.clear();
    m_savedTimeFillerActivities.clear();
    m_notesSubtitle.clear();
    m_loading = true;
    ensureUi();
    m_sections->setCurrentIndex(0);
    if (m_analyticsEvaluation)
    {
        const QSignalBlocker blocker(m_analyticsEvaluation);
        m_analyticsEvaluation->setCurrentIndex(0);
    }
    m_previousSectionIndex = 0;
    m_emptyState->hide();
    m_sections->show();
    m_loading = false;
    if (m_rosterEditor)
    {
        m_rosterEditor->clear();
    }
    clearNotesDisplay();
    clearAnalyticsDisplay();
    readClassDetails();
    updateDirtyState();
}

void SelectedClassEditor::clear()
{
    m_detailsAutosave->stop();
    m_notesAutosave->stop();
    m_classId.reset();
    m_databasePath.reset();
    m_classNameFallback.clear();
    m_detailsSnapshot.reset();
    m_coTeacherSnapshot.reset();
    m_teacherChoices.reset();
    m_selectedTeacherDetails.reset();
    m_selectedTeacherDetailsId.reset();
    m_savedCoTeacherId.reset();
    m_detailsLoaded = false;
    m_coTeacherReadAttempted = false;
    m_coTeacherChoicesReadAttempted = false;
    m_detailsDirty = false;
    m_coTeacherDirty = false;
    m_notesLoaded = false;
    m_notesReadAttempted = false;
    m_notesDirty = false;
    m_analyticsReadAttempted = false;
    m_analyticsDefaultSelectionPending = true;
    m_analyticsSelectedEvaluationName.reset();
    m_analyticsSnapshot.reset();
    m_savedNotes.clear();
    m_savedTimeFillerActivities.clear();
    m_notesSubtitle.clear();
    if (m_rosterEditor)
    {
        m_rosterEditor->clear();
    }
    if (m_sections)
    {
        m_loading = true;
        m_sections->hide();
        m_sections->setCurrentIndex(0);
        m_previousSectionIndex = 0;
        for (QLineEdit* const field : {m_classColor, m_fontColor})
        {
            field->clear();
        }
        for (QComboBox* const choice : {m_grade, m_level, m_readingBook, m_essayBook})
        {
            choice->clear();
        }
        for (QTableWidget* const table : {m_regularSchedule, m_intensiveSchedule})
        {
            table->setRowCount(0);
        }
        m_regularScheduleAdd->setEnabled(false);
        m_intensiveScheduleAdd->setEnabled(false);
        m_teacherLabel->clear();
        m_studentCountLabel->clear();
        if (m_coTeacher)
        {
            m_coTeacher->clear();
            updateCoTeacherDisplay();
        }
        setStatus({}, false);
        setCoTeacherStatus({}, false);
        clearNotesDisplay();
        clearAnalyticsDisplay();
        if (m_analyticsEvaluation)
        {
            const QSignalBlocker blocker(m_analyticsEvaluation);
            m_analyticsEvaluation->setCurrentIndex(0);
            m_analyticsEvaluation->setEnabled(false);
        }
        if (m_retryDetailsButton)
        {
            m_retryDetailsButton->hide();
        }
        m_loading = false;
    }
    m_emptyState->show();
    updateDirtyState();
}

bool SelectedClassEditor::resolveUnsavedChanges(const QString& reason)
{
    if (!hasUnsavedChanges())
    {
        return true;
    }
    switch (promptUnsavedChanges(reason))
    {
    case UnsavedChangesChoice::Save:
        return saveChanges();
    case UnsavedChangesChoice::Discard:
        return discardChanges();
    case UnsavedChangesChoice::Cancel:
        return false;
    }
    return false;
}

bool SelectedClassEditor::saveChanges(QString* error)
{
    if (m_detailsDirty && !saveDetails(error))
    {
        return false;
    }
    if (m_coTeacherDirty && !saveCoTeacher(error))
    {
        return false;
    }
    if (m_notesDirty && !saveNotes(error))
    {
        return false;
    }
    if (m_rosterEditor && m_rosterEditor->hasUnsavedChanges()
        && !m_rosterEditor->saveChanges(true))
    {
        if (error)
        {
            *error = translated("Roster changes could not be saved.");
        }
        return false;
    }
    return true;
}

bool SelectedClassEditor::discardChanges(QString* error)
{
    if (m_rosterEditor && m_rosterEditor->hasUnsavedChanges()
        && !m_rosterEditor->discardChanges())
    {
        if (error)
        {
            *error = translated("Roster changes could not be reloaded.");
        }
        return false;
    }
    if (!m_classId || !m_databasePath)
    {
        m_detailsDirty = false;
        m_coTeacherDirty = false;
        m_notesDirty = false;
        updateDirtyState();
        return true;
    }
    m_detailsAutosave->stop();
    m_notesAutosave->stop();
    m_detailsDirty = false;
    m_coTeacherDirty = false;
    m_notesDirty = false;
    m_detailsLoaded = false;
    m_coTeacherReadAttempted = false;
    m_coTeacherChoicesReadAttempted = false;
    m_detailsSnapshot.reset();
    m_coTeacherSnapshot.reset();
    m_teacherChoices.reset();
    m_selectedTeacherDetails.reset();
    m_selectedTeacherDetailsId.reset();
    m_notesLoaded = false;
    m_notesReadAttempted = false;
    m_savedNotes.clear();
    m_savedTimeFillerActivities.clear();
    readClassDetails();
    if (m_sections && m_sections->currentIndex() == 1)
    {
        if (!loadCoTeacherIfNeeded())
        {
            if (error)
            {
                *error = m_coTeacherStatus->text();
            }
            return false;
        }
    }
    if (m_sections && m_sections->currentIndex() == 2
        && !loadNotesIfNeeded())
    {
        if (error)
        {
            *error = m_notesStatus ? m_notesStatus->text() : QString{};
        }
        return false;
    }
    updateDirtyState();
    return m_detailsLoaded;
}

void SelectedClassEditor::setStatus(const QString& message, const bool error)
{
    if (!m_detailsStatus)
    {
        return;
    }
    m_detailsStatus->setText(message);
    m_detailsStatus->setStyleSheet(error ? QStringLiteral("color: #a32121;") : QString{});
    m_detailsStatus->setVisible(!message.isEmpty());
}

void SelectedClassEditor::setCoTeacherStatus(const QString& message, const bool error)
{
    if (!m_coTeacherStatus)
    {
        return;
    }
    m_coTeacherStatus->setText(message);
    m_coTeacherStatus->setStyleSheet(error ? QStringLiteral("color: #a32121;") : QString{});
    m_coTeacherStatus->setVisible(!message.isEmpty());
    m_retryCoTeacherButton->setVisible(error);
}

void SelectedClassEditor::setNotesStatus(
    const QString& message,
    const bool error,
    const bool retryable
    )
{
    if (!m_notesStatus)
    {
        return;
    }
    m_notesStatus->setText(message);
    m_notesStatus->setStyleSheet(error ? QStringLiteral("color: #a32121;") : QString{});
    m_notesStatus->setVisible(!message.isEmpty());
    if (m_retryNotesButton)
    {
        m_retryNotesButton->setVisible(retryable);
    }
}

void SelectedClassEditor::clearNotesDisplay()
{
    if (!m_notesEdit || !m_timeFillerActivitiesEdit)
    {
        return;
    }
    m_loading = true;
    m_notesEdit->clear();
    m_timeFillerActivitiesEdit->clear();
    m_notesEdit->setEnabled(false);
    m_timeFillerActivitiesEdit->setEnabled(false);
    m_notesSubtitle = m_classNameFallback;
    if (m_notesSubtitle.isEmpty() && m_classId)
    {
        m_notesSubtitle = translated("Class %1").arg(text(m_classId->value()));
    }
    if (m_notesSubtitleLabel)
    {
        m_notesSubtitleLabel->setText(m_notesSubtitle);
    }
    setNotesStatus({}, false);
    if (m_saveNotesButton)
    {
        m_saveNotesButton->setEnabled(false);
    }
    m_loading = false;
}

void SelectedClassEditor::clearDetailsDisplay()
{
    if (!m_sections)
    {
        return;
    }
    m_loading = true;
    for (QComboBox* const choice : {m_grade, m_level, m_readingBook, m_essayBook})
    {
        choice->clear();
    }
    for (QLineEdit* const field : {m_classColor, m_fontColor})
    {
        field->clear();
    }
    for (QTableWidget* const table : {m_regularSchedule, m_intensiveSchedule})
    {
        table->setRowCount(0);
    }
    m_teacherLabel->clear();
    m_studentCountLabel->clear();
    const std::array<QWidget*, 10> controls{
        m_grade, m_level, m_readingBook, m_essayBook, m_classColor,
        m_fontColor, m_chooseColor, m_regularSchedule, m_intensiveSchedule,
        m_saveDetailsButton
    };
    for (QWidget* const control : controls)
    {
        control->setEnabled(false);
    }
    m_regularScheduleAdd->setEnabled(false);
    m_intensiveScheduleAdd->setEnabled(false);
    m_loading = false;
}

void SelectedClassEditor::readClassDetails()
{
    if (!m_classId || !m_databasePath || !m_actions.readDetails)
    {
        m_detailsLoaded = false;
        m_detailsSnapshot.reset();
        if (!m_detailsDirty)
        {
            clearDetailsDisplay();
        }
        setStatus(translated("Class details could not be loaded. Retry this section."), true);
        if (m_retryDetailsButton)
        {
            m_retryDetailsButton->show();
        }
        return;
    }
    Application::ClassDetailsPageReadResult read = [&] {
        try
        {
            return m_actions.readDetails(*m_databasePath, *m_classId);
        }
        catch (...)
        {
            return Application::ClassDetailsPageReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The class details could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!read)
    {
        m_detailsLoaded = false;
        m_detailsSnapshot.reset();
        if (!m_detailsDirty)
        {
            clearDetailsDisplay();
        }
        setStatus(errorText(read.error()), true);
        if (m_retryDetailsButton)
        {
            m_retryDetailsButton->show();
        }
        return;
    }
    m_detailsSnapshot = std::move(read.value());
    QString ignored;
    if (!applyDetailsSnapshot(*m_detailsSnapshot, &ignored))
    {
        m_detailsLoaded = false;
        if (!m_detailsDirty)
        {
            clearDetailsDisplay();
        }
        setStatus(ignored, true);
        if (m_retryDetailsButton)
        {
            m_retryDetailsButton->show();
        }
        return;
    }
    m_detailsLoaded = true;
    m_regularScheduleAdd->setEnabled(m_regularSchedule->rowCount()
        < static_cast<int>(Application::kClassesNavigationMaximumScheduleRows));
    m_intensiveScheduleAdd->setEnabled(m_intensiveSchedule->rowCount()
        < static_cast<int>(Application::kClassesNavigationMaximumScheduleRows));
    if (m_retryDetailsButton)
    {
        m_retryDetailsButton->hide();
    }
    setStatus({}, false);
}

bool SelectedClassEditor::loadNotesIfNeeded()
{
    if (!m_notesReadAttempted)
    {
        readNotes();
    }
    return m_notesLoaded;
}

void SelectedClassEditor::loadRosterIfNeeded()
{
    if (m_rosterEditor && m_databasePath && m_classId)
    {
        m_rosterEditor->enterClassSection(*m_databasePath, *m_classId);
    }
}

bool SelectedClassEditor::loadAnalyticsIfNeeded()
{
    if (!m_analyticsReadAttempted)
    {
        readAnalytics(m_analyticsDefaultSelectionPending);
    }
    return m_analyticsSnapshot.has_value();
}

void SelectedClassEditor::readAnalytics(const bool useDefaultSelection)
{
    m_analyticsReadAttempted = true;
    if (!m_classId || !m_databasePath || !m_actions.readAnalytics)
    {
        m_analyticsSnapshot.reset();
        clearAnalyticsDisplay();
        m_analyticsStatus->setText(translated(
            "Class Analytics could not be loaded. Retry this section."));
        m_analyticsStatus->setStyleSheet(QStringLiteral("color: #a32121;"));
        m_analyticsStatus->show();
        m_retryAnalyticsButton->show();
        return;
    }

    const Application::ClassAnalyticsPageReadRequest request{
        .classId = *m_classId,
        .useDefaultSelection = useDefaultSelection,
        .selectedEvaluationName = m_analyticsSelectedEvaluationName
    };
    Application::ClassAnalyticsPageReadResult read = [&] {
        try
        {
            return m_actions.readAnalytics(*m_databasePath, request);
        }
        catch (...)
        {
            return Application::ClassAnalyticsPageReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Class Analytics could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!read)
    {
        m_analyticsSnapshot.reset();
        clearAnalyticsDisplay();
        m_analyticsStatus->setText(errorText(read.error()));
        m_analyticsStatus->setStyleSheet(QStringLiteral("color: #a32121;"));
        m_analyticsStatus->show();
        m_retryAnalyticsButton->show();
        return;
    }
    if (read.value().classId != *m_classId)
    {
        m_analyticsSnapshot.reset();
        clearAnalyticsDisplay();
        m_analyticsStatus->setText(translated(
            "Class Analytics returned a different class."));
        m_analyticsStatus->setStyleSheet(QStringLiteral("color: #a32121;"));
        m_analyticsStatus->show();
        m_retryAnalyticsButton->show();
        return;
    }

    m_analyticsDefaultSelectionPending = false;
    m_analyticsSelectedEvaluationName = read.value().selectedEvaluationName;
    m_analyticsSnapshot = std::move(read.value());
    const QString selectedName = m_analyticsSelectedEvaluationName
        ? QString::fromStdU16String(*m_analyticsSelectedEvaluationName)
        : QString{};
    const int selectedIndex = m_analyticsEvaluation->findData(selectedName);
    {
        const QSignalBlocker blocker(m_analyticsEvaluation);
        m_analyticsEvaluation->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    }
    m_analyticsEvaluation->setEnabled(true);
    m_analyticsStatus->clear();
    m_analyticsStatus->hide();
    m_retryAnalyticsButton->hide();
    renderAnalytics();
}

void SelectedClassEditor::clearAnalyticsDisplay()
{
    m_analyticsSnapshot.reset();
    if (m_analyticsRankingModel)
    {
        m_analyticsRankingModel->setRankings(nullptr);
    }
    if (!m_analyticsStatus)
    {
        return;
    }
    m_analyticsStatus->clear();
    m_analyticsStatus->hide();
    m_retryAnalyticsButton->hide();
    m_analyticsEmptyState->hide();
    m_analyticsSummary->setDashboard(nullptr);
    m_analyticsSummary->hide();
    m_analyticsRankingTable->hide();
    m_analyticsEvaluation->setEnabled(m_classId.has_value());
}

void SelectedClassEditor::renderAnalytics()
{
    if (!m_analyticsSnapshot)
    {
        return;
    }
    const Application::ClassAnalyticsDashboard& dashboard =
        m_analyticsSnapshot->dashboard;
    const Application::ClassAnalyticsSnapshot& selected =
        dashboard.selectedSnapshot;
    m_analyticsEmptyState->setVisible(!selected.hasData);
    if (!selected.hasData)
    {
        m_analyticsRankingModel->setRankings(nullptr);
        m_analyticsSummary->setDashboard(nullptr);
        m_analyticsSummary->hide();
        m_analyticsRankingTable->hide();
        return;
    }
    m_analyticsSummary->setDashboard(&dashboard);
    m_analyticsSummary->show();
    m_analyticsRankingTable->show();
    m_analyticsRankingModel->setRankings(&selected.rankings);
}

void SelectedClassEditor::readNotes()
{
    m_notesReadAttempted = true;
    if (!m_classId || !m_databasePath || !m_actions.readNotes)
    {
        if (!m_notesDirty)
        {
            m_notesLoaded = false;
            clearNotesDisplay();
        }
        setNotesStatus(translated(
            "Class notes could not be loaded. Retry this section."), true, true);
        return;
    }

    Application::ClassNotesPageReadResult read = [&] {
        try
        {
            return m_actions.readNotes(*m_databasePath, *m_classId);
        }
        catch (...)
        {
            return Application::ClassNotesPageReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Class notes could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!read)
    {
        if (!m_notesDirty)
        {
            m_notesLoaded = false;
            clearNotesDisplay();
        }
        setNotesStatus(errorText(read.error()), true, true);
        return;
    }
    const Application::ClassNotesPageReadSnapshot& snapshot = read.value();
    if (snapshot.classId != *m_classId)
    {
        if (!m_notesDirty)
        {
            m_notesLoaded = false;
            clearNotesDisplay();
        }
        setNotesStatus(translated("Class notes returned a different class."),
            true, true);
        return;
    }
    if (!snapshot.classFields)
    {
        if (!m_notesDirty)
        {
            m_notesLoaded = false;
            clearNotesDisplay();
        }
        setNotesStatus(errorText(snapshot.classFields.error()), true, true);
        return;
    }

    const Application::ClassNotesPageFields& fields = snapshot.classFields.value();
    QString teacherDisplayName;
    if (snapshot.teacherDisplayName)
    {
        teacherDisplayName = text(snapshot.teacherDisplayName.value()).trimmed();
    }
    QString levelText = text(fields.classGrade).trimmed();
    const QString level = text(fields.classLevel).trimmed();
    if (!level.isEmpty())
    {
        levelText = levelText.isEmpty()
            ? level : QStringLiteral("%1 %2").arg(levelText, level);
    }
    if (levelText.isEmpty())
    {
        levelText = m_classNameFallback.isEmpty()
            ? translated("Unknown Class")
            : m_classNameFallback;
    }
    if (teacherDisplayName.isEmpty())
    {
        teacherDisplayName = QStringLiteral("No Teacher");
    }
    m_notesSubtitle = QStringLiteral("%1 • %2")
        .arg(levelText, teacherDisplayName);
    if (!fields.regularSchedule.empty())
    {
        QString day = text(fields.regularSchedule.front().day);
        const QStringList dayAbbreviations{
            QStringLiteral("Monday"), QStringLiteral("Mon"),
            QStringLiteral("Tuesday"), QStringLiteral("Tues"),
            QStringLiteral("Wednesday"), QStringLiteral("Wed"),
            QStringLiteral("Thursday"), QStringLiteral("Thurs"),
            QStringLiteral("Friday"), QStringLiteral("Fri"),
            QStringLiteral("Saturday"), QStringLiteral("Sat"),
            QStringLiteral("Sunday"), QStringLiteral("Sun")
        };
        for (qsizetype index = 0; index + 1 < dayAbbreviations.size(); index += 2)
        {
            if (day == dayAbbreviations.at(index))
            {
                day = dayAbbreviations.at(index + 1);
                break;
            }
        }
        QString startTime = text(fields.regularSchedule.front().startTime);
        startTime.replace(QStringLiteral(" AM"), QString{});
        startTime.replace(QStringLiteral(" PM"), QString{});
        m_notesSubtitle += QStringLiteral(" • %1 (%2)").arg(day, startTime);
    }
    if (m_notesSubtitleLabel)
    {
        m_notesSubtitleLabel->setText(m_notesSubtitle);
    }

    m_notesLoaded = true;
    m_savedNotes = text(fields.notes).trimmed();
    m_savedTimeFillerActivities = text(fields.timeFillerActivities).trimmed();
    if (!m_notesDirty)
    {
        m_loading = true;
        m_notesEdit->setPlainText(m_savedNotes);
        m_timeFillerActivitiesEdit->setPlainText(m_savedTimeFillerActivities);
        m_notesDirty = false;
        m_loading = false;
    }
    m_notesEdit->setEnabled(true);
    m_timeFillerActivitiesEdit->setEnabled(true);

    if (!snapshot.teacherDisplayName)
    {
        setNotesStatus(translated(
            "The teacher display name could not be loaded. Notes can still be saved."),
            true, true);
    }
    else
    {
        setNotesStatus({}, false);
    }
    updateDirtyState();
}

bool SelectedClassEditor::applyDetailsSnapshot(
    const Application::ClassDetailsPageReadSnapshot& snapshot,
    QString* error
    )
{
    if (snapshot.classId != m_classId)
    {
        if (error)
        {
            *error = translated("Class details returned a different class.");
        }
        return false;
    }
    if (!snapshot.classFields)
    {
        if (error)
        {
            *error = errorText(snapshot.classFields.error());
        }
        return false;
    }
    const auto& fields = snapshot.classFields.value();
    m_loading = true;
    updateGradeChoices(false);
    m_grade->setCurrentText(text(fields.classGrade));
    updateGradeChoices(false);
    m_level->setCurrentText(text(fields.classLevel));
    updateBookChoices(false);
    m_readingBook->setCurrentText(text(fields.readingBook));
    m_essayBook->setCurrentText(text(fields.essayBook));
    m_classColor->setText(text(fields.classColor));
    m_fontColor->setText(text(fields.fontColor));
    populateSchedule(m_regularSchedule, fields.regularSchedule);
    populateSchedule(m_intensiveSchedule, fields.intensiveSchedule);
    m_teacherLabel->setText(snapshot.teacherDisplayName
        ? text(snapshot.teacherDisplayName.value())
        : translated("Unavailable"));
    m_studentCountLabel->setText(snapshot.studentCount
        ? QString::number(snapshot.studentCount.value().rawValue())
        : translated("Unavailable"));
    const std::array<QWidget*, 10> controls{
        m_grade, m_level, m_readingBook, m_essayBook, m_classColor,
        m_fontColor, m_chooseColor, m_regularSchedule, m_intensiveSchedule,
        m_saveDetailsButton
    };
    for (QWidget* const control : controls)
    {
        control->setEnabled(true);
    }
    if (!snapshot.teacherDisplayName)
    {
        m_teacherLabel->setToolTip(errorText(snapshot.teacherDisplayName.error()));
    }
    if (!snapshot.studentCount)
    {
        m_studentCountLabel->setToolTip(errorText(snapshot.studentCount.error()));
    }
    m_loading = false;
    m_detailsDirty = false;
    updateDirtyState();
    return true;
}

void SelectedClassEditor::readCoTeacher()
{
    m_coTeacherReadAttempted = true;
    if (!m_classId || !m_databasePath || !m_actions.readCoTeacher)
    {
        setCoTeacherStatus(translated("Co-Teacher details could not be loaded."), true);
        return;
    }
    Application::ClassCoTeacherPageReadResult read = [&] {
        try
        {
            return m_actions.readCoTeacher(*m_databasePath, *m_classId);
        }
        catch (...)
        {
            return Application::ClassCoTeacherPageReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The Co-Teacher details could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!read)
    {
        m_coTeacherSnapshot.reset();
        setCoTeacherStatus(errorText(read.error()), true);
        return;
    }
    m_coTeacherSnapshot = std::move(read.value());
    if (!m_coTeacherSnapshot->classFields)
    {
        setCoTeacherStatus(errorText(m_coTeacherSnapshot->classFields.error()), true);
        return;
    }
    m_savedCoTeacherId = m_coTeacherSnapshot->classFields.value().selectedTeacherId;
    m_loading = true;
    updateCoTeacherDisplay();
    m_loading = false;
    if (!m_coTeacherSnapshot->teacherDisplayName)
    {
        setCoTeacherStatus(errorText(m_coTeacherSnapshot->teacherDisplayName.error()), true);
    }
    else
    {
        setCoTeacherStatus({}, false);
    }
    updateDirtyState();
}

void SelectedClassEditor::readTeacherChoices()
{
    m_coTeacherChoicesReadAttempted = true;
    if (!m_databasePath || !m_actions.readTeacherChoices)
    {
        setCoTeacherStatus(translated("Teacher choices could not be loaded."), true);
        return;
    }
    Application::ClassCoTeacherTeacherChoicesResult read = [&] {
        try
        {
            return m_actions.readTeacherChoices(*m_databasePath);
        }
        catch (...)
        {
            return Application::ClassCoTeacherTeacherChoicesResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Teacher choices could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!read)
    {
        m_teacherChoices.reset();
        setCoTeacherStatus(errorText(read.error()), true);
        return;
    }
    m_teacherChoices = std::move(read.value());
    setCoTeacherStatus({}, false);
    m_loading = true;
    updateTeacherDisplay();
    m_loading = false;
}

bool SelectedClassEditor::loadCoTeacherIfNeeded()
{
    if (!m_coTeacherReadAttempted)
    {
        readCoTeacher();
    }
    if (!m_coTeacherChoicesReadAttempted)
    {
        readTeacherChoices();
    }
    return m_coTeacherSnapshot.has_value()
        && m_coTeacherSnapshot->classFields.hasValue()
        && m_teacherChoices.has_value();
}

void SelectedClassEditor::updateGradeChoices(const bool resetDependentValues)
{
    const QString currentGrade = m_grade->currentText();
    const QString currentLevel = m_level->currentText();
    const QString currentReading = m_readingBook->currentText();
    const QString currentEssay = m_essayBook->currentText();
    const QSignalBlocker gradeBlocker(m_grade);
    const QSignalBlocker levelBlocker(m_level);
    const QSignalBlocker readingBlocker(m_readingBook);
    const QSignalBlocker essayBlocker(m_essayBook);
    m_grade->clear();
    m_grade->addItem(QString{});
    const auto grades = ::ClassMngr::Domain::ClassDetailsCatalog::grades();
    for (const std::string_view grade : grades)
    {
        m_grade->addItem(QString::fromUtf8(grade.data(),
            static_cast<qsizetype>(grade.size())));
    }
    m_grade->setCurrentText(currentGrade);
    m_level->clear();
    m_level->addItem(QString{});
    const QByteArray gradeBytes = m_grade->currentText().toUtf8();
    const auto levels = ::ClassMngr::Domain::ClassDetailsCatalog::levelsForGrade(
        std::string_view(gradeBytes.constData(), static_cast<std::size_t>(gradeBytes.size())));
    for (const std::string_view level : levels)
    {
        m_level->addItem(QString::fromUtf8(level.data(),
            static_cast<qsizetype>(level.size())));
    }
    m_level->setCurrentText(resetDependentValues ? QString{} : currentLevel);
    updateBookChoices(resetDependentValues);
    if (!resetDependentValues)
    {
        m_readingBook->setCurrentText(currentReading);
        m_essayBook->setCurrentText(currentEssay);
    }
}

void SelectedClassEditor::updateBookChoices(const bool resetSelectedValues)
{
    const QString currentReading = m_readingBook->currentText();
    const QString currentEssay = m_essayBook->currentText();
    const QSignalBlocker readingBlocker(m_readingBook);
    const QSignalBlocker essayBlocker(m_essayBook);
    m_readingBook->clear();
    m_essayBook->clear();
    const QByteArray gradeBytes = m_grade->currentText().toUtf8();
    const QByteArray levelBytes = m_level->currentText().toUtf8();
    const std::string_view grade(gradeBytes.constData(),
        static_cast<std::size_t>(gradeBytes.size()));
    const std::string_view level(levelBytes.constData(),
        static_cast<std::size_t>(levelBytes.size()));
    for (const std::string_view book : ::ClassMngr::Domain::ClassDetailsCatalog::readingBooks(grade, level))
    {
        m_readingBook->addItem(QString::fromUtf8(book.data(),
            static_cast<qsizetype>(book.size())));
    }
    for (const std::string_view book : ::ClassMngr::Domain::ClassDetailsCatalog::essayBooks(grade, level))
    {
        m_essayBook->addItem(QString::fromUtf8(book.data(),
            static_cast<qsizetype>(book.size())));
    }
    m_readingBook->setCurrentText(resetSelectedValues ? QString{} : currentReading);
    m_essayBook->setCurrentText(resetSelectedValues ? QString{} : currentEssay);
}

void SelectedClassEditor::updateTeacherDisplay()
{
    if (!m_teacherChoices || !m_coTeacher)
    {
        return;
    }
    const QString currentId = m_coTeacherDirty
        ? m_coTeacher->currentData().toString()
        : m_savedCoTeacherId
            ? QString::fromStdString(m_savedCoTeacherId->value())
            : QString{};
    const bool korean = m_coTeacherLocale->currentData().toString()
        == QStringLiteral("ko");
    const QSignalBlocker blocker(m_coTeacher);
    m_rebuildingChoices = true;
    m_coTeacher->clear();
    m_coTeacher->addItem(translated("Unassigned"), QString{});
    for (const Application::ClassCoTeacherTeacherChoice& teacher :
         m_teacherChoices->teachers)
    {
        m_coTeacher->addItem(teacherName(teacher, korean),
            QString::fromStdString(teacher.teacherId.value()));
    }
    if (!currentId.isEmpty() && m_coTeacher->findData(currentId) < 0)
    {
        m_coTeacher->addItem(translated("Unavailable (%1)").arg(currentId), currentId);
    }
    const int index = currentId.isEmpty() ? 0 : m_coTeacher->findData(currentId);
    m_coTeacher->setCurrentIndex(index >= 0 ? index : 0);
    m_rebuildingChoices = false;
    updateCoTeacherDisplay();
}

void SelectedClassEditor::updateCoTeacherDisplay()
{
    if (!m_coTeacher)
    {
        return;
    }
    const QString selectedId = m_coTeacher->currentData().toString();
    const bool korean = m_coTeacherLocale->currentData().toString()
        == QStringLiteral("ko");
    const auto found = m_teacherChoices
        ? std::find_if(m_teacherChoices->teachers.cbegin(),
            m_teacherChoices->teachers.cend(), [&selectedId](const auto& teacher) {
                return QString::fromStdString(teacher.teacherId.value()) == selectedId;
            })
        : std::vector<Application::ClassCoTeacherTeacherChoice>::const_iterator{};
    if (!m_teacherChoices || selectedId.isEmpty()
        || found == m_teacherChoices->teachers.cend())
    {
        m_selectedTeacherDetails.reset();
        m_selectedTeacherDetailsId.reset();
        m_coTeacherNameLabel->setText(selectedId.isEmpty()
            ? translated("Unassigned") : translated("Unavailable"));
        for (QLabel* const label : {m_coTeacherRoomLabel, m_coTeacherInternetLabel,
                m_coTeacherWifiNameLabel, m_coTeacherWifiPasswordLabel,
                m_coTeacherProjectionLabel, m_coTeacherZoomIdLabel,
                m_coTeacherZoomPasswordLabel})
        {
            label->clear();
        }
        if (selectedId.isEmpty())
        {
            setCoTeacherStatus({}, false);
        }
        return;
    }
    m_coTeacherNameLabel->setText(teacherName(*found, korean));
    if (!m_selectedTeacherDetailsId
        || *m_selectedTeacherDetailsId != found->teacherId)
    {
        readSelectedTeacherDetails();
        return;
    }
    if (!m_selectedTeacherDetails)
    {
        for (QLabel* const label : {m_coTeacherRoomLabel, m_coTeacherInternetLabel,
                m_coTeacherWifiNameLabel, m_coTeacherWifiPasswordLabel,
                m_coTeacherProjectionLabel, m_coTeacherZoomIdLabel,
                m_coTeacherZoomPasswordLabel})
        {
            label->clear();
        }
        return;
    }
    m_coTeacherRoomLabel->setText(text(m_selectedTeacherDetails->roomNumber));
    m_coTeacherInternetLabel->setText(text(m_selectedTeacherDetails->internetType));
    m_coTeacherWifiNameLabel->setText(text(m_selectedTeacherDetails->wifiName));
    m_coTeacherWifiPasswordLabel->setText(text(m_selectedTeacherDetails->wifiPassword));
    m_coTeacherProjectionLabel->setText(text(m_selectedTeacherDetails->projectionType));
    m_coTeacherZoomIdLabel->setText(text(m_selectedTeacherDetails->zoomId));
    m_coTeacherZoomPasswordLabel->setText(text(m_selectedTeacherDetails->zoomPassword));
}

void SelectedClassEditor::readSelectedTeacherDetails()
{
    if (!m_coTeacher || !m_teacherChoices)
    {
        return;
    }
    const QString selectedId = m_coTeacher->currentData().toString();
    const auto found = std::find_if(m_teacherChoices->teachers.cbegin(),
        m_teacherChoices->teachers.cend(), [&selectedId](const auto& teacher) {
            return QString::fromStdString(teacher.teacherId.value()) == selectedId;
        });
    if (selectedId.isEmpty() || found == m_teacherChoices->teachers.cend())
    {
        m_selectedTeacherDetails.reset();
        m_selectedTeacherDetailsId.reset();
        setCoTeacherStatus({}, false);
        updateCoTeacherDisplay();
        return;
    }
    if (m_selectedTeacherDetailsId
        && *m_selectedTeacherDetailsId == found->teacherId)
    {
        updateCoTeacherDisplay();
        return;
    }
    m_selectedTeacherDetails.reset();
    m_selectedTeacherDetailsId = found->teacherId;
    if (!m_databasePath || !m_actions.readTeacherDetails)
    {
        setCoTeacherStatus(translated("Teacher details could not be loaded."), true);
        updateCoTeacherDisplay();
        return;
    }
    Domain::Result<Application::ClassCoTeacherTeacherChoice> read = [&] {
        try
        {
            return m_actions.readTeacherDetails(*m_databasePath, found->teacherId);
        }
        catch (...)
        {
            return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The selected teacher details could not be loaded.",
                .recoverable = true
            });
        }
    }();
    if (!read)
    {
        setCoTeacherStatus(errorText(read.error()), true);
        updateCoTeacherDisplay();
        return;
    }
    if (read.value().teacherId != found->teacherId)
    {
        setCoTeacherStatus(translated(
            "Teacher details returned a different teacher."), true);
        updateCoTeacherDisplay();
        return;
    }
    m_selectedTeacherDetails = std::move(read.value());
    setCoTeacherStatus({}, false);
    updateCoTeacherDisplay();
}

void SelectedClassEditor::updateDirtyState()
{
    const bool dirty = hasUnsavedChanges();
    setProperty("dirty", dirty);
    if (m_saveDetailsButton)
    {
        m_saveDetailsButton->setEnabled(m_detailsLoaded && m_detailsDirty);
    }
    if (m_saveCoTeacherButton)
    {
        m_saveCoTeacherButton->setEnabled(
            m_coTeacherSnapshot.has_value() && m_teacherChoices.has_value()
            && m_coTeacherDirty);
    }
    if (m_saveNotesButton)
    {
        m_saveNotesButton->setEnabled(m_notesLoaded && m_notesDirty);
    }
    update();
}

void SelectedClassEditor::scheduleDetailsSave()
{
    if (m_detailsLoaded && m_detailsAutosave
        && m_saveMode == Application::SaveMode::Automatic)
    {
        m_detailsAutosave->start();
    }
}

void SelectedClassEditor::scheduleNotesSave()
{
    if (m_notesLoaded && m_notesAutosave
        && m_saveMode == Application::SaveMode::Automatic)
    {
        m_notesAutosave->start();
    }
}

Application::ClassDetailsValidationInput SelectedClassEditor::detailsDraft() const
{
    Application::ClassDetailsValidationInput input;
    if (m_classId)
    {
        input.classId = *m_classId;
    }
    input.classGrade = m_grade->currentText().toStdU16String();
    input.classLevel = m_level->currentText().toStdU16String();
    input.readingBook = m_readingBook->currentText().toStdU16String();
    input.essayBook = m_essayBook->currentText().toStdU16String();
    input.classColor = m_classColor->text().toStdU16String();
    input.fontColor = m_fontColor->text().toStdU16String();
    input.regularTimes = scheduleRows(m_regularSchedule);
    input.intensiveTimes = scheduleRows(m_intensiveSchedule);
    return input;
}

bool SelectedClassEditor::saveDetails(QString* error)
{
    if (!m_detailsDirty)
    {
        return true;
    }
    if (!m_classId || !m_databasePath || !m_actions.saveDetails || !m_detailsLoaded)
    {
        const QString message = translated("The class details cannot be saved right now.");
        if (error)
        {
            *error = message;
        }
        setStatus(message, true);
        return false;
    }
    Application::ClassDetailsSaveWorkflowResult saved = [&] {
        try
        {
            return m_actions.saveDetails(*m_databasePath, {
                .input = detailsDraft(),
                .saveRegularTimes = true,
                .saveIntensiveTimes = true
            });
        }
        catch (...)
        {
            return Application::ClassDetailsSaveWorkflowResult(
                Application::ClassDetailsSaveWorkflowFailure{
                    Application::ClassDetailsSaveWorkflowStage::Save,
                    {.code = Domain::ErrorCode::Technical,
                     .message = "The class details could not be saved.",
                     .recoverable = true}
                });
        }
    }();
    if (!std::holds_alternative<Application::ClassDetailsSaveWorkflowSuccess>(saved))
    {
        const QString message = workflowError(saved);
        if (error)
        {
            *error = message;
        }
        setStatus(message, true);
        updateDirtyState();
        return false;
    }
    m_detailsAutosave->stop();
    m_detailsDirty = false;
    setStatus(translated("Class details saved."), false);
    readClassDetails();
    updateDirtyState();
    return true;
}

bool SelectedClassEditor::saveCoTeacher(QString* error)
{
    if (!m_coTeacherDirty)
    {
        return true;
    }
    if (!m_classId || !m_databasePath || !m_actions.saveCoTeacher
        || !m_teacherChoices || !m_coTeacherSnapshot)
    {
        const QString message = translated("The Co-Teacher assignment cannot be saved right now.");
        if (error)
        {
            *error = message;
        }
        setCoTeacherStatus(message, true);
        return false;
    }
    const QString selectedId = m_coTeacher->currentData().toString();
    std::optional<Domain::TeacherId> teacherId;
    if (!selectedId.isEmpty())
    {
        teacherId = Domain::TeacherId::fromString(selectedId.toStdString());
        if (!teacherId)
        {
            const QString message = translated("The selected teacher identifier is invalid.");
            if (error)
            {
                *error = message;
            }
            setCoTeacherStatus(message, true);
            return false;
        }
    }
    Domain::Result<void> saved = [&] {
        try
        {
            return m_actions.saveCoTeacher(*m_databasePath, {
                .classId = *m_classId,
                .teacherId = teacherId
            });
        }
        catch (...)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The co-teacher assignment could not be saved.",
                .recoverable = true
            });
        }
    }();
    if (!saved)
    {
        const QString message = errorText(saved.error());
        if (error)
        {
            *error = message;
        }
        setCoTeacherStatus(message, true);
        updateDirtyState();
        return false;
    }
    m_savedCoTeacherId = teacherId;
    m_coTeacherDirty = false;
    readCoTeacher();
    readClassDetails();
    if (m_coTeacherSnapshot && m_coTeacherSnapshot->classFields)
    {
        const auto savedAssignment =
            m_coTeacherSnapshot->classFields.value().selectedTeacherId;
        if (savedAssignment != teacherId)
        {
            setCoTeacherStatus(translated(
                "The saved Co-Teacher assignment could not be confirmed."), true);
        }
    }
    updateDirtyState();
    return true;
}

bool SelectedClassEditor::saveNotes(QString* error)
{
    if (!m_notesDirty)
    {
        return true;
    }
    if (!m_classId || !m_databasePath || !m_actions.saveNotes || !m_notesLoaded)
    {
        const QString message = translated("Class notes cannot be saved right now.");
        if (error)
        {
            *error = message;
        }
        setNotesStatus(message, true, false);
        return false;
    }

    const Application::ClassNotesSaveRequest request{
        .classId = *m_classId,
        .notes = m_notesEdit->toPlainText().trimmed().toStdU16String(),
        .timeFillerActivities = m_timeFillerActivitiesEdit->toPlainText()
            .trimmed().toStdU16String()
    };
    const auto requestValidation = request.validate();
    if (!requestValidation)
    {
        const QString message = errorText(requestValidation.error());
        if (error)
        {
            *error = message;
        }
        setNotesStatus(message, true, false);
        return false;
    }

    Application::ClassNotesSaveResult saved = [&] {
        try
        {
            return m_actions.saveNotes(*m_databasePath, request);
        }
        catch (...)
        {
            return Application::ClassNotesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Class notes could not be saved.",
                .recoverable = true
            });
        }
    }();
    if (!saved)
    {
        const QString message = errorText(saved.error());
        if (error)
        {
            *error = message;
        }
        setNotesStatus(message, true, false);
        updateDirtyState();
        return false;
    }

    m_notesAutosave->stop();
    m_savedNotes = request.notes.empty()
        ? QString{}
        : QString::fromStdU16String(request.notes);
    m_savedTimeFillerActivities = request.timeFillerActivities.empty()
        ? QString{}
        : QString::fromStdU16String(request.timeFillerActivities);
    m_notesDirty = false;
    setNotesStatus({}, false);
    updateDirtyState();
    return true;
}

void SelectedClassEditor::populateSchedule(
    QTableWidget* table,
    const std::vector<Application::ClassDetailsPageScheduleRow>& rows
    )
{
    const QSignalBlocker blocker(table);
    table->setRowCount(0);
    for (const auto& row : rows)
    {
        const int index = table->rowCount();
        table->insertRow(index);
        table->setItem(index, 0, new QTableWidgetItem(text(row.day)));
        table->setItem(index, 1, new QTableWidgetItem(text(row.startTime)));
        table->setItem(index, 2, new QTableWidgetItem(text(row.endTime)));
    }
}

std::vector<Application::ClassDetailsScheduleValidationRow>
SelectedClassEditor::scheduleRows(const QTableWidget* table) const
{
    std::vector<Application::ClassDetailsScheduleValidationRow> rows;
    rows.reserve(static_cast<std::size_t>(table->rowCount()));
    for (int row = 0; row < table->rowCount(); ++row)
    {
        const auto cell = [table, row](const int column)
        {
            const QTableWidgetItem* const item = table->item(row, column);
            return item ? item->text().toStdU16String() : std::u16string{};
        };
        rows.push_back({cell(0), cell(1), cell(2)});
    }
    return rows;
}

void SelectedClassEditor::appendScheduleRow(QTableWidget* table)
{
    if (table->rowCount()
        >= static_cast<int>(Application::kClassesNavigationMaximumScheduleRows))
    {
        return;
    }
    const int row = table->rowCount();
    const QSignalBlocker blocker(table);
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(QStringLiteral("Monday")));
    table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("4:00 PM")));
    table->setItem(row, 2, new QTableWidgetItem(QStringLiteral("4:55 PM")));
    table->setCurrentCell(row, 0);
    (table == m_regularSchedule ? m_regularScheduleAdd : m_intensiveScheduleAdd)
        ->setEnabled(table->rowCount()
            < static_cast<int>(Application::kClassesNavigationMaximumScheduleRows));
    m_detailsDirty = true;
    updateDirtyState();
    scheduleDetailsSave();
}

void SelectedClassEditor::removeSelectedScheduleRows(QTableWidget* table)
{
    const int row = table->currentRow();
    if (row < 0)
    {
        return;
    }
    table->removeRow(row);
    (table == m_regularSchedule ? m_regularScheduleAdd : m_intensiveScheduleAdd)
        ->setEnabled(true);
    m_detailsDirty = true;
    updateDirtyState();
    scheduleDetailsSave();
}

UnsavedChangesChoice SelectedClassEditor::promptUnsavedChanges(
    const QString& reason
    ) const
{
    if (m_actions.resolveUnsavedChanges)
    {
        return m_actions.resolveUnsavedChanges(reason);
    }
    QMessageBox prompt(QMessageBox::Warning, translated("Unsaved Changes"),
        translated("Save changes before %1?").arg(reason), QMessageBox::NoButton,
        const_cast<SelectedClassEditor*>(this));
    QPushButton* const save = prompt.addButton(translated("Save"), QMessageBox::AcceptRole);
    QPushButton* const discard = prompt.addButton(translated("Discard"), QMessageBox::DestructiveRole);
    QPushButton* const cancel = prompt.addButton(translated("Cancel"), QMessageBox::RejectRole);
    prompt.setDefaultButton(save);
    prompt.exec();
    if (prompt.clickedButton() == save)
    {
        return UnsavedChangesChoice::Save;
    }
    if (prompt.clickedButton() == discard)
    {
        return UnsavedChangesChoice::Discard;
    }
    Q_UNUSED(cancel);
    return UnsavedChangesChoice::Cancel;
}

void SelectedClassEditor::retranslateUi()
{
    m_emptyState->setText(translated("Select a class to edit its details."));
    if (m_sections)
    {
        m_sections->setTabText(0, translated("Class Details and Times"));
        m_sections->setTabText(1, translated("Co-Teacher"));
        m_sections->setTabText(2, translated("Notes"));
        m_sections->setTabText(3, translated("Analytics"));
        m_sections->setTabText(4,
            QCoreApplication::translate("NextRosterEditor", "Roster"));
    }
    if (m_chooseColor)
    {
        m_chooseColor->setText(translated("Choose Color"));
        m_saveDetailsButton->setText(translated("Save Details"));
        m_retryDetailsButton->setText(translated("Retry"));
        m_saveCoTeacherButton->setText(translated("Save Changes"));
        m_retryCoTeacherButton->setText(translated("Retry"));
        m_coTeacherLocale->setItemText(0, translated("English"));
        m_coTeacherLocale->setItemText(1, translated("Korean"));
        m_saveNotesButton->setText(translated("Save Changes"));
        m_retryNotesButton->setText(translated("Retry"));
        m_notesTitleLabel->setText(translated("Class Notes"));
        m_notesLabel->setText(translated("Notes"));
        m_timeFillerActivitiesLabel->setText(
            translated("Time Filler Activities"));
        m_notesSubtitleLabel->setText(m_notesSubtitle);
        m_notesEdit->setPlaceholderText(translated("Notes"));
        m_timeFillerActivitiesEdit->setPlaceholderText(
            translated("Time Filler Activities"));
    }
    if (m_analyticsEvaluation)
    {
        m_analyticsEvaluation->setItemText(0, translated("All"));
        m_analyticsEvaluation->setItemText(1, translated("Winter"));
        m_analyticsEvaluation->setItemText(2, translated("Speech Contest"));
        m_analyticsEvaluation->setItemText(3, translated("Summer"));
        m_analyticsEvaluation->setItemText(4, translated("Fall"));
        m_retryAnalyticsButton->setText(translated("Retry"));
        m_analyticsEmptyState->setText(translated(
            "No analytics data is available for this class."));
        if (QLabel* const heading = m_analyticsTab->findChild<QLabel*>(
                QStringLiteral("classEditorAnalyticsHeading")))
        {
            heading->setText(translated("Class Analytics"));
        }
        if (QLabel* const evaluationLabel = m_analyticsTab->findChild<QLabel*>(
                QStringLiteral("classEditorAnalyticsEvaluationLabel")))
        {
            evaluationLabel->setText(translated("Evaluation"));
        }
        m_analyticsRankingModel->setHeaderLabels({
            translated("Rank"), translated("English"), translated("Korean"),
            translated("Average"), translated("Grammar"),
            translated("Pronunciation"), translated("Fluency"),
            translated("Manner"), translated("Content"),
            translated("Overall Effort")
        });
        if (m_analyticsSnapshot)
        {
            renderAnalytics();
        }
    }
}

void SelectedClassEditor::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

} // namespace ClassMngr::Next::Ui
