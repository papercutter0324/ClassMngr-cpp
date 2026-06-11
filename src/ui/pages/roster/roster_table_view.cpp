#include "roster_table_view.h"

#include "ui/pages/roster/roster_column_layout_controller.h"

#include <QHeaderView>
#include <QPainter>

RosterTableView::RosterTableView(
    QWidget* parent
    )
    : QTableView(parent)
{
    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::SelectedClicked
        );

    verticalHeader()->setVisible(true);
    verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
}

void RosterTableView::setLayoutController(
    RosterColumnLayoutController* controller
    )
{
    m_controller = controller;
    viewport()->update();
}

void RosterTableView::paintEvent(
    QPaintEvent* event
    )
{
    QTableView::paintEvent(event);

    if (!model() || !m_controller)
    {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(
        QPen(QColor(55, 65, 81), 2)
        );

    for (int column = 0; column < model()->columnCount(); ++column)
    {
        if (!m_controller->isGroupBoundaryAfter(column))
        {
            continue;
        }

        const int x =
            columnViewportPosition(column)
            + columnWidth(column)
            - 1;

        painter.drawLine(
            x,
            0,
            x,
            viewport()->height()
            );
    }
}
