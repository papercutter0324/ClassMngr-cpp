#include "sidebar.h"
#include "sidebar_definitions.h"
#include "sidebar_marquee_delegate.h"
#include "sidebar_types.h"
#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"

#include <QAction>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyleOption>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>



namespace
{
class SidebarTreeStyle final : public QProxyStyle
{
public:
    explicit SidebarTreeStyle(
        QObject* parent = nullptr
        )
        : QProxyStyle()
    {
        setParent(parent);
    }

    void drawPrimitive(
        PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr
        ) const override
    {
        if (element == PE_IndicatorBranch && option)
        {
            if (option->state & QStyle::State_Children)
            {
                drawBranchArrow(
                    option,
                    painter,
                    widget
                    );
            }

            return;
        }

        QProxyStyle::drawPrimitive(
            element,
            option,
            painter,
            widget
            );
    }

private:
    void drawBranchArrow(
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget
        ) const
    {
        if (!painter)
        {
            return;
        }

        const int indicatorSize =
            QProxyStyle::pixelMetric(
                QStyle::PM_IndicatorWidth,
                option,
                widget
                );

        QRectF indicatorRect(
            0.0,
            0.0,
            indicatorSize + 2.0,
            indicatorSize + 2.0
            );
        indicatorRect.moveCenter(
            option->rect.center()
            );

        const bool enabled =
            option->state & QStyle::State_Enabled;
        const bool selected =
            option->state & QStyle::State_Selected;
        const bool hovered =
            option->state & QStyle::State_MouseOver;

        const QPalette::ColorGroup colorGroup =
            enabled
                ? (option->state & QStyle::State_Active
                       ? QPalette::Active
                       : QPalette::Inactive)
                : QPalette::Disabled;

        QColor arrowColor =
            option->palette.color(
                colorGroup,
                selected
                    ? QPalette::HighlightedText
                    : hovered
                        ? QPalette::Link
                        : QPalette::Text
                );

        if (!selected && !hovered)
        {
            arrowColor.setAlpha(180);
        }

        painter->save();
        painter->setRenderHint(
            QPainter::Antialiasing,
            true
            );

        if (hovered)
        {
            QColor backgroundColor =
                option->palette.color(
                    colorGroup,
                    QPalette::Highlight
                    );
            backgroundColor.setAlpha(42);

            painter->setPen(Qt::NoPen);
            painter->setBrush(backgroundColor);
            painter->drawRoundedRect(
                indicatorRect.adjusted(
                    1.0,
                    1.0,
                    -1.0,
                    -1.0
                    ),
                4.0,
                4.0
                );
        }

        const QPointF center =
            indicatorRect.center();
        const qreal halfWidth = 3.5;
        const qreal halfHeight = 3.5;

        QPainterPath arrowPath;

        if (option->state & QStyle::State_Open)
        {
            arrowPath.moveTo(
                center.x() - halfWidth,
                center.y() - 1.5
                );
            arrowPath.lineTo(
                center.x(),
                center.y() + 2.0
                );
            arrowPath.lineTo(
                center.x() + halfWidth,
                center.y() - 1.5
                );
        }
        else if (option->direction == Qt::RightToLeft)
        {
            arrowPath.moveTo(
                center.x() + 2.0,
                center.y() - halfHeight
                );
            arrowPath.lineTo(
                center.x() - 1.5,
                center.y()
                );
            arrowPath.lineTo(
                center.x() + 2.0,
                center.y() + halfHeight
                );
        }
        else
        {
            arrowPath.moveTo(
                center.x() - 1.5,
                center.y() - halfHeight
                );
            arrowPath.lineTo(
                center.x() + 2.0,
                center.y()
                );
            arrowPath.lineTo(
                center.x() - 1.5,
                center.y() + halfHeight
                );
        }

        painter->setBrush(Qt::NoBrush);
        painter->setPen(
            QPen(
                arrowColor,
                1.8,
                Qt::SolidLine,
                Qt::RoundCap,
                Qt::RoundJoin
                )
            );
        painter->drawPath(
            arrowPath
            );
        painter->restore();
    }
};

bool itemContainsCurrentSelection(
    QTreeWidgetItem* root,
    QTreeWidgetItem* current
    )
{
    while (current)
    {
        if (current == root)
        {
            return true;
        }

        current =
            current->parent();
    }

    return false;
}

void expandParents(
    QTreeWidgetItem* item
    )
{
    auto* parent =
        item
            ? item->parent()
            : nullptr;

    while (parent)
    {
        parent->setExpanded(true);
        parent =
            parent->parent();
    }
}
}



// =========================================================
// Constructor
// =========================================================

