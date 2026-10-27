#include "next/ui/shared_staff_directory_page.h"

#include "next/domain/operation_result.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QStyledItemDelegate>

#include <algorithm>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int MinimumRowHeight = 42;
constexpr int MinimumHeaderHeight = 38;
constexpr int EditorVerticalMargin = 8;
constexpr int HeaderVerticalPadding = 16;

const QStringList NativeTeacherPositions{
    QStringLiteral("Co-ordinator"),
    QStringLiteral("Team Leader"),
    QStringLiteral("M3 Song's"),
    QStringLiteral("M2 Song's"),
    QStringLiteral("M1 Song's"),
    QStringLiteral("E6 Song's"),
    QStringLiteral("E5 Athena"),
    QStringLiteral("NET")
};

QString text(const char* value)
{
    return QCoreApplication::translate("StaffDirectoryPage", value);
}

QString errorText(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString nativePositionDisplay(const QString& position)
{
    return position == QStringLiteral("Co-ordinator")
        ? text("Coordinator")
        : position;
}

class NativeTeacherPositionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem&,
        const QModelIndex& index
        ) const override
    {
        if (index.column() != 1)
        {
            QStyleOptionViewItem option;
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
        auto* combo = new QComboBox(parent);
        for (const QString& position : NativeTeacherPositions)
        {
            combo->addItem(nativePositionDisplay(position), position);
        }
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->lineEdit()->setReadOnly(true);
        combo->lineEdit()->setAlignment(Qt::AlignCenter);
        auto* delegate = const_cast<NativeTeacherPositionDelegate*>(this);
        connect(combo, qOverload<int>(&QComboBox::activated), combo,
            [delegate, combo](int) {
                emit delegate->commitData(combo);
                emit delegate->closeEditor(combo);
            });
        return combo;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (!combo)
        {
            QStyledItemDelegate::setEditorData(editor, index);
            return;
        }
        const QString value = index.data(Qt::EditRole).toString();
        int selected = combo->findData(value);
        if (selected < 0 && !value.isEmpty())
        {
            combo->addItem(nativePositionDisplay(value), value);
            selected = combo->count() - 1;
        }
        combo->setCurrentIndex(selected);
    }

    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index
        ) const override
    {
        if (const auto* combo = qobject_cast<QComboBox*>(editor))
        {
            model->setData(index, combo->currentData().toString(), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index
        ) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (index.column() == 1)
        {
            option->text = nativePositionDisplay(
                index.data(Qt::DisplayRole).toString());
        }
        option->displayAlignment = Qt::AlignCenter;
    }
};

QString valueForCell(
    const SharedStaffDirectoryTableModel& model,
    const int row,
    const int column
    )
{
    return model.index(row, column).data(Qt::EditRole).toString().trimmed();
}

} // namespace

SharedStaffDirectoryPage::SharedStaffDirectoryPage(
    const PageContext& context,
    const StaffDirectoryKind kind,
    SharedStaffDirectoryPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_kind(kind)
    , m_actions(std::move(actions))
    , m_saveMode(m_actions.saveMode)
{
    buildUi();
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(750);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this] {
        if (m_dirty && m_saveMode == Application::SaveMode::Automatic)
        {
            static_cast<void>(saveDirectory(false));
        }
    });
}

StaffDirectoryKind SharedStaffDirectoryPage::kind() const noexcept
{
    return m_kind;
}

SharedStaffDirectoryTableModel*
SharedStaffDirectoryPage::directoryModel() const noexcept
{
    return m_model;
}

QTableView* SharedStaffDirectoryPage::tableView() const noexcept
{
    return m_table;
}

void SharedStaffDirectoryPage::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(11, 11, 11, 11);
    rootLayout->setSpacing(0);

    auto* contentWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin);
    layout->setSpacing(UiConstants::Pages::Spacing);

    auto* headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin);
    headerLayout->setSpacing(UiConstants::Pages::HeaderSpacing);

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(UiConstants::Pages::HeaderSpacing);
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("pageTitle"));
    QFont titleFont = font();
    titleFont.setPointSize(UiConstants::Pages::TitleFontSize);
    titleFont.setWeight(QFont::Bold);
    m_titleLabel->setFont(titleFont);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();

    m_keyboardButton = new QPushButton(this);
    m_keyboardButton->setObjectName(QStringLiteral("staffDirectoryKoreanKeyboardButton"));
    m_keyboardButton->setMinimumSize(44, 40);
    m_keyboardButton->setMaximumWidth(52);
    m_onScreenKeyboard = new OnScreenKeyboard(this);
    m_onScreenKeyboard->setTriggerButton(m_keyboardButton);
    titleRow->addWidget(m_keyboardButton);
    headerLayout->addLayout(titleRow);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("pageSubtitle"));
    QFont subtitleFont = font();
    subtitleFont.setPointSize(UiConstants::Pages::SubtitleFontSize);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setWordWrap(true);
    headerLayout->addWidget(m_subtitleLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("staffDirectoryStatus"));
    m_statusLabel->setAccessibleName(text("Save status"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();
    headerLayout->addWidget(m_statusLabel);
    layout->addLayout(headerLayout);
    layout->addSpacing(UiConstants::Pages::HeaderContentSpacing);

    m_table = new QTableView(this);
    m_table->setObjectName(
        m_kind == StaffDirectoryKind::NativeEnglishTeachers
            ? QStringLiteral("nativeEnglishTeachersTable")
            : QStringLiteral("gsTeamTable"));
    m_model = new SharedStaffDirectoryTableModel(m_kind, this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::SelectedClicked);
    m_table->setSortingEnabled(false);
    m_table->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->horizontalHeader()->setSectionsMovable(false);
    m_table->horizontalHeader()->setSectionsClickable(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        m_table->setItemDelegateForColumn(
            1,
            new NativeTeacherPositionDelegate(m_table));
    }
    layout->addWidget(m_table, 1);

    rootLayout->addWidget(contentWidget, 1);
    rootLayout->addStretch();

    auto* bottomBar = new QWidget(this);
    auto* bottom = new QHBoxLayout(bottomBar);
    bottom->setContentsMargins(16, 8, 16, 8);
    bottom->setSpacing(8);
    m_addButton = new TextFitPushButton(this);
    m_deleteButton = new TextFitPushButton(this);
    m_discardButton = new TextFitPushButton(this);
    m_saveButton = new TextFitPushButton(this);
    m_addButton->setObjectName(QStringLiteral("staffDirectoryAddButton"));
    m_deleteButton->setObjectName(QStringLiteral("staffDirectoryDeleteButton"));
    m_discardButton->setObjectName(QStringLiteral("staffDirectoryDiscardButton"));
    m_saveButton->setObjectName(QStringLiteral("staffDirectorySaveButton"));
    bottom->addWidget(m_addButton);
    bottom->addWidget(m_deleteButton);
    bottom->addStretch();
    bottom->addWidget(m_discardButton);
    bottom->addWidget(m_saveButton);
    rootLayout->addWidget(bottomBar);

    connect(m_addButton, &QPushButton::clicked,
        this, &SharedStaffDirectoryPage::addRow);
    connect(m_deleteButton, &QPushButton::clicked,
        this, &SharedStaffDirectoryPage::deleteSelectedRows);
    connect(m_discardButton, &QPushButton::clicked,
        this, [this] { static_cast<void>(discardChanges()); });
    connect(m_saveButton, &QPushButton::clicked,
        this, [this] { static_cast<void>(saveDirectory(true)); });
    connect(m_model, &QAbstractItemModel::dataChanged, this,
        [this] { markDirty(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, [this] { updateActions(); });
    connect(m_keyboardButton, &QPushButton::clicked, this, [this] {
        if (m_onScreenKeyboard)
        {
            m_onScreenKeyboard->showForFocusScope(this);
        }
    });
    retranslateUi();
    updateActions();
}

bool SharedStaffDirectoryPage::refreshFromStore(QString* error)
{
    if (error)
    {
        error->clear();
    }
    if (m_dirty && !m_reloadRequired)
    {
        if (error)
        {
            *error = text("Save or discard directory changes before refreshing.");
        }
        return false;
    }

    m_loading = true;
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        const auto loaded = m_actions.readNativeEnglishTeachers
            ? m_actions.readNativeEnglishTeachers()
            : Application::NativeEnglishTeacherDirectoryReadResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The Native English directory read action is unavailable.",
                .recoverable = true
            });
        if (!loaded)
        {
            m_loading = false;
            m_reloadRequired = true;
            m_dirty = false;
            setHasUnsavedChanges(false);
            const QString detail = errorText(loaded.error().message);
            if (error)
            {
                *error = detail;
            }
            showStatus(detail, true);
            updateActions();
            return false;
        }
        m_model->replaceRows(loaded.value());
    }
    else
    {
        const auto loaded = m_actions.readGsTeam
            ? m_actions.readGsTeam()
            : Application::GsTeamDirectoryReadResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The GS Team directory read action is unavailable.",
                .recoverable = true
            });
        if (!loaded)
        {
            m_loading = false;
            m_reloadRequired = true;
            m_dirty = false;
            setHasUnsavedChanges(false);
            const QString detail = errorText(loaded.error().message);
            if (error)
            {
                *error = detail;
            }
            showStatus(detail, true);
            updateActions();
            return false;
        }
        m_model->replaceRows(loaded.value());
    }

    m_deletedNativeEnglishIds.clear();
    m_deletedGsTeamIds.clear();
    m_dirty = false;
    m_reloadRequired = false;
    m_autosaveFailureStatusActive = false;
    m_autosaveTimer->stop();
    m_loading = false;
    showStatus({}, false);
    setHasUnsavedChanges(false);
    updateActions();
    return true;
}

void SharedStaffDirectoryPage::requireReloadBeforeEditing(
    const QString& reason
    )
{
    m_reloadRequired = true;
    m_autosaveTimer->stop();
    showStatus(
        reason.isEmpty()
            ? text("Reload the directory before editing or saving again.")
            : text("The directory could not be refreshed. Reload before editing or saving again: %1")
                  .arg(reason),
        true
        );
    updateActions();
}

PageOperationResult<void> SharedStaffDirectoryPage::onActivating()
{
    if (hasUnsavedChanges())
    {
        return {};
    }
    QString error;
    if (!refreshFromStore(&error))
    {
        reportWarning(text("Load Directory"), error);
    }
    return {};
}

bool SharedStaffDirectoryPage::hasUnsavedChanges() const noexcept
{
    return m_dirty && !m_reloadRequired;
}

PageOperationResult<void> SharedStaffDirectoryPage::saveChanges()
{
    if (m_reloadRequired)
    {
        return std::unexpected(PageOperationError{
            text("Reload the directory before continuing.")
        });
    }
    if (!m_dirty)
    {
        return {};
    }
    return saveDirectory(true);
}

PageOperationResult<void> SharedStaffDirectoryPage::discardChanges()
{
    m_autosaveTimer->stop();
    const bool wasDirty = m_dirty;
    m_dirty = false;
    setHasUnsavedChanges(false);
    QString error;
    if (!refreshFromStore(&error))
    {
        m_dirty = wasDirty;
        setHasUnsavedChanges(wasDirty);
        updateActions();
        return std::unexpected(PageOperationError{error});
    }
    return {};
}

void SharedStaffDirectoryPage::onSuspending() noexcept
{
    m_autosaveTimer->stop();
}

void SharedStaffDirectoryPage::setSaveMode(const Application::SaveMode mode)
{
    m_saveMode = mode;
    if (mode != Application::SaveMode::Automatic)
    {
        m_autosaveFailureStatusActive = false;
        showStatus({}, false);
    }
    updateActions();
    if (mode == Application::SaveMode::Automatic && m_dirty && !m_reloadRequired)
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

void SharedStaffDirectoryPage::retranslateUi()
{
    m_model->retranslateHeaders();
    const bool native = m_kind == StaffDirectoryKind::NativeEnglishTeachers;
    m_titleLabel->setText(native ? text("Native English Teachers") : text("GS Team"));
    m_subtitleLabel->setText(native
        ? text("View and maintain all Native English Teacher contact information.")
        : text("View and maintain all GS and CS team contact information."));
    m_addButton->setText(text("Add"));
    m_deleteButton->setText(text("Delete"));
    m_discardButton->setText(text("Discard Changes"));
    if (m_keyboardButton)
    {
        m_keyboardButton->setToolTip(text("Open Korean / English on-screen keyboard"));
        m_keyboardButton->setAccessibleName(text("Korean Keyboard"));
    }
    m_statusLabel->setAccessibleName(text("Save status"));
    if (m_autosaveFailureStatusActive)
    {
        m_statusLabel->setText(text(
            "Staff directory changes could not be saved automatically. Your edits are still here."));
    }
    updateActions();
}

void SharedStaffDirectoryPage::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    if (event && (event->type() == QEvent::FontChange
        || event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::StyleChange))
    {
        QLineEdit probe;
        probe.setFont(m_table->font());
        probe.ensurePolished();
        m_table->verticalHeader()->setMinimumSectionSize(std::max(
            MinimumRowHeight,
            probe.sizeHint().height() + EditorVerticalMargin));
        m_table->verticalHeader()->setDefaultSectionSize(std::max(
            MinimumRowHeight,
            probe.sizeHint().height() + EditorVerticalMargin));
        m_table->horizontalHeader()->setFixedHeight(std::max(
            MinimumHeaderHeight,
            m_table->horizontalHeader()->fontMetrics().height()
                + HeaderVerticalPadding));
    }
    Page::changeEvent(event);
}

void SharedStaffDirectoryPage::addRow()
{
    if (m_reloadRequired)
    {
        return;
    }
    const int row = m_model->rowCount();
    if (!m_model->insertRows(row, 1))
    {
        return;
    }
    const QModelIndex firstCell = m_model->index(row, 0);
    m_table->setCurrentIndex(firstCell);
    m_table->edit(firstCell);
    markDirty(false);
}

void SharedStaffDirectoryPage::deleteSelectedRows()
{
    if (m_reloadRequired)
    {
        return;
    }
    const QModelIndexList selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        return;
    }
    bool confirmed = false;
    if (m_actions.confirmDelete)
    {
        confirmed = m_actions.confirmDelete(selected.size());
    }
    else
    {
        QMessageBox dialog(QMessageBox::Warning,
            text("Delete Directory Entries"),
            selected.size() == 1
                ? text("Delete the selected entry?")
                : text("Delete the selected %1 entries.").arg(selected.size()),
            QMessageBox::NoButton,
            this);
        QPushButton* const deleteButton = dialog.addButton(
            text("Delete"), QMessageBox::DestructiveRole);
        QPushButton* const cancelButton = dialog.addButton(
            text("Cancel"), QMessageBox::RejectRole);
        dialog.setDefaultButton(cancelButton);
        dialog.exec();
        confirmed = dialog.clickedButton() == deleteButton;
    }
    if (!confirmed)
    {
        return;
    }

    QList<int> rows;
    rows.reserve(selected.size());
    for (const QModelIndex& index : selected)
    {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : rows)
    {
        if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
        {
            const auto id = m_model->nativeRow(row).id;
            if (id)
            {
                m_deletedNativeEnglishIds.push_back(*id);
            }
        }
        else
        {
            const auto id = m_model->gsRow(row).id;
            if (id)
            {
                m_deletedGsTeamIds.push_back(*id);
            }
        }
        static_cast<void>(m_model->removeRows(row, 1));
    }
    markDirty();
}

void SharedStaffDirectoryPage::markDirty(const bool scheduleAutosave)
{
    if (m_loading || m_reloadRequired)
    {
        return;
    }
    m_dirty = true;
    setHasUnsavedChanges(true);
    updateActions();
    if (scheduleAutosave && m_saveMode == Application::SaveMode::Automatic)
    {
        m_autosaveTimer->start();
    }
}

PageOperationResult<void> SharedStaffDirectoryPage::saveDirectory(
    const bool interactive
    )
{
    if (m_reloadRequired)
    {
        return std::unexpected(PageOperationError{
            text("Reload the directory before continuing.")
        });
    }
    m_autosaveTimer->stop();
    QString error;
    bool saved = false;
    bool validationFailure = false;
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        Application::NativeEnglishTeacherDirectorySaveRequest request;
        request.rows.reserve(static_cast<std::size_t>(m_model->rowCount()));
        for (int index = 0; index < m_model->rowCount(); ++index)
        {
            const NativeEnglishTeacherDirectoryRow& row = m_model->nativeRow(index);
            Application::NativeEnglishTeacherDirectorySaveRow value;
            value.id = row.id;
            value.name = valueForCell(*m_model, index, 0).simplified().toStdU16String();
            value.position = valueForCell(*m_model, index, 1).toStdU16String();
            value.phoneNumber = valueForCell(*m_model, index, 2).toStdU16String();
            value.email = valueForCell(*m_model, index, 3).toStdU16String();
            value.birthday = valueForCell(*m_model, index, 4).toStdU16String();
            value.nationality = valueForCell(*m_model, index, 5).toStdU16String();
            request.rows.push_back(std::move(value));
        }
        request.deletedIds = m_deletedNativeEnglishIds;
        const auto outcome = m_actions.saveNativeEnglishTeachers
            ? m_actions.saveNativeEnglishTeachers(request)
            : Application::NativeEnglishTeacherDirectorySaveOutcome::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The Native English directory save action is unavailable.",
                .recoverable = true
            });
        if (!outcome.validation.isValid())
        {
            validationFailure = true;
            error = validationMessage(static_cast<int>(outcome.validation.issue));
        }
        else if (!outcome.saved)
        {
            error = outcome.error
                ? errorText(outcome.error->message)
                : text("The Native English Teacher directory could not be saved.");
        }
        else
        {
            saved = true;
        }
    }
    else
    {
        Application::GsTeamDirectorySaveRequest request;
        request.rows.reserve(static_cast<std::size_t>(m_model->rowCount()));
        for (int index = 0; index < m_model->rowCount(); ++index)
        {
            const GsTeamDirectoryRow& row = m_model->gsRow(index);
            Application::GsTeamDirectorySaveRow value;
            value.id = row.id;
            value.name = valueForCell(*m_model, index, 0).simplified().toStdU16String();
            value.koreanName = valueForCell(*m_model, index, 1).simplified().toStdU16String();
            value.position = valueForCell(*m_model, index, 2).toStdU16String();
            value.phoneNumber = valueForCell(*m_model, index, 3).toStdU16String();
            value.birthday = valueForCell(*m_model, index, 4).toStdU16String();
            request.rows.push_back(std::move(value));
        }
        request.deletedIds = m_deletedGsTeamIds;
        const auto outcome = m_actions.saveGsTeam
            ? m_actions.saveGsTeam(request)
            : Application::GsTeamDirectorySaveOutcome::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The GS Team directory save action is unavailable.",
                .recoverable = true
            });
        if (!outcome.validation.isValid())
        {
            validationFailure = true;
            error = validationMessage(static_cast<int>(outcome.validation.issue));
        }
        else if (!outcome.saved)
        {
            error = outcome.error
                ? errorText(outcome.error->message)
                : text("The GS Team directory could not be saved.");
        }
        else
        {
            saved = true;
        }
    }

    if (!saved)
    {
        const QString title = text("Save Directory");
        if (interactive)
        {
            reportWarning(title, error);
        }
        else if (!validationFailure)
        {
            m_autosaveFailureStatusActive = true;
            showStatus(text(
                "Staff directory changes could not be saved automatically. Your edits are still here."), true);
        }
        updateActions();
        return std::unexpected(PageOperationError{error});
    }

    m_dirty = false;
    setHasUnsavedChanges(false);
    m_deletedNativeEnglishIds.clear();
    m_deletedGsTeamIds.clear();
    m_autosaveFailureStatusActive = false;
    showStatus({}, false);
    updateActions();
    return reloadAfterSave();
}

PageOperationResult<void> SharedStaffDirectoryPage::reloadAfterSave()
{
    m_reloadRequired = true;
    QString error;
    if (!refreshFromStore(&error))
    {
        m_reloadRequired = true;
        showStatus(text(
            "The directory was saved, but its refresh failed. Reload the directory before continuing."), true);
        updateActions();
        return {};
    }
    return {};
}

void SharedStaffDirectoryPage::updateActions()
{
    if (!m_table)
    {
        return;
    }
    const bool editable = !m_reloadRequired;
    m_table->setEditTriggers(editable
        ? QAbstractItemView::DoubleClicked
            | QAbstractItemView::EditKeyPressed
            | QAbstractItemView::SelectedClicked
        : QAbstractItemView::NoEditTriggers);
    m_addButton->setEnabled(editable);
    m_deleteButton->setEnabled(editable
        && !m_table->selectionModel()->selectedRows().isEmpty());
    const bool manual = m_saveMode == Application::SaveMode::Manual;
    m_discardButton->setVisible(manual);
    m_discardButton->setEnabled(manual && m_dirty && editable);
    m_saveButton->setVisible(manual);
    m_saveButton->setEnabled(manual && m_dirty && editable);
    m_saveButton->setText(m_dirty
        ? text("Save Changes *")
        : text("Save Changes"));
}

void SharedStaffDirectoryPage::reportWarning(
    const QString& title,
    const QString& message
    )
{
    if (m_actions.reportWarning)
    {
        m_actions.reportWarning(title, message);
        return;
    }
    QMessageBox::warning(this, title, message);
}

void SharedStaffDirectoryPage::showStatus(
    const QString& message,
    const bool error
    )
{
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(error
        ? QStringLiteral("color: #b3261e;")
        : QString{});
    m_statusLabel->setVisible(!message.isEmpty());
}

QString SharedStaffDirectoryPage::validationMessage(const int issue) const
{
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        using Issue = Application::NativeEnglishTeacherDirectorySaveIssue;
        switch (static_cast<Issue>(issue))
        {
        case Issue::EmptyName:
        case Issue::DuplicateName:
        case Issue::InvalidBirthday:
            return text("Each Native English Teacher needs a unique name and a valid MM-dd birthday.");
        case Issue::InvalidRowId:
        case Issue::DuplicateRowId:
        case Issue::InvalidDeletedId:
            return text("The Native English Teacher directory contains an invalid entry.");
        case Issue::None:
            break;
        }
    }
    else
    {
        using Issue = Application::GsTeamDirectorySaveIssue;
        switch (static_cast<Issue>(issue))
        {
        case Issue::MissingNames:
        case Issue::DuplicateEnglishName:
        case Issue::DuplicateKoreanName:
        case Issue::InvalidBirthday:
            return text("Each GS Team member needs a unique name or Korean name and a valid MM-dd birthday.");
        case Issue::InvalidRowId:
        case Issue::DuplicateRowId:
        case Issue::InvalidDeletedId:
            return text("The GS Team directory contains an invalid entry.");
        case Issue::None:
            break;
        }
    }
    return text("The directory could not be saved.");
}

} // namespace ClassMngr::Next::Ui
