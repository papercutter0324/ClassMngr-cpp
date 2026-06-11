#include "roster_header_view.h"

#include "core/fontmanager.h"
#include "ui/pages/roster/roster_column_layout_controller.h"
#include "ui/pages/roster/roster_constants.h"

#include <QAbstractItemModel>
#include <QPainter>

RosterHeaderView::RosterHeaderView(
    Qt::Orientation orientation,
    QWidget* parent
    )
    : QHeaderView(orientation, parent)
{
    setFixedHeight(
        RosterUi::HeaderHeight
        );

    setDefaultAlignment(
        Qt::AlignCenter
        );

    setHighlightSections(false);
    setSectionsClickable(true);
    setSectionsMovable(false);
}

QSize RosterHeaderView::sizeHint() const
{
    QSize size =
        QHeaderView::sizeHint();

    size.setHeight(
        RosterUi::HeaderHeight
        );

    return size;
}

void RosterHeaderView::setLayoutController(
    RosterColumnLayoutController* controller
    )
{
    m_controller = controller;
    viewport()->update();
}

void RosterHeaderView::paintEvent(
    QPaintEvent* event
    )
{
    Q_UNUSED(event);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.fillRect(
        rect(),
        palette().window()
        );

    paintGroupRow(painter);
    paintColumnRow(painter);

    painter.setPen(
        QColor(55, 65, 81)
        );

    painter.drawLine(
        0,
        RosterUi::HeaderGroupsHeight - 1,
        width(),
        RosterUi::HeaderGroupsHeight - 1
        );

    painter.drawLine(
        0,
        RosterUi::HeaderHeight - 1,
        width(),
        RosterUi::HeaderHeight - 1
        );
}

void RosterHeaderView::paintGroupRow(
    QPainter& painter
    ) const
{
    if (!model() || !m_controller)
    {
        return;
    }

    painter.setFont(
        FontManager::getUiFont(
            11,
            QFont::DemiBold
            )
        );

    int column = 0;

    while (column < count())
    {
        const QString group =
            m_controller->columnGroup(column);

        const int start =
            m_controller->groupStart(column);

        const int end =
            m_controller->groupEnd(column);

        int x =
            sectionViewportPosition(start);

        int width = 0;

        for (int current = start; current <= end; ++current)
        {
            width += sectionSize(current);
        }

        QRect groupRect(
            x,
            0,
            width,
            RosterUi::HeaderGroupsHeight
            );

        const QColor color =
            RosterUi::groupColor(group);

        painter.fillRect(
            groupRect,
            color
            );

        painter.setPen(
            RosterUi::contrastTextColor(color)
            );

        painter.drawText(
            groupRect.adjusted(6, 0, -6, 0),
            Qt::AlignCenter,
            group
            );

        painter.setPen(
            QPen(QColor(55, 65, 81), 2)
            );

        painter.drawLine(
            groupRect.left(),
            groupRect.top(),
            groupRect.left(),
            RosterUi::HeaderHeight
            );

        painter.drawLine(
            groupRect.right(),
            groupRect.top(),
            groupRect.right(),
            RosterUi::HeaderHeight
            );

        column = end + 1;
    }
}

void RosterHeaderView::paintColumnRow(
    QPainter& painter
    ) const
{
    if (!model() || !m_controller)
    {
        return;
    }

    painter.setFont(
        FontManager::getUiFont(
            10,
            QFont::DemiBold
            )
        );

    for (int column = 0; column < count(); ++column)
    {
        const QString name =
            model()->headerData(
                column,
                Qt::Horizontal,
                Qt::DisplayRole
                ).toString();

        const QString group =
            m_controller->columnGroup(column);

        const QColor color =
            RosterUi::soften(
                RosterUi::groupColor(group),
                0.38
                );

        QRect columnRect(
            sectionViewportPosition(column),
            RosterUi::HeaderGroupsHeight,
            sectionSize(column),
            RosterUi::HeaderColumnHeight
            );

        painter.fillRect(
            columnRect,
            color
            );

        painter.setPen(
            RosterUi::contrastTextColor(color)
            );

        painter.drawText(
            columnRect.adjusted(4, 0, -4, 0),
            Qt::AlignCenter,
            name
            );

        painter.setPen(
            QColor(156, 163, 175)
            );

        painter.drawLine(
            columnRect.right(),
            columnRect.top(),
            columnRect.right(),
            columnRect.bottom()
            );

        if (m_controller->isGroupBoundaryAfter(column))
        {
            painter.setPen(
                QPen(QColor(55, 65, 81), 2)
                );

            painter.drawLine(
                columnRect.right(),
                0,
                columnRect.right(),
                RosterUi::HeaderHeight
                );
        }

        if (m_controller->isGroupBoundaryBefore(column))
        {
            painter.setPen(
                QPen(QColor(55, 65, 81), 2)
                );

            painter.drawLine(
                columnRect.left(),
                0,
                columnRect.left(),
                RosterUi::HeaderHeight
                );
        }
    }
}
