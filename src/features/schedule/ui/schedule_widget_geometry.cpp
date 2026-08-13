#include "schedule_widget_geometry.h"

#include <QHeaderView>
#include <QScrollBar>
#include <QTableWidget>

ScheduleTableGeometry ScheduleWidgetGeometry::tableGeometry(
    const QTableWidget* table,
    int maximumVisibleRows
    )
{
    ScheduleTableGeometry geometry;
    if (!table)
    {
        return geometry;
    }

    const int rowCount = table->rowCount();
    geometry.hasHiddenRows =
        maximumVisibleRows > 0 && rowCount > maximumVisibleRows;
    const int visibleRows = geometry.hasHiddenRows
        ? maximumVisibleRows
        : rowCount;
    geometry.height =
        table->horizontalHeader()->height() + (table->frameWidth() * 2);
    for (int row = 0; row < visibleRows; ++row)
    {
        geometry.height += table->rowHeight(row);
    }
    if (
        const auto* scrollBar = table->horizontalScrollBar();
        scrollBar && scrollBar->isVisible()
        )
    {
        geometry.height += scrollBar->sizeHint().height();
    }
    return geometry;
}
