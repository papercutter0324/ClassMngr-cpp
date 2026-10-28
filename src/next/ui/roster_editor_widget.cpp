#include "next/ui/roster_editor_widget.h"

#include "next/application/roster_save_preparation.h"
#include "next/application/roster_evaluation_column_policy.h"
#include "next/ui/roster_table_model.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QStyledItemDelegate>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{

class RosterCellDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem&,
        const QModelIndex&
        ) const override
    {
        auto* const editor = new QLineEdit(parent);
        editor->setFrame(false);
        return editor;
    }
};

namespace
{

QString text(const char* source)
{
    return QCoreApplication::translate("NextRosterEditor", source);
}

QString operationMessage(const Domain::OperationError& error)
{
    const QByteArray bytes(error.message.data(),
        static_cast<qsizetype>(error.message.size()));
    return QString::fromUtf8(bytes);
}

std::vector<std::u16string> requiredColumns()
{
    return {u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"};
}

bool qtCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QStringView(left.data(), static_cast<qsizetype>(left.size())).compare(
        QStringView(right.data(), static_cast<qsizetype>(right.size())),
        Qt::CaseInsensitive) == 0;
}

double colorLuminance(const QColor& color)
{
    const auto linear = [](const double component) {
        return component <= 0.04045
            ? component / 12.92
            : std::pow((component + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF())
        + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

double colorContrastRatio(const QColor& first, const QColor& second)
{
    const double firstLuminance = colorLuminance(first);
    const double secondLuminance = colorLuminance(second);
    return (std::max(firstLuminance, secondLuminance) + 0.05)
        / (std::min(firstLuminance, secondLuminance) + 0.05);
}

QColor contrastingBackground(QColor background, const QColor& textColor)
{
    constexpr double MinimumTextContrast = 4.5;
    if (!background.isValid() || !textColor.isValid()
        || colorContrastRatio(background, textColor) >= MinimumTextContrast)
    {
        return background;
    }

    float hue = -1.0F;
    float saturation = 0.0F;
    float lightness = 0.0F;
    float alpha = 1.0F;
    background.getHslF(&hue, &saturation, &lightness, &alpha);
    const float direction = colorLuminance(textColor) > colorLuminance(background)
        ? -1.0F : 1.0F;
    for (int step = 1; step <= 100; ++step)
    {
        QColor candidate;
        const float adjustedLightness = std::clamp(
            lightness + direction * static_cast<float>(step) / 100.0F,
            0.0F, 1.0F);
        candidate.setHslF(hue, saturation, adjustedLightness, alpha);
        if (colorContrastRatio(candidate, textColor) >= MinimumTextContrast)
        {
            return candidate;
        }
    }
    return background;
}

QColor blendColors(
    const QColor& first,
    const QColor& second,
    const qreal secondWeight
    )
{
    const qreal firstWeight = 1.0 - secondWeight;
    return QColor::fromRgbF(
        first.redF() * firstWeight + second.redF() * secondWeight,
        first.greenF() * firstWeight + second.greenF() * secondWeight,
        first.blueF() * firstWeight + second.blueF() * secondWeight,
        first.alphaF() * firstWeight + second.alphaF() * secondWeight);
}

Application::RosterSnapshotColumnWidth defaultWidth(const QString& name)
{
    if (name.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0)
    {
        return Application::RosterSnapshotColumnWidth{170};
    }
    if (name.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0)
    {
        return Application::RosterSnapshotColumnWidth{120};
    }
    if (Application::isRosterEvaluationColumnName(name,
            [](const QString& candidate, const std::u16string_view other) {
                return candidate.compare(QString::fromStdU16String(
                    std::u16string(other)), Qt::CaseInsensitive) == 0;
            }))
    {
        return Application::RosterSnapshotColumnWidth{130};
    }
    return Application::RosterSnapshotColumnWidth{100};
}

} // namespace

RosterEditorWidget::RosterEditorWidget(
    RosterEditorActions actions,
    QWidget* parent
    )
    : QWidget(parent)
    , m_actions(std::move(actions))
    , m_model(new RosterTableModel(this))
    , m_delegate(new RosterCellDelegate(this))
    , m_autosaveTimer(new QTimer(this))
{
    buildUi();
    m_model->setDirtyChangedCallback([this](const bool) {
        updateActions();
        if (m_dirtyChanged)
        {
            m_dirtyChanged(hasUnsavedChanges());
        }
        if (!m_loading && m_actions.saveMode == Application::SaveMode::Automatic)
        {
            m_autosaveTimer->start();
        }
    });
}

RosterTableModel* RosterEditorWidget::model() const noexcept
{
    return m_model;
}

QTableView* RosterEditorWidget::tableView() const noexcept
{
    return m_table;
}

bool RosterEditorWidget::hasUnsavedChanges() const noexcept
{
    return m_model->isDirty() || m_widthsDirty || !m_removedCustomColumns.empty();
}

bool RosterEditorWidget::isLoaded() const noexcept
{
    return m_loaded;
}

bool RosterEditorWidget::readFailed() const noexcept
{
    return m_readFailed;
}

void RosterEditorWidget::enterClassSection(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const bool testingClassMode
    )
{
    const bool classChanged = m_databasePath != databasePath || m_classId != classId;
    if (classChanged)
    {
        clear();
        m_databasePath = databasePath;
        m_classId = classId;
    }
    m_testingClassMode = testingClassMode;
    if (!m_loaded)
    {
        static_cast<void>(reload());
    }
    updateActions();
}

void RosterEditorWidget::clear()
{
    m_autosaveTimer->stop();
    m_loading = true;
    releaseKeyboard();
    m_databasePath.reset();
    m_classId.reset();
    m_model->clear();
    m_loaded = false;
    m_readFailed = false;
    m_saveFailed = false;
    m_hasRowsOutsideWindow = false;
    m_widthsDirty = false;
    m_removedCustomColumns.clear();
    m_overflowLabel->clear();
    updateStatus({}, false);
    m_loading = false;
    updateActions();
}

void RosterEditorWidget::finishEditing()
{
    if (m_keyboard)
    {
        m_keyboard->close();
    }
    if (m_table)
    {
        m_table->clearFocus();
    }
}

bool RosterEditorWidget::saveChanges(const bool showValidationFeedback)
{
    m_autosaveTimer->stop();
    if (!m_loaded || m_readFailed || !m_databasePath || !m_classId
        || !hasUnsavedChanges())
    {
        return m_loaded && !m_readFailed;
    }

    m_table->clearFocus();
    const Application::RosterSnapshot draft = m_model->snapshot();
    auto prepared = Application::prepareRosterSaveText(
        draft, false, qtCaseInsensitiveEquals);
    bool allowQuestionableLengths = false;
    if (prepared.hasErrors())
    {
        if (Application::hasOnlyQuestionableKoreanNameLengthErrors(prepared)
            && showValidationFeedback
            && confirmQuestionableKoreanLengths())
        {
            allowQuestionableLengths = true;
            prepared = Application::prepareRosterSaveText(
                draft, true, qtCaseInsensitiveEquals);
        }
        else
        {
            if (showValidationFeedback)
            {
                focusFirstValidationIssue();
            }
            else
            {
                updateStatus(text(
                    "Resolve the highlighted roster cells before saving."), true);
            }
            updateActions();
            return false;
        }
    }

    const Application::RosterSaveRequest request{
        .classId = *m_classId,
        .roster = draft,
        .allowQuestionableKoreanNameLengths = allowQuestionableLengths,
        .removedCustomColumns = m_removedCustomColumns
    };
    const Domain::Result<void> saved = m_actions.saveRoster
        ? m_actions.saveRoster(*m_databasePath, request)
        : Domain::Result<void>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The roster save action is unavailable.",
            .recoverable = true
        });
    if (!saved)
    {
        m_saveFailed = true;
        updateStatus(text("The roster could not be saved. Your draft is still here."),
            true, true);
        updateActions();
        return false;
    }

    m_model->clearDirty();
    m_widthsDirty = false;
    m_removedCustomColumns.clear();
    m_saveFailed = false;
    updateStatus(text("Roster saved."), false);
    updateActions();
    if (m_dirtyChanged)
    {
        m_dirtyChanged(false);
    }
    return true;
}

bool RosterEditorWidget::discardChanges()
{
    m_autosaveTimer->stop();
    if (!m_loaded)
    {
        return !m_readFailed;
    }
    if (!reload())
    {
        return false;
    }
    if (m_dirtyChanged)
    {
        m_dirtyChanged(false);
    }
    return true;
}

void RosterEditorWidget::setDirtyChangedCallback(
    std::function<void(bool)> callback
    )
{
    m_dirtyChanged = std::move(callback);
}

void RosterEditorWidget::buildUi()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    m_overflowLabel = new QLabel(this);
    m_overflowLabel->setObjectName(QStringLiteral("nextRosterOverflowNotice"));
    m_overflowLabel->setWordWrap(true);
    m_overflowLabel->hide();
    root->addWidget(m_overflowLabel);

    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("nextRosterTable"));
    m_table->setModel(m_model);
    m_table->setItemDelegate(m_delegate);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    m_table->setAlternatingRowColors(true);
    updateTablePalette();
    m_table->setWordWrap(false);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->verticalHeader()->setMinimumSectionSize(28);
    m_table->horizontalHeader()->setSectionsMovable(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setMinimumSectionSize(60);
    root->addWidget(m_table, 1);

    auto* const toolbar = new QHBoxLayout();
    m_addColumnButton = new QPushButton(this);
    m_removeColumnButton = new QPushButton(this);
    m_removeRowButton = new QPushButton(this);
    m_moveUpButton = new QPushButton(this);
    m_moveDownButton = new QPushButton(this);
    m_saveButton = new QPushButton(this);
    m_discardButton = new QPushButton(this);
    m_retryButton = new QPushButton(this);
    m_keyboardButton = new QPushButton(QStringLiteral("한 / EN"), this);
    for (const auto [button, name] : std::array{
             std::pair{m_addColumnButton, "nextRosterAddColumn"},
             std::pair{m_removeColumnButton, "nextRosterRemoveColumn"},
             std::pair{m_removeRowButton, "nextRosterRemoveStudent"},
             std::pair{m_moveUpButton, "nextRosterMoveStudentUp"},
             std::pair{m_moveDownButton, "nextRosterMoveStudentDown"},
             std::pair{m_saveButton, "nextRosterSave"},
             std::pair{m_discardButton, "nextRosterDiscard"},
             std::pair{m_retryButton, "nextRosterRetry"},
             std::pair{m_keyboardButton, "nextRosterKoreanKeyboard"}})
    {
        button->setObjectName(QString::fromLatin1(name));
    }
    toolbar->addWidget(m_addColumnButton);
    toolbar->addWidget(m_removeColumnButton);
    toolbar->addWidget(m_removeRowButton);
    toolbar->addWidget(m_moveUpButton);
    toolbar->addWidget(m_moveDownButton);
    toolbar->addWidget(m_keyboardButton);
    toolbar->addStretch();
    toolbar->addWidget(m_retryButton);
    toolbar->addWidget(m_discardButton);
    toolbar->addWidget(m_saveButton);
    root->addLayout(toolbar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("nextRosterStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();
    root->addWidget(m_statusLabel);

    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(750);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this] {
        static_cast<void>(saveChanges(false));
    });
    connect(m_addColumnButton, &QPushButton::clicked, this,
        [this] { addColumn(); });
    connect(m_removeColumnButton, &QPushButton::clicked, this,
        [this] { removeColumn(); });
    connect(m_removeRowButton, &QPushButton::clicked, this,
        [this] { removeStudentRow(); });
    connect(m_moveUpButton, &QPushButton::clicked, this,
        [this] { moveStudentRow(-1); });
    connect(m_moveDownButton, &QPushButton::clicked, this,
        [this] { moveStudentRow(1); });
    connect(m_saveButton, &QPushButton::clicked, this,
        [this] { static_cast<void>(saveChanges(true)); });
    connect(m_discardButton, &QPushButton::clicked, this, [this] {
        if (QMessageBox::warning(this, text("Discard Roster Changes?"),
                text("Discard all unsaved roster changes?"),
                QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel)
            == QMessageBox::Discard)
        {
            static_cast<void>(discardChanges());
        }
    });
    connect(m_retryButton, &QPushButton::clicked, this,
        [this] { retry(); });
    connect(m_keyboardButton, &QPushButton::clicked, this,
        [this] { showKeyboard(); });
    connect(m_model, &QAbstractItemModel::dataChanged, this,
        [this] {
            updateActions();
            if (!m_loading
                && m_actions.saveMode == Application::SaveMode::Automatic
                && hasUnsavedChanges())
            {
                m_autosaveTimer->start();
            }
        });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this] { updateActions(); });
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
        this, [this](const int logicalIndex, const int, const int newSize) {
            if (m_loading || !m_loaded)
            {
                return;
            }
            m_model->setColumnWidth(logicalIndex, newSize);
            m_widthsDirty = true;
            markDirty();
        });
    retranslateUi();
    m_model->clear();
    updateActions();
}

void RosterEditorWidget::showKeyboard()
{
    if (!m_keyboard)
    {
        m_keyboard = new OnScreenKeyboard(this);
        m_keyboard->setTriggerButton(m_keyboardButton);
    }
    m_keyboard->showFor(m_table);
}

void RosterEditorWidget::releaseKeyboard()
{
    if (!m_keyboard)
    {
        return;
    }
    m_keyboard->close();
    m_keyboard->clearTarget();
    delete m_keyboard;
    m_keyboard = nullptr;
}

void RosterEditorWidget::retranslateUi()
{
    m_addColumnButton->setText(text("Add Column"));
    m_removeColumnButton->setText(text("Remove Column"));
    m_removeRowButton->setText(text("Remove Student"));
    m_moveUpButton->setText(text("Move Up"));
    m_moveDownButton->setText(text("Move Down"));
    m_saveButton->setText(text("Save Changes"));
    m_discardButton->setText(text("Discard Changes"));
    m_retryButton->setText(text("Retry"));
    m_keyboardButton->setToolTip(text("Open Korean / English on-screen keyboard"));
    m_keyboardButton->setAccessibleName(text("Korean Keyboard"));
    m_model->retranslate();
}

void RosterEditorWidget::updateTablePalette()
{
    if (!m_table)
    {
        return;
    }

    QPalette tablePalette = QApplication::palette();
    for (const QPalette::ColorGroup group : {
             QPalette::Active, QPalette::Inactive, QPalette::Disabled})
    {
        const QColor textColor = tablePalette.color(group, QPalette::Text);
        const QColor base = contrastingBackground(
            tablePalette.color(group, QPalette::Base), textColor);
        QColor alternate = tablePalette.color(group, QPalette::AlternateBase);
        if (colorContrastRatio(alternate, textColor) < 4.5
            || std::abs(alternate.lightness() - base.lightness()) < 8)
        {
            alternate = blendColors(
                base, tablePalette.color(group, QPalette::Mid), 0.35);
            alternate = contrastingBackground(alternate, textColor);
        }
        tablePalette.setColor(group, QPalette::Base, base);
        tablePalette.setColor(group, QPalette::AlternateBase, alternate);
    }
    m_table->setPalette(tablePalette);
}

void RosterEditorWidget::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        const bool wasLoading = m_loading;
        m_loading = true;
        retranslateUi();
        m_loading = wasLoading;
        updateActions();
    }
    QWidget::changeEvent(event);
    if (event && (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange))
    {
        updateTablePalette();
    }
}

bool RosterEditorWidget::reload()
{
    if (!m_databasePath || !m_classId)
    {
        m_readFailed = true;
        m_loaded = false;
        m_model->setEditable(false);
        updateStatus(text("A roster cannot be loaded while no workspace is open."),
            true, true);
        updateActions();
        return false;
    }

    const Application::RosterWindowReadResult loaded = m_actions.readRoster
        ? m_actions.readRoster(*m_databasePath, *m_classId)
        : Application::RosterWindowReadResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The roster read action is unavailable.",
            .recoverable = true
        });
    if (!loaded)
    {
        m_readFailed = true;
        if (!m_loaded)
        {
            m_model->clear();
            m_model->setEditable(false);
        }
        updateStatus(text("The roster could not be loaded. Editing is disabled."),
            true, true);
        updateActions();
        return false;
    }

    m_loading = true;
    m_model->setRoster(loaded.value().roster);
    m_loaded = true;
    m_readFailed = false;
    m_saveFailed = false;
    m_hasRowsOutsideWindow = loaded.value().hasRowsOutsideWindow;
    m_widthsDirty = false;
    m_removedCustomColumns.clear();
    m_model->setEditable(true);
    const QSignalBlocker headerBlocker(m_table->horizontalHeader());
    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        m_table->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::Interactive);
        int width = m_model->columnWidth(column);
        if (width <= 0)
        {
            width = defaultWidth(m_model->columnName(column)).rawValue();
        }
        m_table->setColumnWidth(column, width);
        m_model->setColumnWidth(column, width);
        m_table->setColumnHidden(column,
            m_testingClassMode && m_model->isEvaluationColumn(column));
    }
    m_table->verticalHeader()->setDefaultSectionSize(50);
    m_overflowLabel->setText(m_hasRowsOutsideWindow
        ? text("This roster has saved rows after row 25. They remain stored at their original row numbers.")
        : QString{});
    m_overflowLabel->setVisible(m_hasRowsOutsideWindow);
    updateStatus({}, false);
    m_loading = false;
    updateActions();
    if (m_dirtyChanged)
    {
        m_dirtyChanged(false);
    }
    return true;
}

void RosterEditorWidget::updateActions()
{
    const bool editable = m_loaded && !m_readFailed;
    const QModelIndex current = m_table->currentIndex();
    const bool hasCurrent = current.isValid();
    const bool questionableOnly = hasQuestionableKoreanLengthErrors();
    const bool dirty = hasUnsavedChanges();
    m_table->setEnabled(editable);
    m_addColumnButton->setEnabled(editable);
    m_removeColumnButton->setEnabled(editable && hasCurrent
        && !m_model->isRequiredColumn(current.column()));
    m_removeRowButton->setEnabled(editable && hasCurrent);
    m_moveUpButton->setEnabled(editable && hasCurrent && current.row() > 0);
    m_moveDownButton->setEnabled(editable && hasCurrent
        && current.row() + 1 < m_model->rowCount());
    m_keyboardButton->setVisible(editable);
    m_keyboardButton->setEnabled(editable);
    m_saveButton->setVisible(m_actions.saveMode == Application::SaveMode::Manual
        || questionableOnly);
    m_saveButton->setEnabled(editable && dirty);
    if (questionableOnly)
    {
        m_saveButton->setEnabled(editable && dirty);
        m_saveButton->setText(text("Review Korean Names and Save"));
    }
    else
    {
        m_saveButton->setText(text("Save Changes"));
    }
    m_discardButton->setVisible(dirty);
    m_discardButton->setEnabled(editable && dirty);
    m_retryButton->setVisible((m_readFailed || m_saveFailed));
    m_retryButton->setEnabled(m_readFailed || m_saveFailed);
}

void RosterEditorWidget::updateStatus(
    const QString& message,
    const bool error,
    const bool retryable
    )
{
    m_lastStatus = message;
    m_statusLabel->setText(message);
    m_statusLabel->setProperty("error", error);
    m_statusLabel->setStyleSheet(error
        ? QStringLiteral("color: #a32121;") : QString{});
    m_statusLabel->setVisible(!message.isEmpty());
    m_retryButton->setVisible(retryable);
}

void RosterEditorWidget::markDirty()
{
    if (m_loading)
    {
        return;
    }
    if (m_dirtyChanged)
    {
        m_dirtyChanged(true);
    }
    updateActions();
    if (m_actions.saveMode == Application::SaveMode::Automatic)
    {
        m_autosaveTimer->start();
    }
}

void RosterEditorWidget::retry()
{
    if (m_readFailed || !m_loaded)
    {
        static_cast<void>(reload());
        return;
    }
    static_cast<void>(saveChanges(false));
}

void RosterEditorWidget::addColumn()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, text("Add Column"), text("Column name:"), QLineEdit::Normal,
        QString{}, &accepted).trimmed();
    if (!accepted)
    {
        return;
    }
    QString rejection;
    if (!m_model->appendCustomColumn(name, &rejection))
    {
        QMessageBox::warning(this, text("Cannot Add Column"), rejection);
        return;
    }
    const int added = m_model->columnCount() - 1;
    m_table->horizontalHeader()->setSectionResizeMode(added, QHeaderView::Interactive);
    m_table->setColumnWidth(added, 100);
    m_model->setColumnWidth(added, 100);
    m_table->setCurrentIndex(m_model->index(0, added));
    m_table->edit(m_model->index(0, added));
    markDirty();
    updateActions();
}

void RosterEditorWidget::removeColumn()
{
    const int column = m_table->currentIndex().column();
    const auto name = activeColumnName();
    if (!name || column < 0 || m_model->isRequiredColumn(column))
    {
        return;
    }
    const auto response = QMessageBox::warning(this, text("Remove Column"),
        text("Remove the \"%1\" column?").arg(*name),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (response != QMessageBox::Yes)
    {
        return;
    }

    if (!m_actions.columnHasTailData || !m_databasePath || !m_classId)
    {
        QMessageBox::warning(this, text("Cannot Remove Column"),
            text("Stored rows after row 25 could not be checked. The column was not removed."));
        return;
    }
    const Domain::Result<bool> tail = m_actions.columnHasTailData(
        *m_databasePath, *m_classId, name->toStdU16String());
    if (!tail)
    {
        QMessageBox::warning(this, text("Cannot Remove Column"),
            text("Stored rows after row 25 could not be checked. The column was not removed."));
        return;
    }
    if (tail.value())
    {
        const auto tailResponse = QMessageBox::warning(this,
            text("Remove Column Data After Row 25?"),
            text("The \"%1\" column has stored data after row 25. Removing it will also delete that data.").arg(*name),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (tailResponse != QMessageBox::Yes)
        {
            return;
        }
    }

    const auto removed = m_model->removeCustomColumn(column);
    if (!removed)
    {
        return;
    }
    m_removedCustomColumns.push_back(removed->toStdU16String());
    markDirty();
    updateActions();
}

void RosterEditorWidget::removeStudentRow()
{
    const int row = m_table->currentIndex().row();
    if (row < 0 || row >= m_model->rowCount())
    {
        return;
    }
    if (QMessageBox::warning(this, text("Remove Student"),
            text("Remove roster row %1 and move later visible rows up?").arg(row + 1),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
    {
        return;
    }
    if (!m_model->removeRowData(row))
    {
        updateStatus(text("Select a populated row before removing a student."), true);
        return;
    }
    m_table->setCurrentIndex(m_model->index(row, 0));
    markDirty();
    updateActions();
}

void RosterEditorWidget::moveStudentRow(const int offset)
{
    const QModelIndex current = m_table->currentIndex();
    const int destination = current.row() + offset;
    if (!current.isValid() || destination < 0 || destination >= m_model->rowCount())
    {
        return;
    }
    const int column = current.column();
    if (!m_model->moveRowData(current.row(), destination))
    {
        updateStatus(text("Select a populated row before moving it."), true);
        return;
    }
    m_table->setCurrentIndex(m_model->index(destination, column));
    markDirty();
    updateActions();
}

void RosterEditorWidget::focusFirstValidationIssue()
{
    if (const auto index = m_model->firstErrorIndex())
    {
        m_table->setCurrentIndex(*index);
        m_table->scrollTo(*index, QAbstractItemView::EnsureVisible);
        m_table->setFocus();
        m_table->edit(*index);
        updateStatus(m_model->data(*index, Qt::ToolTipRole).toString(), true);
        return;
    }
    if (!m_model->validationIssues().empty())
    {
        const auto& issue = m_model->validationIssues().front();
        updateStatus(QCoreApplication::translate("NextRosterEditor",
            "Roster validation failed: %1").arg(
                QString::fromStdU16String(issue.field)), true);
    }
}

bool RosterEditorWidget::hasQuestionableKoreanLengthErrors() const
{
    return Application::hasOnlyQuestionableKoreanNameLengthErrors(
        Application::prepareRosterSaveText(
            m_model->snapshot(), false, qtCaseInsensitiveEquals));
}

bool RosterEditorWidget::confirmQuestionableKoreanLengths() const
{
    return QMessageBox::warning(const_cast<RosterEditorWidget*>(this),
        text("Check Korean Names"),
        text("Some Korean names have an unusual syllable count. Save them anyway?"),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Save;
}

std::optional<QString> RosterEditorWidget::activeColumnName() const
{
    const int column = m_table->currentIndex().column();
    if (column < 0 || column >= m_model->columnCount())
    {
        return std::nullopt;
    }
    return m_model->columnName(column);
}

} // namespace ClassMngr::Next::Ui
