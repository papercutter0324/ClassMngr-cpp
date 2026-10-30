#include "next/ui/roster_print_dialog.h"
#include "next/ui/roster_editor_widget.h"

#include "next/application/roster_save_preparation.h"
#include "next/application/roster_evaluation_column_policy.h"
#include "next/application/roster_row_availability.h"
#include "next/application/roster_transfer_target_eligibility.h"
#include "next/application/speaking_evaluation_roster_score_row_assignments.h"
#include "next/ui/roster_table_model.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QAction>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
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
#include <vector>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr std::array<std::pair<const char*, const char*>, 4>
    RosterEvaluationMappings{{
        {"Winter", "Winter"},
        {"Speech Contest", "Speech Contest"},
        {"Summer", "Summer"},
        {"Fall", "Fall"}
    }};

} // namespace

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

bool hasRosterColumn(
    const Application::RosterSnapshot& roster,
    const QString& name
    )
{
    return std::any_of(roster.columns.begin(), roster.columns.end(),
        [&name](const std::u16string& column) {
            return QString::fromStdU16String(column).compare(
                name, Qt::CaseInsensitive) == 0;
        });
}

class AutosaveTimerPause final
{
public:
    explicit AutosaveTimerPause(QTimer* timer) noexcept
        : m_timer(timer)
        , m_wasActive(timer && timer->isActive())
        , m_remaining(m_wasActive ? timer->remainingTime() : -1)
    {
        if (m_wasActive)
        {
            m_timer->stop();
        }
    }

    AutosaveTimerPause(const AutosaveTimerPause&) = delete;
    AutosaveTimerPause& operator=(const AutosaveTimerPause&) = delete;

    ~AutosaveTimerPause()
    {
        if (m_resumes && m_wasActive && m_timer)
        {
            m_timer->start(std::max(1, m_remaining));
        }
    }

    void commit() noexcept
    {
        m_resumes = false;
        if (m_timer)
        {
            m_timer->stop();
        }
    }

private:
    QTimer* m_timer = nullptr;
    bool m_wasActive = false;
    int m_remaining = -1;
    bool m_resumes = true;
};

class CallbackRosterWindowReadPort final
    : public Application::RosterWindowReadPort
{
public:
    CallbackRosterWindowReadPort(
        QString databasePath,
        std::function<Application::RosterWindowReadResult(
            const QString&, const Domain::ClassId&)> callback)
        : m_databasePath(std::move(databasePath))
        , m_callback(std::move(callback))
    {
    }

    [[nodiscard]] Application::RosterWindowReadResult readRosterWindow(
        const Application::RosterWindowReadQuery& query
        ) const override
    {
        if (!m_callback)
        {
            return Application::RosterWindowReadResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The roster read action is unavailable.",
                .recoverable = true
            });
        }
        return m_callback(m_databasePath, query.classId);
    }

private:
    QString m_databasePath;
    std::function<Application::RosterWindowReadResult(
        const QString&, const Domain::ClassId&)> m_callback;
};

class CallbackRosterTransferSavePort final
    : public Application::RosterRowTransferSavePort
{
public:
    CallbackRosterTransferSavePort(
        QString databasePath,
        std::function<Domain::Result<void>(
            const QString&, const Application::RosterRowTransferSaveRequest&)>
            callback)
        : m_databasePath(std::move(databasePath))
        , m_callback(std::move(callback))
    {
    }

    [[nodiscard]] Domain::Result<void> saveTransfer(
        const Application::RosterRowTransferSaveRequest& request
        ) const override
    {
        if (!m_callback)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The roster transfer save action is unavailable.",
                .recoverable = true
            });
        }
        return m_callback(m_databasePath, request);
    }

private:
    QString m_databasePath;
    std::function<Domain::Result<void>(
        const QString&, const Application::RosterRowTransferSaveRequest&)>
        m_callback;
};

class RosterEditorTransferConfirmationPort final
    : public Application::RosterRowTransferConfirmationPort
{
public:
    explicit RosterEditorTransferConfirmationPort(
        RosterEditorWidget* parent) noexcept
        : m_parent(parent)
    {
    }

    [[nodiscard]] bool confirmTargetQuestionableKoreanNameLengths(
        const Application::RosterSnapshot& preparedTarget,
        const std::vector<Application::RosterSaveIssue>& issues
        ) const override
    {
        QStringList names;
        for (const Application::RosterSaveIssue& issue : issues)
        {
            const int row = issue.row.rawValue();
            const int column = issue.column.rawValue();
            if (row < 0 || column < 0
                || static_cast<std::size_t>(row) >= preparedTarget.rows.size()
                || static_cast<std::size_t>(column)
                    >= preparedTarget.rows[static_cast<std::size_t>(row)].size())
            {
                continue;
            }
            names.append(QCoreApplication::translate("NextRosterEditor",
                    "Row %1: %2")
                .arg(row + 1)
                .arg(QString::fromStdU16String(
                    preparedTarget.rows[static_cast<std::size_t>(row)]
                        [static_cast<std::size_t>(column)])));
        }
        if (names.isEmpty())
        {
            return false;
        }
        return QMessageBox::warning(m_parent,
                QCoreApplication::translate("NextRosterEditor",
                    "Verify Korean Name Lengths"),
                QCoreApplication::translate("NextRosterEditor",
                    "These Korean names have an unusual syllable count:\n%1\n\nSave them anyway?")
                    .arg(names.join(QLatin1Char('\n'))),
                QMessageBox::Save | QMessageBox::Cancel,
                QMessageBox::Cancel) == QMessageBox::Save;
    }

private:
    RosterEditorWidget* m_parent = nullptr;
};

QString transferFailureMessage(const Application::RosterRowTransferResult& result)
{
    if (const auto* error = std::get_if<Application::RosterRowRemovalError>(&result))
    {
        return error->code == Application::RosterRowRemovalErrorCode::InvalidRowIndex
            ? QCoreApplication::translate("NextRosterEditor",
                "Select a student row to transfer.")
            : QCoreApplication::translate("NextRosterEditor",
                "Selected row is already empty.");
    }
    if (const auto* error =
            std::get_if<Application::RosterRowTransferPreparationError>(&result))
    {
        switch (error->rejection)
        {
        case Application::RosterRowTransferPreparationRejection::SourceRowHasNoData:
            return QCoreApplication::translate("NextRosterEditor",
                "Selected row is empty.");
        case Application::RosterRowTransferPreparationRejection::TargetRosterIsFull:
            return QCoreApplication::translate("NextRosterEditor",
                "Target roster is full.");
        case Application::RosterRowTransferPreparationRejection::DuplicateStudentNamePair:
            return QCoreApplication::translate("NextRosterEditor",
                "Target roster already contains this student.");
        }
    }
    if (const auto* error = std::get_if<Domain::OperationError>(&result))
    {
        const QByteArray bytes(error->message.data(),
            static_cast<qsizetype>(error->message.size()));
        return QString::fromUtf8(bytes);
    }
    return {};
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
    m_hasSourceEnglishColumn = false;
    m_hasSourceKoreanColumn = false;
    m_hasSourceEvaluationColumns.fill(false);
    m_widthsDirty = false;
    m_removedCustomColumns.clear();
    m_overflowLabel->clear();
    updateStatus({}, false);
    m_loading = false;
    updateActions();
}

void RosterEditorWidget::setSaveMode(const Application::SaveMode mode)
{
    m_actions.saveMode = mode;
    if (mode == Application::SaveMode::Automatic && hasUnsavedChanges() && m_loaded)
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
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
        if (auto* focused = QApplication::focusWidget(); focused && m_table->isAncestorOf(focused)) focused->clearFocus();
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
    m_hasSourceEnglishColumn = true;
    m_hasSourceKoreanColumn = true;
    m_hasSourceEvaluationColumns.fill(true);
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
    m_importScoresButton = new QPushButton(this);
    m_transferButton = new QPushButton(this);
    m_moveUpButton = new QPushButton(this);
    m_moveDownButton = new QPushButton(this);
    m_outputButton = new QPushButton(this);
    m_outputButton->setObjectName(QStringLiteral("nextRosterOutput"));
    connect(m_outputButton, &QPushButton::clicked, this, [this] { openRosterOutput(); });
    m_saveButton = new QPushButton(this);
    m_discardButton = new QPushButton(this);
    m_retryButton = new QPushButton(this);
    m_keyboardButton = new QPushButton(QStringLiteral("한 / EN"), this);
    for (const auto [button, name] : std::array{
             std::pair{m_addColumnButton, "nextRosterAddColumn"},
             std::pair{m_removeColumnButton, "nextRosterRemoveColumn"},
             std::pair{m_removeRowButton, "nextRosterRemoveStudent"},
             std::pair{m_importScoresButton, "nextRosterImportScores"},
             std::pair{m_transferButton, "nextRosterTransferStudent"},
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
    toolbar->addWidget(m_importScoresButton);
    toolbar->addWidget(m_transferButton);
    toolbar->addWidget(m_moveUpButton);
    toolbar->addWidget(m_moveDownButton);
    toolbar->addWidget(m_keyboardButton);
    auto* outputToolbar = new QHBoxLayout();
    outputToolbar->addStretch();
    outputToolbar->addWidget(m_outputButton);
    root->insertLayout(0, outputToolbar);
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
    m_autosaveTimer->setObjectName(QStringLiteral("nextRosterAutosaveTimer"));
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
    connect(m_importScoresButton, &QPushButton::clicked, this,
        [this] { importScores(); });
    connect(m_transferButton, &QPushButton::clicked, this,
        [this] { showTransferMenu(); });
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
    m_outputButton->setText(text("Templates / Print"));
    m_addColumnButton->setText(text("Add Column"));
    m_removeColumnButton->setText(text("Remove Column"));
    m_removeRowButton->setText(text("Remove Student"));
    m_importScoresButton->setText(text("Import Scores"));
    m_importScoresButton->setToolTip(
        text("Import final grades from speaking evaluations."));
    m_transferButton->setText(text("Transfer Student"));
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
    m_hasSourceEnglishColumn = hasRosterColumn(
        loaded.value().roster, QStringLiteral("English"));
    m_hasSourceKoreanColumn = hasRosterColumn(
        loaded.value().roster, QStringLiteral("Korean"));
    for (std::size_t index = 0;
         index < RosterEvaluationMappings.size(); ++index)
    {
        m_hasSourceEvaluationColumns[index] = hasRosterColumn(
            loaded.value().roster,
            QString::fromLatin1(RosterEvaluationMappings[index].second));
    }
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
    m_outputButton->setEnabled(editable && static_cast<bool>(m_actions.output.readChoices)
        && static_cast<bool>(m_actions.output.savePdf));
    m_table->setEnabled(editable);
    m_addColumnButton->setEnabled(editable);
    m_removeColumnButton->setEnabled(editable && hasCurrent
        && !m_model->isRequiredColumn(current.column()));
    m_removeRowButton->setEnabled(editable && hasCurrent);
    m_importScoresButton->setVisible(!m_testingClassMode);
    m_importScoresButton->setEnabled(editable);
    m_transferButton->setVisible(!m_testingClassMode);
    bool currentRowHasData = false;
    if (hasCurrent && current.row() >= 0
        && current.row() < m_model->rowCount())
    {
        const Application::RosterSnapshot snapshot = m_model->snapshot();
        if (static_cast<std::size_t>(current.row()) < snapshot.rows.size())
        {
            currentRowHasData = Application::rosterRowHasData(
                snapshot.rows[static_cast<std::size_t>(current.row())]);
        }
    }
    m_transferButton->setEnabled(editable && currentRowHasData
        && static_cast<bool>(m_actions.readTransferTargets)
        && static_cast<bool>(m_actions.saveRosterTransfer));
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

void RosterEditorWidget::importScores()
{
    if (!m_loaded || m_readFailed || m_testingClassMode
        || !m_databasePath || !m_classId)
    {
        return;
    }

    const auto findModelColumn = [this](const QString& name) {
        for (int column = 0; column < m_model->columnCount(); ++column)
        {
            if (m_model->columnName(column).compare(
                    name, Qt::CaseInsensitive) == 0)
            {
                return column;
            }
        }
        return -1;
    };
    const int englishColumn = findModelColumn(QStringLiteral("English"));
    const int koreanColumn = findModelColumn(QStringLiteral("Korean"));
    if (!m_hasSourceEnglishColumn || !m_hasSourceKoreanColumn
        || englishColumn < 0 || koreanColumn < 0)
    {
        updateStatus(text(
            "Roster must contain 'English' and 'Korean' columns."), true);
        return;
    }

    std::vector<std::u16string> evaluationNames;
    std::vector<int> scoreColumns;
    for (std::size_t index = 0;
         index < RosterEvaluationMappings.size(); ++index)
    {
        if (!m_hasSourceEvaluationColumns[index])
        {
            continue;
        }
        const auto& [evaluationName, columnName] =
            RosterEvaluationMappings[index];
        const int column = findModelColumn(QString::fromLatin1(columnName));
        if (column < 0)
        {
            continue;
        }
        evaluationNames.push_back(QString::fromLatin1(evaluationName).toStdU16String());
        scoreColumns.push_back(column);
    }

    const auto imported = m_actions.readEvaluationScores
        ? m_actions.readEvaluationScores(
            *m_databasePath, *m_classId, evaluationNames)
        : Application::SpeakingEvaluationRosterScoreImportBatchResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The speaking evaluation read action is unavailable.",
            .recoverable = true
        });
    if (!imported)
    {
        const QString detail = operationMessage(imported.error());
        updateStatus(text(
            "Saved speaking-evaluation results could not be loaded. The roster was not changed.")
                + (detail.isEmpty() ? QString{} : QStringLiteral("\n") + detail),
            true);
        return;
    }
    if (imported.value().size() != evaluationNames.size())
    {
        updateStatus(text(
            "Saved speaking-evaluation results could not be loaded. The roster was not changed."),
            true);
        return;
    }

    struct PlannedGrade final
    {
        int row = -1;
        int column = -1;
        std::u16string grade;
    };
    std::vector<PlannedGrade> planned;
    for (std::size_t evaluationIndex = 0;
         evaluationIndex < evaluationNames.size(); ++evaluationIndex)
    {
        const auto& scoreResult = imported.value()[evaluationIndex];
        if (!scoreResult)
        {
            const QString detail = operationMessage(scoreResult.error());
            updateStatus(text(
                "Saved speaking-evaluation results could not be loaded. The roster was not changed.")
                    + (detail.isEmpty() ? QString{} : QStringLiteral("\n") + detail),
                true);
            return;
        }

        std::vector<Application::SpeakingEvaluationRosterScoreRow> rosterRows;
        rosterRows.reserve(static_cast<std::size_t>(m_model->rowCount()));
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            rosterRows.push_back({
                .names = {
                    m_model->index(row, englishColumn)
                        .data(Qt::EditRole).toString().toStdU16String(),
                    m_model->index(row, koreanColumn)
                        .data(Qt::EditRole).toString().toStdU16String()
                },
                .currentGrade = m_model->index(
                    row, scoreColumns[evaluationIndex])
                    .data(Qt::EditRole).toString().toStdU16String()
            });
        }
        const auto assignments =
            Application::speakingEvaluationRosterScoreRowAssignments(
                rosterRows, scoreResult.value());
        for (const auto& assignment : assignments)
        {
            planned.push_back({
                .row = static_cast<int>(assignment.rosterRowIndex.rawValue()),
                .column = scoreColumns[evaluationIndex],
                .grade = assignment.finalGrade
            });
        }
    }

    int changeCount = 0;
    for (const PlannedGrade& assignment : planned)
    {
        const QString grade = QString::fromUtf16(
            assignment.grade.data(),
            static_cast<qsizetype>(assignment.grade.size()));
        if (m_model->setImportedEvaluationGrade(
                assignment.row, assignment.column, grade))
        {
            ++changeCount;
        }
    }
    if (changeCount == 0)
    {
        updateStatus(text("Scores are already up to date."), false);
        return;
    }
    updateStatus(text("Imported %1 score cells.").arg(changeCount), false);
    updateActions();
}

void RosterEditorWidget::showTransferMenu()
{
    if (!m_loaded || m_readFailed || m_testingClassMode
        || !m_databasePath || !m_classId)
    {
        return;
    }
    const QModelIndex current = m_table->currentIndex();
    if (!current.isValid())
    {
        updateStatus(text("Select a student row to transfer."), true);
        return;
    }

    AutosaveTimerPause pendingAutosave(m_autosaveTimer);
    const auto choices = m_actions.readTransferTargets
        ? m_actions.readTransferTargets(*m_databasePath, *m_classId)
        : Application::RosterTransferTargetChoicesResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The transfer target read action is unavailable.",
            .recoverable = true
        });
    if (!choices)
    {
        const QString detail = operationMessage(choices.error());
        updateStatus(text("Transfer classes could not be loaded.")
                + (detail.isEmpty() ? QString{} : QStringLiteral("\n") + detail),
            true);
        return;
    }

    struct MenuTarget final
    {
        Application::RosterTransferTargetChoice choice;
        QString label;
    };
    std::vector<MenuTarget> targets;
    for (const auto& candidate : choices.value().candidates)
    {
        if (Application::isRosterTransferTargetEligible(
                *m_classId, choices.value().sourceGrade, candidate.classId,
                candidate.displayMetadata.grade))
        {
            const auto formatted =
                Application::formatRosterTransferTargetDisplayName(
                    candidate.displayMetadata,
                    text("Unknown Class").toStdU16String(),
                    text("No Teacher").toStdU16String());
            QString label = formatted
                ? QString::fromStdU16String(*formatted)
                : QString::fromStdU16String(candidate.className).trimmed();
            if (label.isEmpty())
            {
                label = text("Class %1").arg(QString::fromStdString(
                    candidate.classId.value()));
            }
            targets.push_back({candidate, std::move(label)});
        }
    }
    std::sort(targets.begin(), targets.end(), [](const auto& left, const auto& right) {
        const int comparison = QString::localeAwareCompare(
            left.label, right.label);
        return comparison != 0
            ? comparison < 0
            : left.choice.classId.value() < right.choice.classId.value();
    });

    QMenu menu(this);
    std::vector<std::pair<QAction*, Domain::ClassId>> transferActions;
    if (targets.empty())
    {
        QAction* const emptyAction = menu.addAction(text("No same-grade classes"));
        emptyAction->setEnabled(false);
    }
    else
    {
        for (const MenuTarget& target : targets)
        {
            QAction* const action = menu.addAction(target.choice.full
                ? text("%1 (full)").arg(target.label) : target.label);
            action->setData(QString::fromStdString(
                target.choice.classId.value()));
            action->setEnabled(!target.choice.full);
            if (target.choice.full)
            {
                action->setToolTip(text("Target roster is full."));
            }
            else
            {
                transferActions.emplace_back(action, target.choice.classId);
            }
        }
    }

    QAction* const selected = menu.exec(m_transferButton->mapToGlobal(
        QPoint(0, m_transferButton->height())));
    for (const auto& [action, targetClassId] : transferActions)
    {
        if (selected == action
            && transferStudentToClass(current.row(), targetClassId))
        {
            pendingAutosave.commit();
            break;
        }
    }
}

bool RosterEditorWidget::transferStudentToClass(
    const int row,
    const Domain::ClassId& targetClassId
    )
{
    if (!m_loaded || m_readFailed || m_testingClassMode
        || !m_databasePath || !m_classId
        || !Application::shouldReadRosterTransferTargetClassInfo(
            *m_classId, targetClassId))
    {
        return false;
    }

    AutosaveTimerPause pendingAutosave(m_autosaveTimer);
    bool sourceLengthsConfirmed = false;
    if (hasQuestionableKoreanLengthErrors())
    {
        if (!confirmQuestionableKoreanLengths())
        {
            return false;
        }
        sourceLengthsConfirmed = true;
    }

    const Application::RosterRowTransferRequest request{
        .sourceClassId = *m_classId,
        .targetClassId = targetClassId,
        .sourceRoster = m_model->snapshot(),
        .sourceRow = Application::RosterRowTransferSourceIndex{row},
        .baseColumnNames = requiredColumns(),
        .sourceQuestionableKoreanNameLengthsConfirmed = sourceLengthsConfirmed,
        .sourceRemovedCustomColumns = m_removedCustomColumns
    };
    CallbackRosterWindowReadPort windowReadPort(*m_databasePath,
        m_actions.readRoster);
    const Application::RosterWindowAsRosterReadPort readPort(windowReadPort);
    CallbackRosterTransferSavePort savePort(*m_databasePath,
        m_actions.saveRosterTransfer);
    const RosterEditorTransferConfirmationPort confirmationPort(this);
    const auto transferred = Application::RosterRowTransferUseCase::execute(
        request, readPort, savePort, qtCaseInsensitiveEquals,
        &confirmationPort);
    if (!std::holds_alternative<Application::RosterRowTransferSuccess>(transferred))
    {
        if (const auto* error = std::get_if<Domain::OperationError>(&transferred);
            error && error->code == Domain::ErrorCode::Canceled)
        {
            return false;
        }
        const QString reason = transferFailureMessage(transferred);
        updateStatus(text("The student could not be transferred.")
                + (reason.isEmpty() ? QString{} : QStringLiteral("\n") + reason),
            true);
        return false;
    }

    pendingAutosave.commit();
    m_autosaveTimer->stop();
    m_loading = true;
    const bool removed = m_model->removeRowData(row);
    m_loading = false;
    if (!removed)
    {
        static_cast<void>(reload());
        updateStatus(text("Student transferred."), false);
        return true;
    }
    m_model->clearDirty();
    m_hasSourceEnglishColumn = true;
    m_hasSourceKoreanColumn = true;
    m_hasSourceEvaluationColumns.fill(true);
    m_widthsDirty = false;
    m_removedCustomColumns.clear();
    m_saveFailed = false;
    updateStatus(text("Student transferred."), false);
    const int nextRow = std::min(row, m_model->rowCount() - 1);
    if (nextRow >= 0 && m_model->columnCount() > 0)
    {
        m_table->setCurrentIndex(m_model->index(nextRow, 0));
    }
    updateActions();
    if (m_dirtyChanged)
    {
        m_dirtyChanged(false);
    }
    return true;
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

void ClassMngr::Next::Ui::RosterEditorWidget::openRosterOutput()
{
    finishEditing();
    if (!saveChanges(true) || !m_databasePath || !m_classId || !m_actions.output.readChoices) return;
    const auto choices = m_actions.output.readChoices(*m_databasePath, *m_classId, m_testingClassMode);
    if (!choices) { updateStatus(choices.error(), true); return; }
    RosterPrintDialog dialog(*m_databasePath, *m_classId, m_testingClassMode, *choices, m_actions.output, this);
    dialog.exec();
}
