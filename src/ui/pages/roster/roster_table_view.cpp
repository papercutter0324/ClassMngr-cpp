#include "roster_table_view.h"

#include "ui/pages/roster/roster_column_layout_controller.h"

#include <QEvent>
#include <QHeaderView>
#include <QPainter>
#include <QPalette>

namespace
{

class RosterVerticalHeaderView : public QHeaderView
{
public:
    explicit RosterVerticalHeaderView(
        RosterTableView* table
        )
        : QHeaderView(Qt::Vertical, table),
          m_table(table)
    {
    }

protected:
    void paintEvent(
        QPaintEvent* event
        ) override
    {
        QHeaderView::paintEvent(event);

        if (!m_table || !m_table->viewport())
        {
            return;
        }

        const int bottomEdge =
            m_table->contentBottomEdge();

        if (bottomEdge < 0 || bottomEdge >= viewport()->height() - 1)
        {
            return;
        }

        QPainter painter(viewport());
        painter.fillRect(
            QRect(
                0,
                bottomEdge + 1,
                viewport()->width(),
                viewport()->height() - bottomEdge - 1
                ),
            m_table->viewport()->palette().brush(QPalette::Base)
            );
    }

private:
    RosterTableView* m_table = nullptr;
};

} // namespace

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
        | QAbstractItemView::AnyKeyPressed
        | QAbstractItemView::SelectedClicked
        );

    setVerticalHeader(
        new RosterVerticalHeaderView(this)
        );

    verticalHeader()->setVisible(true);
    verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    updateVerticalHeaderTrailingBackground();
}

void RosterTableView::setLayoutController(
    RosterColumnLayoutController* controller
    )
{
    m_controller = controller;
    viewport()->update();
}

void RosterTableView::changeEvent(
    QEvent* event
    )
{
    QTableView::changeEvent(event);

    if (
        event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange
        )
    {
        updateVerticalHeaderTrailingBackground();
    }
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

    const int bottomEdge =
        contentBottomEdge();

    if (bottomEdge < 0)
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
        if (m_controller->isGroupBoundaryBefore(column))
        {
            const int x =
                columnViewportPosition(column);

            painter.drawLine(
                x,
                0,
                x,
                bottomEdge
                );
        }

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
            bottomEdge
            );
    }
}

int RosterTableView::contentBottomEdge() const
{
    if (!model() || model()->rowCount() <= 0)
    {
        return -1;
    }

    const int lastRow =
        model()->rowCount() - 1;

    return rowViewportPosition(lastRow)
        + rowHeight(lastRow)
        - 1;
}

void RosterTableView::updateVerticalHeaderTrailingBackground()
{
    if (!verticalHeader() || !verticalHeader()->viewport())
    {
        return;
    }

    verticalHeader()->viewport()->update();
}
