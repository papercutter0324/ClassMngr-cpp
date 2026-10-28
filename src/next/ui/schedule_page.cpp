#include "next/ui/schedule_page.h"
#include "next/ui/schedule_intensive_cell_layout.h"
#include "next/ui/schedule_import_workflow_dialog.h"
#include "next/ui/schedule_output_dialog.h"
#include "next/ui/schedule_print_projection.h"
#include "next/ui/schedule_testing_assignment_dialog.h"

#include "next/application/schedule_slot_state_save_use_case.h"

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QFont>
#include <QStringList>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPolygon>
#include <QPushButton>
#include <QTime>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <array>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] QString pageText(const char* source)
{
    return QCoreApplication::translate("SchedulePage", source);
}

[[nodiscard]] QString scheduleText(const char* source)
{
    return QCoreApplication::translate("ScheduleWidget", source);
}

[[nodiscard]] QColor validColor(const QString& value, const QColor& fallback)
{
    const QColor color(value);
    return color.isValid() ? color : fallback;
}

[[nodiscard]] QString entryTeacherLine(
    const SchedulePageEntry& entry,
    const bool english
    )
{
    return QStringLiteral("%1 %2")
        .arg(scheduleEntryTeacherName(entry, english), entry.room.trimmed())
        .simplified();
}

class ScheduleCellDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        const auto* model = dynamic_cast<const SchedulePageModel*>(index.model());
        const bool dark = QApplication::palette()
            .color(QPalette::Window).lightness() < 128;
        const QRectF card = QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0);
        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing, true);

        if (index.column() == 0)
        {
            painter->setPen(QPen(dark ? QColor(QStringLiteral("#4B5056"))
                                      : QColor(QStringLiteral("#C5C7C3")), 1));
            painter->setBrush(dark ? QColor(QStringLiteral("#30343A"))
                                   : QColor(QStringLiteral("#F2F1ED")));
            painter->drawRoundedRect(card, 4, 4);
            QFont timeFont = option.font;
            timeFont.setPointSize(11);
            timeFont.setWeight(QFont::Medium);
            painter->setFont(timeFont);
            painter->setPen(option.palette.color(QPalette::Text));
            painter->drawText(card.adjusted(3, 2, -3, -2),
                              Qt::AlignCenter | Qt::TextWordWrap,
                              index.data(Qt::DisplayRole).toString());
            painter->restore();
            return;
        }

        const SchedulePageCell* const cell = model
            ? model->cellAt(index.row(), index.column()) : nullptr;
        if (!cell)
        {
            painter->restore();
            return;
        }

        const auto drawMarker = [&] (const QColor& color) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
            const int right = option.rect.right();
            const int top = option.rect.top();
            painter->drawPolygon(QPolygon{
                QPoint(right - 18, top), QPoint(right, top), QPoint(right, top + 18)});
        };

        if (!cell->entries.empty())
        {
            const SchedulePageEntry& first = cell->entries.front();
            const QColor fill = validColor(first.classColor, Qt::white);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(card, 6, 6);

            if (model->displayMode() == Application::ScheduleDisplayMode::Intensive)
            {
                const QRect content = card.toRect().adjusted(4, 3, -4, -3);
                const auto layouts = layoutScheduleIntensiveEntries(
                    content, cell->entries,
                    model->preferences().showEnglishNames, option.font);
                painter->save();
                painter->setClipRect(content);
                for (std::size_t entryIndex = 0;
                     entryIndex < cell->entries.size(); ++entryIndex)
                {
                    const SchedulePageEntry& entry = cell->entries[entryIndex];
                    const QColor ink = validColor(entry.fontColor, Qt::black);
                    const ScheduleIntensiveEntryLayout& layout =
                        layouts[entryIndex];
                    const auto drawLine = [painter, &ink](
                        const ScheduleIntensiveTextLine& line) {
                        painter->save();
                        painter->setClipRect(line.bounds);
                        painter->setFont(line.font);
                        painter->setPen(ink);
                        painter->drawText(line.bounds,
                            Qt::AlignCenter | Qt::TextSingleLine,
                            line.visibleText);
                        painter->restore();
                    };
                    drawLine(layout.teacher);
                    drawLine(layout.room);
                    drawLine(layout.classLine);
                }
                painter->restore();
            }
            else
            {
                QString html;
                for (const SchedulePageEntry& entry : cell->entries)
                {
                    const QColor ink = validColor(entry.fontColor, Qt::black);
                    const QString teacherLine = entryTeacherLine(
                        entry, model->preferences().showEnglishNames).toHtmlEscaped();
                    const QString classLine = scheduleEntryClassLine(entry).toHtmlEscaped();
                    const QString color = ink.name(QColor::HexRgb);
                    if (entry.testingClass)
                    {
                        html += QStringLiteral(
                            "<div style='margin-bottom:6.4px;text-align:center;line-height:0.92;'>"
                            "<div style='color:%1;font-size:13pt;font-weight:700;'>%2</div>"
                            "<div style='color:%1;font-size:11pt;font-weight:500;'>%3</div>"
                            "<div style='color:%1;font-size:10pt;font-weight:400;'>%4</div></div>")
                            .arg(color, entry.className.toHtmlEscaped(),
                                 teacherLine, classLine);
                    }
                    else
                    {
                        html += QStringLiteral(
                            "<div style='margin-bottom:6.4px;text-align:center;line-height:0.96;'>"
                            "<div style='color:%1;font-size:15pt;font-weight:600;'>%2</div>"
                            "<div style='color:%1;font-size:14pt;font-weight:400;'>%3</div></div>")
                            .arg(color, teacherLine, classLine);
                    }
                }
                QTextDocument document;
                document.setDocumentMargin(0);
                document.setDefaultFont(option.font);
                document.setHtml(html);
                const QRectF content = card.adjusted(4, 3, -4, -3);
                document.setTextWidth(content.width());
                painter->save();
                painter->setClipRect(content);
                painter->translate(content.left(),
                    content.top() + std::max(0.0,
                        (content.height() - document.size().height()) / 2.0));
                QAbstractTextDocumentLayout::PaintContext context;
                document.documentLayout()->draw(painter, context);
                painter->restore();
            }
            if (cell->entries.size() == 1 && first.testingClass)
            {
                drawMarker(validColor(first.fontColor, Qt::black));
            }
        }
        else if (cell->plainTestingAssignment)
        {
            const QColor fill(dark ? QStringLiteral("#4B3D20")
                                   : QStringLiteral("#FFF0B8"));
            const QColor border(dark ? QStringLiteral("#8D7339")
                                     : QStringLiteral("#D39B25"));
            painter->setPen(QPen(border, 1));
            painter->setBrush(fill);
            painter->drawRoundedRect(card, 6, 6);
            painter->setPen(QColor(dark ? QStringLiteral("#FFF2C2")
                                        : QStringLiteral("#4A3500")));
            QFont testingFont = option.font;
            testingFont.setPointSize(14);
            testingFont.setBold(true);
            painter->setFont(testingFont);
            const QString text = cell->testingRoom.trimmed().isEmpty()
                ? scheduleText("Oral Testing")
                : scheduleText("Oral Testing") + QStringLiteral("\nRm: ")
                    + cell->testingRoom.trimmed();
            painter->drawText(card.adjusted(4, 3, -4, -3),
                Qt::AlignCenter | Qt::TextWordWrap, text);
            drawMarker(QColor(dark ? QStringLiteral("#FFD166")
                                   : QStringLiteral("#B66A00")));
        }
        else
        {
            QColor fill;
            QColor ink = Qt::black;
            QString text;
            QFont slotFont = option.font;
            slotFont.setPointSize(16);
            slotFont.setBold(true);
            switch (cell->state)
            {
            case Application::ScheduleSlotState::Empty:
                painter->restore();
                return;
            case Application::ScheduleSlotState::Essay:
                fill = Qt::white;
                text = scheduleText("Essay");
                slotFont.setItalic(true);
                break;
            case Application::ScheduleSlotState::Lunch:
                fill = QColor(QStringLiteral("#DCDCDC"));
                text = scheduleText("Lunch");
                slotFont.setWeight(QFont::Black);
                break;
            }
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(card, 6, 6);
            painter->setFont(slotFont);
            painter->setPen(ink);
            painter->drawText(card.adjusted(4, 3, -4, -3),
                Qt::AlignCenter | Qt::TextWordWrap, text);
        }
        painter->restore();
    }

    QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        if (index.column() == 0)
        {
            return base.expandedTo(QSize(80, 46));
        }
        const auto* model = dynamic_cast<const SchedulePageModel*>(index.model());
        const SchedulePageCell* const cell = model
            ? model->cellAt(index.row(), index.column()) : nullptr;
        const int entryHeight = cell && !cell->entries.empty()
            ? 22 + static_cast<int>(cell->entries.size()) * 38 : 60;
        return base.expandedTo(QSize(100, entryHeight));
    }
};

class CallbackScheduleSlotStateSavePort final
    : public Application::ScheduleSlotStateSavePort
{
public:
    CallbackScheduleSlotStateSavePort(
        const QString& path,
        const SchedulePageActions& actions
        )
        : m_path(path)
        , m_actions(&actions)
    {
    }

    [[nodiscard]] Application::ScheduleSlotStateSaveResult saveSlotState(
        const Application::ScheduleSlotStateSaveRequest& request
        ) const override
    {
        if (!m_actions || !m_actions->saveSlotState)
        {
            return Application::ScheduleSlotStateSaveResult::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "Schedule slot editing is unavailable.",
                .recoverable = true
            });
        }
        return m_actions->saveSlotState(m_path, request);
    }

private:
    QString m_path;
    const SchedulePageActions* m_actions = nullptr;
};

[[nodiscard]] std::optional<Application::ScheduleWeekday> weekdayFor(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday")) return Application::ScheduleWeekday::Monday;
    if (day == QStringLiteral("Tuesday")) return Application::ScheduleWeekday::Tuesday;
    if (day == QStringLiteral("Wednesday")) return Application::ScheduleWeekday::Wednesday;
    if (day == QStringLiteral("Thursday")) return Application::ScheduleWeekday::Thursday;
    if (day == QStringLiteral("Friday")) return Application::ScheduleWeekday::Friday;
    if (day == QStringLiteral("Saturday")) return Application::ScheduleWeekday::Saturday;
    if (day == QStringLiteral("Sunday")) return Application::ScheduleWeekday::Sunday;
    return std::nullopt;
}

[[nodiscard]] Application::ScheduleSlotState nextSlotState(
    const Application::ScheduleSlotState current
    ) noexcept
{
    switch (current)
    {
    case Application::ScheduleSlotState::Essay:
        return Application::ScheduleSlotState::Lunch;
    case Application::ScheduleSlotState::Lunch:
        return Application::ScheduleSlotState::Empty;
    case Application::ScheduleSlotState::Empty:
        return Application::ScheduleSlotState::Essay;
    }
    return Application::ScheduleSlotState::Essay;
}

[[nodiscard]] QString operationMessage(const Domain::OperationError& error)
{
    return QString::fromUtf8(
        error.message.data(), static_cast<qsizetype>(error.message.size()));
}

} // namespace

SchedulePage::SchedulePage(
    const PageContext& context,
    SchedulePageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextSchedulePage"));
    buildUi();
    retranslateUi();
}

PageOperationResult<void> SchedulePage::onActivating()
{
    m_pageActive = true;
    const QString identity = workspaceIdentity();
    if (identity != m_workspaceIdentity)
    {
        clearRetainedState(true);
        m_workspaceIdentity = identity;
    }
    if (identity.isEmpty() || activePath().isEmpty())
    {
        clearForClosedWorkspace();
        return {};
    }
    static_cast<void>(reload());
    return {};
}

void SchedulePage::onSuspending() noexcept
{
    m_pageActive = false;
    closeScheduleImport();
    clearRetainedState(false);
}

void SchedulePage::clearForWorkspaceChange()
{
    closeScheduleImport();
    clearRetainedState(true);
    m_workspaceIdentity.clear();
    setStatus(pageText("Schedule data is not loaded."));
}

void SchedulePage::clearForClosedWorkspace()
{
    closeScheduleImport();
    clearRetainedState(true);
    m_workspaceIdentity.clear();
    setStatus(pageText("No Teacher Profile is open."));
    setControlsEnabled(false);
}

bool SchedulePage::reload()
{
    if (m_loading)
    {
        return false;
    }
    const QString path = activePath();
    if (path.isEmpty())
    {
        clearForClosedWorkspace();
        return false;
    }
    m_loading = true;
    m_retry->setEnabled(false);
    setStatus(pageText("Loading schedule…"));
    if (!m_actions.readSchedule)
    {
        m_model->clear();
        updateTestingBanner();
        m_lastError = pageText("Schedule actions are unavailable.");
        setStatus(m_lastError, true);
        setControlsEnabled(false);
        m_loading = false;
        m_retry->setEnabled(true);
        return false;
    }

    Application::SchedulePageReadResult loaded = m_actions.readSchedule(path);
    if (!loaded)
    {
        m_model->clear();
        updateTestingBanner();
        m_lastError = operationMessage(loaded.error());
        setStatus(m_lastError, true);
        setControlsEnabled(false);
        m_loading = false;
        m_retry->setEnabled(true);
        return false;
    }

    const Domain::Result<void> applied = m_model->setSnapshot(
        std::move(loaded.value()));
    if (!applied)
    {
        m_model->clear();
        updateTestingBanner();
        m_lastError = operationMessage(applied.error());
        setStatus(m_lastError, true);
        setControlsEnabled(false);
        m_loading = false;
        m_retry->setEnabled(true);
        return false;
    }

    m_lastError.clear();
    restoreControlsFromSnapshot();
    setControlsEnabled(true);
    setStatus(pageText("Schedule data is ready."));
    updateRowHeights();
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 92);
    for (int column = 1; column < m_table->model()->columnCount(); ++column)
    {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
    }
    m_loading = false;
    m_retry->setEnabled(true);
    return true;
}

SchedulePageModel* SchedulePage::model() const noexcept
{
    return m_model;
}

QTableView* SchedulePage::scheduleTable() const noexcept
{
    return m_table;
}

QPushButton* SchedulePage::modeButton(
    const Application::ScheduleDisplayMode mode
    ) const noexcept
{
    const int index = static_cast<int>(mode);
    return index >= 0 && index < 3 ? m_modeButtons[index] : nullptr;
}

QCheckBox* SchedulePage::preferenceCheckBox(const int index) const noexcept
{
    return index >= 0 && index < 5 ? m_preferenceCheckBoxes[index] : nullptr;
}

QPushButton* SchedulePage::retryButton() const noexcept
{
    return m_retry;
}

QLabel* SchedulePage::statusLabel() const noexcept
{
    return m_status;
}

QAction* SchedulePage::scheduleOutputAction() const noexcept
{
    return m_scheduleOutputAction;
}

ScheduleImportWorkflowDialog* SchedulePage::importDialog() const noexcept
{
    return m_importDialog;
}

SchedulePageVisualMetrics SchedulePage::visualMetrics() const
{
    SchedulePageVisualMetrics metrics;
    metrics.model = m_model->runtimeMetrics();
    metrics.tableViewCount = m_table ? 1 : 0;
    metrics.descendantWidgetCount = findChildren<QWidget*>().size();
    if (m_table && m_table->model())
    {
        for (int row = 0; row < m_table->model()->rowCount(); ++row)
        {
            for (int column = 0; column < m_table->model()->columnCount(); ++column)
            {
                if (m_table->indexWidget(m_table->model()->index(row, column)))
                {
                    ++metrics.tableCellWidgetCount;
                }
            }
        }
    }
    return metrics;
}

void SchedulePage::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    else if (event && event->type() == QEvent::PaletteChange)
    {
        updateTestingBanner();
    }
    QWidget::changeEvent(event);
}

void SchedulePage::buildUi()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);
    setAccessibleName(pageText("Weekly Class Schedule"));

    auto* const modes = new QHBoxLayout();
    modes->setContentsMargins(0, 0, 0, 0);
    modes->setSpacing(8);
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    const std::array<const char*, 3> modeNames{
        "Regular", "Intensive", "Testing"
    };
    const std::array<const char*, 3> modeObjects{
        "nextScheduleModeRegular", "nextScheduleModeIntensive",
        "nextScheduleModeTesting"
    };
    for (int index = 0; index < 3; ++index)
    {
        auto* const button = new QPushButton(this);
        button->setObjectName(QString::fromLatin1(modeObjects[index]));
        button->setProperty("translationSource", QString::fromLatin1(modeNames[index]));
        button->setCheckable(true);
        button->setMinimumSize(96, 40);
        button->setMaximumHeight(40);
        m_modeButtons[index] = button;
        m_modeGroup->addButton(button, index);
        modes->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, index] {
            handleModeChanged(static_cast<Application::ScheduleDisplayMode>(index));
        });
    }
    modes->addStretch(1);
    m_preferencesButton = new QToolButton(this);
    m_preferencesButton->setObjectName(QStringLiteral("nextSchedulePreferences"));
    m_preferencesButton->setProperty(
        "translationSource", QStringLiteral("Preferences"));
    m_preferencesButton->setMinimumSize(112, 40);
    m_preferencesButton->setMaximumHeight(40);
    m_preferencesButton->setPopupMode(QToolButton::InstantPopup);
    m_preferencesMenu = new QMenu(m_preferencesButton);
    m_preferencesButton->setMenu(m_preferencesMenu);
    modes->addWidget(m_preferencesButton);
    m_importAction = new QAction(m_preferencesMenu);
    m_importAction->setObjectName(QStringLiteral("nextScheduleImportAction"));
    m_importAction->setProperty(
        "translationSource", QStringLiteral("Import Schedule"));
    m_preferencesMenu->addAction(m_importAction);
    m_preferencesMenu->addSeparator();
    connect(m_importAction, &QAction::triggered, this, [this] {
        startScheduleImport();
    });
    m_scheduleOutputAction = new QAction(m_preferencesMenu);
    m_scheduleOutputAction->setObjectName(
        QStringLiteral("nextScheduleOutputAction"));
    m_scheduleOutputAction->setProperty(
        "translationSource", QStringLiteral("Print Schedule…"));
    m_preferencesMenu->addAction(m_scheduleOutputAction);
    connect(m_scheduleOutputAction, &QAction::triggered,
        this, &SchedulePage::startScheduleOutput);
    m_preferencesMenu->addSeparator();
    root->addLayout(modes);

    const std::array<const char*, 5> preferenceNames{
        "24-hour time", "English teacher names", "Show weekends",
        "Show all intensive hours", "Testing affects M1"
    };
    const std::array<const char*, 5> preferenceObjects{
        "nextScheduleUse24Hour", "nextScheduleEnglishNames",
        "nextScheduleShowWeekends", "nextScheduleShowAllIntensiveHours",
        "nextScheduleTestingAffectsM1"
    };
    for (int index = 0; index < 5; ++index)
    {
        auto* const checkBox = new QCheckBox();
        checkBox->setObjectName(QString::fromLatin1(preferenceObjects[index]));
        checkBox->setProperty("translationSource",
            QString::fromLatin1(preferenceNames[index]));
        m_preferenceCheckBoxes[index] = checkBox;
        auto* const action = new QWidgetAction(m_preferencesMenu);
        action->setDefaultWidget(checkBox);
        m_preferencesMenu->addAction(action);
        connect(checkBox, &QCheckBox::clicked, this, [this] {
            handlePreferenceChanged();
        });
    }

    m_statusContainer = new QWidget(this);
    auto* const statusRow = new QHBoxLayout(m_statusContainer);
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(8);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("nextScheduleStatus"));
    m_status->setWordWrap(true);
    statusRow->addWidget(m_status, 1);
    m_retry = new QPushButton(this);
    m_retry->setObjectName(QStringLiteral("nextScheduleRetry"));
    m_retry->setProperty("translationSource", QStringLiteral("Retry"));
    statusRow->addWidget(m_retry);
    m_statusContainer->setVisible(false);
    root->addWidget(m_statusContainer);

    m_testingBanner = new QLabel(this);
    m_testingBanner->setObjectName(QStringLiteral("nextScheduleTestingBanner"));
    m_testingBanner->setAlignment(Qt::AlignCenter);
    m_testingBanner->setWordWrap(true);
    m_testingBanner->setMinimumHeight(36);
    m_testingBanner->setVisible(false);
    root->addWidget(m_testingBanner);

    connect(m_retry, &QPushButton::clicked, this, [this] {
        static_cast<void>(reload());
    });

    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("nextScheduleTable"));
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(false);
    m_table->setWordWrap(true);
    m_table->setShowGrid(false);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setStyleSheet(QStringLiteral(
        "QTableView#nextScheduleTable { border:none; background:transparent; }"
        "QTableView#nextScheduleTable::item { padding:0; border:none; }"));
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(60);
    m_table->horizontalHeader()->setMinimumSectionSize(84);
    m_table->horizontalHeader()->setFixedHeight(42);
    m_table->horizontalHeader()->setFont(QFont(m_table->font().family(), 10,
                                               QFont::DemiBold));
    m_table->horizontalHeader()->setSectionsClickable(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setFocusPolicy(Qt::NoFocus);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setItemDelegate(new ScheduleCellDelegate(m_table));
    m_model = new SchedulePageModel(m_table);
    m_table->setModel(m_model);
    root->addWidget(m_table, 1);
    connect(m_table, &QTableView::clicked,
        this, [this](const QModelIndex& index) {
            handleCellClicked(index.row(), index.column());
        });
    setControlsEnabled(false);
}

void SchedulePage::retranslateUi()
{
    setAccessibleName(pageText("Weekly Class Schedule"));
    for (QObject* object : findChildren<QObject*>())
    {
        const QVariant source = object->property("translationSource");
        if (!source.isValid())
        {
            continue;
        }
        const QString text = pageText(source.toString().toUtf8().constData());
        if (auto* const label = qobject_cast<QLabel*>(object))
        {
            label->setText(text);
        }
        else if (auto* const button = qobject_cast<QPushButton*>(object))
        {
            button->setText(text);
        }
        else if (auto* const button = qobject_cast<QToolButton*>(object))
        {
            button->setText(text);
        }
        else if (auto* const checkBox = qobject_cast<QCheckBox*>(object))
        {
            checkBox->setText(text);
        }
        else if (auto* const action = qobject_cast<QAction*>(object))
        {
            action->setText(text);
        }
    }
    updateTestingBanner();
    if (m_model)
    {
        m_model->headerDataChanged(Qt::Horizontal, 0,
            std::max(0, m_model->columnCount() - 1));
        m_table->viewport()->update();
    }
}

void SchedulePage::clearRetainedState(const bool clearIdentity)
{
    closeScheduleImport();
    m_model->clear();
    m_lastError.clear();
    if (clearIdentity)
    {
        m_workspaceIdentity.clear();
    }
    m_synchronizingControls = true;
    for (QCheckBox* checkBox : m_preferenceCheckBoxes)
    {
        if (checkBox)
        {
            checkBox->setChecked(false);
        }
    }
    if (m_modeButtons[static_cast<int>(Application::ScheduleDisplayMode::Regular)])
    {
        m_modeButtons[static_cast<int>(Application::ScheduleDisplayMode::Regular)]
            ->setChecked(true);
    }
    m_synchronizingControls = false;
    updateTestingBanner();
    m_table->clearSelection();
    m_table->scrollToTop();
    setControlsEnabled(false);
}

void SchedulePage::setStatus(const QString& message, const bool error)
{
    m_status->setText(message);
    m_status->setProperty("error", error);
    m_status->setStyleSheet(error
        ? QStringLiteral("color: #a32626; font-weight: 600;")
        : QString{});
    const bool visible = error
        || message != pageText("Schedule data is ready.");
    m_statusContainer->setVisible(visible);
    m_retry->setVisible(error && !activePath().isEmpty());
    m_retry->setEnabled(!activePath().isEmpty());
}

void SchedulePage::setControlsEnabled(const bool enabled)
{
    for (QPushButton* button : m_modeButtons)
    {
        if (button)
        {
            button->setEnabled(enabled);
        }
    }
    for (QCheckBox* checkBox : m_preferenceCheckBoxes)
    {
        if (checkBox)
        {
            checkBox->setEnabled(enabled);
        }
    }
    if (m_preferencesButton)
    {
        m_preferencesButton->setEnabled(enabled || !activePath().isEmpty());
    }
    if (m_importAction)
    {
        m_importAction->setEnabled(!activePath().isEmpty()
                                   && m_importDialog.isNull());
    }
    if (m_scheduleOutputAction)
    {
        m_scheduleOutputAction->setEnabled(enabled && !activePath().isEmpty());
    }
    m_table->setEnabled(enabled);
    m_retry->setEnabled(!activePath().isEmpty());
}

void SchedulePage::startScheduleImport()
{
    if (m_importDialog || !m_pageActive)
    {
        return;
    }
    const QString path = activePath();
    const QString identity = workspaceIdentity();
    if (path.isEmpty() || identity.isEmpty())
    {
        setStatus(pageText("No Teacher Profile is open."), true);
        return;
    }

    ScheduleImportWorkflowActions actions;
    actions.chooseWorkbook = m_actions.chooseScheduleImportWorkbook;
    actions.parseWorkbook = m_actions.parseScheduleImportWorkbook;
    actions.parseTimeoutMilliseconds = m_actions.scheduleImportTimeoutMilliseconds;
    actions.activeWorkspacePath = [this] { return activePath(); };
    actions.workspaceIdentity = [this] { return workspaceIdentity(); };
    actions.readImportState = m_actions.readScheduleImportState;
    actions.applyImport = m_actions.applyScheduleImport;
    actions.importApplied = [this](const auto& summary) {
        handleScheduleImportApplied(summary);
    };

    auto* const dialog = new ScheduleImportWorkflowDialog(
        std::move(actions), path, identity, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    m_importDialog = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        if (m_importDialog == dialog)
        {
            m_importDialog = nullptr;
            if (m_importAction)
            {
                m_importAction->setEnabled(!activePath().isEmpty());
            }
        }
    });
    dialog->open();
    if (m_importAction)
    {
        m_importAction->setEnabled(false);
    }
}

void SchedulePage::closeScheduleImport() noexcept
{
    if (!m_importDialog)
    {
        return;
    }
    QPointer<ScheduleImportWorkflowDialog> dialog = m_importDialog;
    m_importDialog = nullptr;
    if (dialog)
    {
        dialog->reject();
        dialog->deleteLater();
    }
}

void SchedulePage::handleScheduleImportApplied(
    const Application::ScheduleImportApplySummary& summary
    )
{
    if (!reload())
    {
        return;
    }
    const QString message = pageText(
        "Schedule import completed: %1 teachers created, %2 room updates, "
        "%3 classes created, %4 classes updated, %5 skipped, %6 schedules cleared.")
        .arg(summary.teachersCreated.rawValue())
        .arg(summary.teachersUpdated.rawValue())
        .arg(summary.classesCreated.rawValue())
        .arg(summary.classesUpdated.rawValue())
        .arg(summary.classesSkipped.rawValue())
        .arg(summary.schedulesCleared.rawValue());
    setStatus(message);
}

void SchedulePage::selectMode(const Application::ScheduleDisplayMode mode)
{
    const int index = static_cast<int>(mode);
    if (index >= 0 && index < 3 && m_modeButtons[index])
    {
        m_modeButtons[index]->setChecked(true);
    }
}

void SchedulePage::handleModeChanged(const Application::ScheduleDisplayMode mode)
{
    if (m_synchronizingControls || m_loading
        || mode == m_model->displayMode())
    {
        return;
    }
    const QString path = activePath();
    if (!m_actions.saveDisplayMode || path.isEmpty())
    {
        selectMode(m_model->displayMode());
        setStatus(pageText("Schedule display mode could not be saved."), true);
        return;
    }
    const auto saved = m_actions.saveDisplayMode(path, mode);
    if (!saved)
    {
        selectMode(m_model->displayMode());
        m_lastError = operationMessage(saved.error());
        setStatus(m_lastError, true);
        return;
    }
    static_cast<void>(reload());
}

void SchedulePage::handlePreferenceChanged()
{
    if (m_synchronizingControls || m_loading)
    {
        return;
    }
    const QString path = activePath();
    const Application::ScheduleDisplayPreferences requested =
        preferencesFromControls();
    if (!m_actions.saveDisplayPreferences || path.isEmpty())
    {
        restoreControlsFromSnapshot();
        setStatus(pageText("Schedule display preferences could not be saved."), true);
        return;
    }
    const auto saved = m_actions.saveDisplayPreferences(path, requested);
    if (!saved)
    {
        restoreControlsFromSnapshot();
        m_lastError = operationMessage(saved.error());
        setStatus(m_lastError, true);
        return;
    }
    static_cast<void>(reload());
}

void SchedulePage::handleCellClicked(const int row, const int column)
{
    const SchedulePageCell* const cell = m_model->cellAt(row, column);
    if (!cell)
    {
        return;
    }
    const bool testingMode = m_model->displayMode()
        == Application::ScheduleDisplayMode::Testing;
    const bool assignedTestingClass = !cell->entries.empty()
        && cell->entries.front().testingClass;
    const bool hasTestingAssignment = cell->plainTestingAssignment
        || assignedTestingClass;
    if (testingMode && (hasTestingAssignment
        || (cell->entries.empty()
            && cell->state == Application::ScheduleSlotState::Essay)))
    {
        openTestingAssignment(*cell);
        return;
    }
    if (!cell->entries.empty())
    {
        if (!cell->entries.front().testingClass && m_actions.openClassDetails)
        {
            m_actions.openClassDetails(cell->entries.front().classId);
        }
        return;
    }
    if (!cell->slotTogglingEnabled || cell->plainTestingAssignment)
    {
        return;
    }
    const auto weekday = weekdayFor(cell->day);
    if (!weekday)
    {
        setStatus(pageText("The selected schedule slot is invalid."), true);
        return;
    }
    const Application::ScheduleSlotStateSaveRequest request{
        .weekday = *weekday,
        .startMinute = Application::ScheduleSlotStartMinute(cell->startMinute),
        .selectedState = nextSlotState(cell->state),
        .defaultState = cell->defaultState
    };
    const CallbackScheduleSlotStateSavePort port(activePath(), m_actions);
    const auto saved = Application::ScheduleSlotStateSaveUseCase::execute(
        request, port);
    if (!saved)
    {
        m_lastError = operationMessage(saved.error());
        setStatus(m_lastError, true);
        return;
    }
    static_cast<void>(reload());
}

void SchedulePage::openTestingAssignment(const SchedulePageCell& cell)
{
    const QString path = activePath();
    const QString identity = workspaceIdentity();
    if (path.isEmpty() || !m_actions.readTestingClassChoices
        || !m_actions.saveTestingAssignment)
    {
        setStatus(pageText("Testing assignment actions are unavailable."), true);
        return;
    }

    const auto choices = m_actions.readTestingClassChoices(path);
    if (!choices)
    {
        m_lastError = operationMessage(choices.error());
        setStatus(m_lastError, true);
        return;
    }

    const bool hasExistingAssignment = cell.plainTestingAssignment
        || (!cell.entries.empty() && cell.entries.front().testingClass);
    const QString existingRoom = cell.plainTestingAssignment
        ? cell.testingRoom : QString{};
    const std::optional<Domain::ClassId> existingClassId =
        !cell.entries.empty() && cell.entries.front().testingClass
            ? std::optional<Domain::ClassId>(cell.entries.front().classId)
            : std::nullopt;
    ScheduleTestingAssignmentDialog dialog(
        choices.value(), hasExistingAssignment, existingRoom,
        existingClassId, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    if (path != activePath() || identity != workspaceIdentity())
    {
        setStatus(pageText("Workspace changed while editing the schedule."), true);
        return;
    }

    const auto weekday = weekdayFor(cell.day);
    const QTime start(cell.startMinute / 60, cell.startMinute % 60);
    if (!weekday || !start.isValid())
    {
        setStatus(pageText("The selected schedule slot is invalid."), true);
        return;
    }
    const QString startTime = start.toString(QStringLiteral("HH:mm"));

    if (dialog.selectedAction()
        == ScheduleTestingAssignmentDialog::Action::ManageTestingClasses)
    {
        if (!m_actions.openTestingClasses)
        {
            setStatus(pageText("Testing Classes are unavailable."), true);
            return;
        }
        const auto domainWeekday = static_cast<Domain::Weekday>(*weekday);
        m_actions.openTestingClasses({
            .weekday = domainWeekday,
            .startMinute = Domain::ScheduleTime::StartMinute(cell.startMinute)
        });
        return;
    }

    Application::ScheduleTestingAssignmentSaveRequest request{
        .day = cell.day.toStdU16String(),
        .startTime = startTime.toStdU16String(),
        .replaceExisting = hasExistingAssignment
    };
    switch (dialog.selectedAction())
    {
    case ScheduleTestingAssignmentDialog::Action::SavePlainTesting:
        request.mutation =
            Application::ScheduleTestingAssignmentMutation::SavePlainTesting;
        request.room = dialog.room().toStdU16String();
        break;
    case ScheduleTestingAssignmentDialog::Action::AssignTestingClass:
        request.mutation =
            Application::ScheduleTestingAssignmentMutation::AssignTestingClass;
        request.classId = dialog.selectedClassId();
        break;
    case ScheduleTestingAssignmentDialog::Action::RemoveAssignment:
        request.mutation =
            Application::ScheduleTestingAssignmentMutation::RemoveAssignment;
        break;
    case ScheduleTestingAssignmentDialog::Action::ManageTestingClasses:
        return;
    }

    const auto saved = m_actions.saveTestingAssignment(path, request);
    if (!saved)
    {
        m_lastError = operationMessage(saved.error());
        setStatus(m_lastError, true);
        return;
    }
    static_cast<void>(reload());
}

void SchedulePage::startScheduleOutput()
{
    const QString path = activePath();
    const QString identity = workspaceIdentity();
    if (path.isEmpty())
    {
        setStatus(pageText("No Teacher Profile is open."), true);
        return;
    }

    ScheduleOutputDialog dialog(m_actions.chooseSchedulePdfPath, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    if (path != activePath() || identity != workspaceIdentity())
    {
        setStatus(pageText("Workspace changed while preparing schedule output."), true);
        return;
    }
    if (!m_actions.performScheduleOutput
        || !m_actions.readScheduleOutputProfileName)
    {
        setStatus(pageText("Schedule output actions are unavailable."), true);
        return;
    }

    QString profileName;
    const auto loadedName = m_actions.readScheduleOutputProfileName(path);
    if (!loadedName)
    {
        m_lastError = operationMessage(loadedName.error());
        setStatus(m_lastError, true);
        return;
    }
    profileName = QString::fromUtf8(
        loadedName.value().data(),
        static_cast<qsizetype>(loadedName.value().size()));

    const Theme currentTheme = QApplication::palette()
        .color(QPalette::Window).lightness() < 128
            ? Theme::Dark : Theme::Light;
    QFont englishFont = QApplication::font();
    const QStringList registeredFamilies = englishFont.families();
    const QString fallbackFamily = englishFont.family();
    const auto familyContaining = [&registeredFamilies, &fallbackFamily](
        const QString& fragment) {
        for (const QString& family : registeredFamilies)
        {
            if (family.contains(fragment, Qt::CaseInsensitive))
            {
                return family;
            }
        }
        return registeredFamilies.isEmpty()
            ? fallbackFamily
            : registeredFamilies.first();
    };
    QStringList englishFamilies{
        familyContaining(QStringLiteral("Inter")),
        familyContaining(QStringLiteral("Pretendard"))
    };
    englishFamilies.removeDuplicates();
    englishFont.setFamilies(englishFamilies);
    const bool koreanGlobalFont = !registeredFamilies.isEmpty()
        && registeredFamilies.first().contains(
            QStringLiteral("Pretendard"), Qt::CaseInsensitive);
#ifdef Q_OS_MACOS
    constexpr int StandardEnglishPointSize = 15;
#else
    constexpr int StandardEnglishPointSize = 14;
#endif
    const int englishPointSize = QApplication::font().pointSize()
        - (koreanGlobalFont ? 1 : 0);
    englishFont.setPointSize(englishPointSize > 0
        ? englishPointSize : StandardEnglishPointSize);
    QFont koreanFont = englishFont;
    QStringList koreanFamilies{
        familyContaining(QStringLiteral("Pretendard")),
        familyContaining(QStringLiteral("Inter"))
    };
    koreanFamilies.removeDuplicates();
    koreanFont.setFamilies(koreanFamilies);
    koreanFont.setPointSize(englishFont.pointSize() + 1);
    ScheduleOutputRequest request{
        .document = schedulePrintDocument(*m_model),
        .kind = dialog.selectedKind(),
        .style = dialog.selectedStyle(),
        .currentTheme = currentTheme,
        .englishFont = std::move(englishFont),
        .koreanFont = std::move(koreanFont),
        .profileName = std::move(profileName),
        .showEnglishNames = m_model->preferences().showEnglishNames,
        .orientation = dialog.selectedOrientation(),
        .documentPath = dialog.selectedPath()
    };
    const ScheduleOutputResult result =
        m_actions.performScheduleOutput(std::move(request), this);
    switch (result.status)
    {
    case ScheduleOutputResult::Status::Sent:
        m_lastError.clear();
        setStatus(result.message.isEmpty()
            ? pageText("Schedule output completed.") : result.message);
        break;
    case ScheduleOutputResult::Status::Canceled:
        break;
    case ScheduleOutputResult::Status::Failed:
        m_lastError = result.message.isEmpty()
            ? pageText("Schedule output could not be completed.")
            : result.message;
        setStatus(m_lastError, true);
        break;
    }
}

QString SchedulePage::activePath() const
{
    if (!m_actions.activeWorkspacePath)
    {
        return {};
    }
    try
    {
        return m_actions.activeWorkspacePath().trimmed();
    }
    catch (...)
    {
        return {};
    }
}

QString SchedulePage::workspaceIdentity() const
{
    if (!m_actions.workspaceIdentity)
    {
        return activePath();
    }
    try
    {
        return m_actions.workspaceIdentity();
    }
    catch (...)
    {
        return {};
    }
}

Application::ScheduleDisplayPreferences SchedulePage::preferencesFromControls() const
{
    return {
        .use24HourTime = m_preferenceCheckBoxes[0]->isChecked(),
        .showEnglishNames = m_preferenceCheckBoxes[1]->isChecked(),
        .showWeekends = m_preferenceCheckBoxes[2]->isChecked(),
        .showAllIntensiveHours = m_preferenceCheckBoxes[3]->isChecked(),
        .testingAffectsM1 = m_preferenceCheckBoxes[4]->isChecked()
    };
}

void SchedulePage::restoreControlsFromSnapshot()
{
    m_synchronizingControls = true;
    selectMode(m_model->displayMode());
    const Application::ScheduleDisplayPreferences& preferences =
        m_model->preferences();
    m_preferenceCheckBoxes[0]->setChecked(preferences.use24HourTime);
    m_preferenceCheckBoxes[1]->setChecked(preferences.showEnglishNames);
    m_preferenceCheckBoxes[2]->setChecked(preferences.showWeekends);
    m_preferenceCheckBoxes[3]->setChecked(preferences.showAllIntensiveHours);
    m_preferenceCheckBoxes[4]->setChecked(preferences.testingAffectsM1);
    m_synchronizingControls = false;
    updateTestingBanner();
}

void SchedulePage::updateTestingBanner()
{
    if (!m_testingBanner)
    {
        return;
    }
    const bool testing = m_model
        && m_model->displayMode() == Application::ScheduleDisplayMode::Testing;
    if (testing)
    {
        const bool hidesM1 = m_model->preferences().testingAffectsM1;
        m_testingBanner->setText(scheduleText(hidesM1
            ? "Testing View — M1, M2, and M3 classes are hidden"
            : "Testing View — M2 and M3 classes are hidden; M1 classes remain"));
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        m_testingBanner->setStyleSheet(dark
            ? QStringLiteral(
                "QLabel {background:#4B3D20;color:#FFF2C2;"
                "border:1px solid #8D7339;border-radius:6px;"
                "padding:7px;font-weight:600;}")
            : QStringLiteral(
                "QLabel {background:#FFF4CC;color:#503B00;"
                "border:1px solid #D5A52E;border-radius:6px;"
                "padding:7px;font-weight:600;}"));
    }
    m_testingBanner->setVisible(testing);
}

void SchedulePage::updateRowHeights()
{
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        int maximumEntryCount = 0;
        for (int column = 1; column < m_model->columnCount(); ++column)
        {
            if (const SchedulePageCell* const cell = m_model->cellAt(row, column))
            {
                maximumEntryCount = std::max(
                    maximumEntryCount, static_cast<int>(cell->entries.size()));
            }
        }
        m_table->setRowHeight(row, maximumEntryCount > 0
            ? 22 + maximumEntryCount * 38
            : 60);
    }
}

} // namespace ClassMngr::Next::Ui
