#include "schedule_table_renderer.h"

#include "schedule_cell_widget_factory.h"
#include "schedule_widget_delegates.h"
#include "schedule_widget_geometry.h"

#include "core/fontmanager.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QObject>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>

namespace
{
using ScheduleWidgetDelegates::TimeCellRole;
using ScheduleWidgetDelegates::TimeColumnDelegate;

constexpr int TimeColumnWidth = 90;
constexpr int HeaderHeight = 42;
constexpr int RowHeight = 48;
constexpr int RowBaseHeight = 22;
constexpr int RowHeightPerEntry = 38;
constexpr int CompactPreviewTimeColumnWidth = 84;
constexpr int CompactPreviewHeaderHeight = 36;
constexpr int CompactPreviewRowHeight = 40;
constexpr int CompactPreviewRowBaseHeight = 19;
constexpr int CompactPreviewRowHeightPerEntry = 32;
constexpr int PreviewFontSizeReduction = 4;
constexpr int PreviewTimeFontSizeReduction = 2;
constexpr char RenderStateObjectName[] = "scheduleTableRenderState";

class ScheduleTableRenderState final : public QObject
{
public:
    explicit ScheduleTableRenderState(
        QObject* parent
        )
        : QObject(parent)
    {
        setObjectName(QString::fromLatin1(RenderStateObjectName));
    }

    bool hasRendered = false;
    ScheduleViewModel model;
    int maximumVisibleRows = 0;
    bool compactPreview = false;
    bool showKoreanTeacherEnglishNames = false;
};

QString translate(
    const char* source
    )
{
    return QCoreApplication::translate(
        "ScheduleWidget",
        source
        );
}

QString weekdayLabel(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return translate("Monday");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return translate("Tuesday");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return translate("Wednesday");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return translate("Thursday");
    }
    if (day == QStringLiteral("Friday"))
    {
        return translate("Friday");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return translate("Saturday");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return translate("Sunday");
    }
    return day;
}

bool equivalent(
    const ScheduleEntry& first,
    const ScheduleEntry& second
    )
{
    return first.classId == second.classId
        && first.kind == second.kind
        && first.className == second.className
        && first.teacherKr == second.teacherKr
        && first.teacherEn == second.teacherEn
        && first.teacherPreferredName == second.teacherPreferredName
        && first.roomNumber == second.roomNumber
        && first.classGrade == second.classGrade
        && first.classLevel == second.classLevel
        && first.classColor == second.classColor
        && first.fontColor == second.fontColor;
}

bool equivalent(
    const ScheduleCellView& first,
    const ScheduleCellView& second
    )
{
    if (
        first.day != second.day
        || first.timeLabel != second.timeLabel
        || first.defaultSlotState != second.defaultSlotState
        || first.slotState != second.slotState
        || first.testingRoom != second.testingRoom
        || first.testingClassAssignment != second.testingClassAssignment
        || first.testingClassId != second.testingClassId
        || first.slotTogglingEnabled != second.slotTogglingEnabled
        || first.testingBlockCreationEnabled
            != second.testingBlockCreationEnabled
        || first.entries.size() != second.entries.size()
        )
    {
        return false;
    }

    for (int index = 0; index < first.entries.size(); ++index)
    {
        if (!equivalent(first.entries.at(index), second.entries.at(index)))
        {
            return false;
        }
    }

    return true;
}

bool equivalent(
    const ScheduleRowView& first,
    const ScheduleRowView& second
    )
{
    if (
        first.timeLabel != second.timeLabel
        || first.timeRangeLabel != second.timeRangeLabel
        || first.cells.size() != second.cells.size()
        )
    {
        return false;
    }

    for (int index = 0; index < first.cells.size(); ++index)
    {
        if (!equivalent(first.cells.at(index), second.cells.at(index)))
        {
            return false;
        }
    }

    return true;
}

bool equivalent(
    const ScheduleViewModel& first,
    const ScheduleViewModel& second
    )
{
    if (
        first.days != second.days
        || first.rows.size() != second.rows.size()
        )
    {
        return false;
    }

    for (int index = 0; index < first.rows.size(); ++index)
    {
        if (!equivalent(first.rows.at(index), second.rows.at(index)))
        {
            return false;
        }
    }

    return true;
}

ScheduleTableRenderState* renderState(
    QTableWidget* table
    )
{
    for (QObject* child : table->children())
    {
        if (
            child->objectName()
            == QString::fromLatin1(RenderStateObjectName)
            )
        {
            return static_cast<ScheduleTableRenderState*>(child);
        }
    }

    return new ScheduleTableRenderState(table);
}

bool canUpdateIncrementally(
    const QTableWidget* table,
    const ScheduleTableRenderState* state,
    const ScheduleViewModel& model,
    const ScheduleTableRenderOptions& options
    )
{
    if (
        !state->hasRendered
        || state->compactPreview != options.compactPreview
        || state->showKoreanTeacherEnglishNames
            != options.showKoreanTeacherEnglishNames
        || state->model.rows.size() != model.rows.size()
        || state->model.rows.isEmpty() != model.rows.isEmpty()
        || table->columnCount() != model.days.size() + 1
        )
    {
        return false;
    }

    const int expectedRowCount =
        model.rows.isEmpty()
            ? 1
            : model.rows.size();
    if (table->rowCount() != expectedRowCount)
    {
        return false;
    }

    for (int index = 0; index < model.rows.size(); ++index)
    {
        if (
            state->model.rows.at(index).cells.size()
            != model.rows.at(index).cells.size()
            )
        {
            return false;
        }
    }

    return true;
}

void configureColumns(
    QTableWidget* table,
    const QStringList& days,
    bool compactPreview
    )
{
    QStringList headers{
        translate("Time")
    };

    for (const QString& day : days)
    {
        headers.append(weekdayLabel(day));
    }

    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(
        0,
        QHeaderView::Fixed
        );
    table->setColumnWidth(
        0,
        compactPreview
            ? CompactPreviewTimeColumnWidth
            : TimeColumnWidth
        );

    for (int column = 1; column < headers.size(); ++column)
    {
        table->horizontalHeader()->setSectionResizeMode(
            column,
            QHeaderView::Stretch
            );
    }
}

int clearCellWidgets(
    QTableWidget* table
    )
{
    int removed = 0;

    for (int row = 0; row < table->rowCount(); ++row)
    {
        for (int column = 0; column < table->columnCount(); ++column)
        {
            QWidget* widget = table->cellWidget(row, column);
            if (!widget)
            {
                continue;
            }

            table->removeCellWidget(row, column);
            widget->deleteLater();
            ++removed;
        }
    }

    return removed;
}

void updateTableHeight(
    QTableWidget* table,
    const ScheduleTableRenderOptions& options
    )
{
    const ScheduleTableGeometry geometry =
        ScheduleWidgetGeometry::tableGeometry(
            table,
            options.maximumVisibleRows
            );

    table->setVerticalScrollBarPolicy(
        geometry.hasHiddenRows
            ? Qt::ScrollBarAsNeeded
            : Qt::ScrollBarAlwaysOff
        );
    table->setMinimumHeight(geometry.height);
    table->setMaximumHeight(geometry.height);
    table->updateGeometry();

    if (options.cellParent)
    {
        options.cellParent->updateGeometry();
    }
}
}

void ScheduleTableRenderer::initialize(
    QTableWidget* table
    )
{
    if (!table)
    {
        return;
    }

    table->verticalHeader()->setVisible(false);
    table->setFrameShape(QFrame::NoFrame);
    table->setShowGrid(false);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setWordWrap(true);
    table->setAlternatingRowColors(false);
    table->setItemDelegateForColumn(
        0,
        new TimeColumnDelegate(table)
        );
    table->verticalHeader()->setDefaultSectionSize(RowHeight);
    table->horizontalHeader()->setFont(
        FontManager::getUiFont(
            12,
            QFont::DemiBold
            )
        );
    table->horizontalHeader()->setFixedHeight(HeaderHeight);
    table->horizontalHeader()->setSectionsClickable(false);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setFocusPolicy(Qt::NoFocus);
}

void ScheduleTableRenderer::invalidate(
    QTableWidget* table
    )
{
    if (!table)
    {
        return;
    }

    ScheduleTableRenderState* state = renderState(table);
    state->hasRendered = false;
}

ScheduleTableRenderMetrics ScheduleTableRenderer::render(
    QTableWidget* table,
    const ScheduleViewModel& model,
    const ScheduleTableRenderOptions& options
    )
{
    if (!table)
    {
        return {};
    }

    ScheduleTableRenderState* state = renderState(table);
    const bool sameModel =
        state->hasRendered
        && equivalent(state->model, model);
    const bool sameCellAppearance =
        state->hasRendered
        && state->compactPreview == options.compactPreview
        && state->showKoreanTeacherEnglishNames
            == options.showKoreanTeacherEnglishNames;

    if (sameModel && sameCellAppearance)
    {
        if (state->maximumVisibleRows != options.maximumVisibleRows)
        {
            updateTableHeight(table, options);
        }

        state->maximumVisibleRows = options.maximumVisibleRows;
        return {};
    }

    ScheduleTableRenderMetrics metrics;
    const bool incremental =
        canUpdateIncrementally(
            table,
            state,
            model,
            options
            );
    metrics.fullRender = !incremental;

    const int headerFontSize =
        options.compactPreview
            ? 12 - PreviewFontSizeReduction
            : 12;
    const int headerHeight =
        options.compactPreview
            ? CompactPreviewHeaderHeight
            : HeaderHeight;
    const int rowHeight =
        options.compactPreview
            ? CompactPreviewRowHeight
            : RowHeight;

    if (incremental)
    {
        if (state->model.days != model.days)
        {
            configureColumns(
                table,
                model.days,
                options.compactPreview
                );
        }
    }
    else
    {
        table->verticalHeader()->setDefaultSectionSize(rowHeight);
        table->horizontalHeader()->setFont(
            FontManager::getUiFont(
                headerFontSize,
                QFont::DemiBold
                )
            );
        table->horizontalHeader()->setFixedHeight(headerHeight);

        configureColumns(
            table,
            model.days,
            options.compactPreview
            );
        metrics.cellWidgetsRemoved = clearCellWidgets(table);
        metrics.cellWidgetsQueuedForDeletion = metrics.cellWidgetsRemoved;
        table->clearContents();
        table->clearSpans();
        table->setRowCount(model.rows.size());
    }

    const ScheduleCellWidgetOptions cellWidgetOptions{
        options.cellParent,
        options.compactPreview,
        options.showKoreanTeacherEnglishNames
    };

    for (int rowIndex = 0; rowIndex < model.rows.size(); ++rowIndex)
    {
        const ScheduleRowView& scheduleRow = model.rows[rowIndex];
        QTableWidgetItem* timeItem = table->item(rowIndex, 0);
        if (!timeItem)
        {
            timeItem =
                new QTableWidgetItem(scheduleRow.timeRangeLabel);
            ++metrics.tableItemsCreated;
            timeItem->setFlags(Qt::ItemIsEnabled);
            timeItem->setData(TimeCellRole, true);
            timeItem->setTextAlignment(Qt::AlignCenter);
            timeItem->setFont(
                FontManager::getUiFont(
                    options.compactPreview
                        ? 11 - PreviewTimeFontSizeReduction
                        : 11,
                    QFont::Medium
                    )
                );
            table->setItem(rowIndex, 0, timeItem);
        }
        else if (timeItem->text() != scheduleRow.timeRangeLabel)
        {
            timeItem->setText(scheduleRow.timeRangeLabel);
        }

        const ScheduleRowView* previousRow =
            incremental
                ? &state->model.rows.at(rowIndex)
                : nullptr;

        int maxEntryCount = 1;
        for (int dayIndex = 0; dayIndex < scheduleRow.cells.size(); ++dayIndex)
        {
            const ScheduleCellView& cell = scheduleRow.cells[dayIndex];
            if (!cell.entries.isEmpty())
            {
                maxEntryCount =
                    std::max(
                        maxEntryCount,
                        static_cast<int>(cell.entries.size())
                        );
            }

            const bool cellChanged =
                !previousRow
                || !equivalent(
                    cell,
                    previousRow->cells.at(dayIndex)
                    );
            if (cellChanged)
            {
                QWidget* oldWidget =
                    table->cellWidget(rowIndex, dayIndex + 1);
                if (oldWidget)
                {
                    table->removeCellWidget(rowIndex, dayIndex + 1);
                    oldWidget->deleteLater();
                    ++metrics.cellWidgetsRemoved;
                    ++metrics.cellWidgetsQueuedForDeletion;
                }

                table->setCellWidget(
                    rowIndex,
                    dayIndex + 1,
                    ScheduleCellWidgetFactory::create(
                        cell,
                        cellWidgetOptions
                        )
                    );
                ++metrics.cellWidgetsCreated;
            }
        }

        if (!previousRow || !equivalent(scheduleRow, *previousRow))
        {
            table->setRowHeight(
                rowIndex,
                std::max(
                    rowHeight,
                    options.compactPreview
                        ? CompactPreviewRowBaseHeight
                            + (maxEntryCount * CompactPreviewRowHeightPerEntry)
                        : RowBaseHeight + (maxEntryCount * RowHeightPerEntry)
                    )
                );
        }
    }

    if (model.rows.isEmpty())
    {
        table->setRowCount(1);
        auto* item =
            new QTableWidgetItem(
                translate("No registered class meeting times available.")
                );
        ++metrics.tableItemsCreated;
        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(0, 0, item);
        table->setSpan(0, 0, 1, table->columnCount());
        table->setRowHeight(0, rowHeight);
    }

    updateTableHeight(table, options);
    state->hasRendered = true;
    state->model = model;
    state->maximumVisibleRows = options.maximumVisibleRows;
    state->compactPreview = options.compactPreview;
    state->showKoreanTeacherEnglishNames =
        options.showKoreanTeacherEnglishNames;
    return metrics;
}
