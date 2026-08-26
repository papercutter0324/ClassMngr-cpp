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

    ScheduleTableRenderMetrics metrics;

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

    const ScheduleCellWidgetOptions cellWidgetOptions{
        options.cellParent,
        options.compactPreview,
        options.showKoreanTeacherEnglishNames
    };

    for (int rowIndex = 0; rowIndex < model.rows.size(); ++rowIndex)
    {
        const ScheduleRowView& scheduleRow = model.rows[rowIndex];
        auto* timeItem =
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
    return metrics;
}
