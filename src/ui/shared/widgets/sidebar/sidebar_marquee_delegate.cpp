#include "sidebar_marquee_delegate.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTreeWidget>

SidebarMarqueeDelegate::SidebarMarqueeDelegate(
    QTreeWidget* tree,
    QObject* parent
    )
    : QStyledItemDelegate(parent)
    , m_tree(tree)
{
    m_timer.setInterval(30);

    connect(
        &m_timer,
        &QTimer::timeout,
        this,
        &SidebarMarqueeDelegate::advanceMarquee
        );

    if (m_tree && m_tree->viewport())
    {
        m_tree->viewport()->setMouseTracking(true);
        m_tree->viewport()->installEventFilter(this);
    }
}

void SidebarMarqueeDelegate::setMarqueeEnabled(
    bool enabled
    )
{
    if (m_enabled == enabled)
    {
        return;
    }

    m_enabled = enabled;
    resetMarquee();
}

void SidebarMarqueeDelegate::resetMarquee()
{
    const QModelIndex oldIndex =
        m_hoveredIndex;

    m_timer.stop();
    m_offset = 0;
    m_hoveredIndex = QPersistentModelIndex();

    updateIndex(oldIndex);
}

void SidebarMarqueeDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QModelIndex hoveredIndex =
        m_hoveredIndex;

    if (
        !m_enabled
        || hoveredIndex != index
        || !isOverflowing(opt)
        )
    {
        QStyledItemDelegate::paint(
            painter,
            option,
            index
            );
        return;
    }

    const QWidget* widget =
        opt.widget;

    QStyle* style =
        widget
            ? widget->style()
            : QApplication::style();

    QStyleOptionViewItem backgroundOpt(opt);
    backgroundOpt.text.clear();
    backgroundOpt.features &=
        ~QStyleOptionViewItem::HasDisplay;

    style->drawControl(
        QStyle::CE_ItemViewItem,
        &backgroundOpt,
        painter,
        widget
        );

    const QRect textRect =
        style->subElementRect(
            QStyle::SE_ItemViewItemText,
            &opt,
            widget
            );

    const QRect visibleTextRect =
        clippedTextRect(textRect);

    if (!visibleTextRect.isValid())
    {
        return;
    }

    const int textWidth =
        opt.fontMetrics.horizontalAdvance(opt.text);

    if (textWidth <= visibleTextRect.width())
    {
        return;
    }

    painter->save();

    painter->setClipRect(
        visibleTextRect
        );

    painter->setFont(
        opt.font
        );

    const QPalette::ColorGroup colorGroup =
        (opt.state & QStyle::State_Enabled)
            ? ((opt.state & QStyle::State_Active)
                   ? QPalette::Active
                   : QPalette::Inactive)
            : QPalette::Disabled;

    const QPalette::ColorRole colorRole =
        (opt.state & QStyle::State_Selected)
            ? QPalette::HighlightedText
            : QPalette::Text;

    painter->setPen(
        opt.palette.color(
            colorGroup,
            colorRole
            )
        );

    Qt::Alignment alignment =
        opt.displayAlignment;

    if (!(alignment & Qt::AlignHorizontal_Mask))
    {
        alignment |= Qt::AlignLeft;
    }

    if (!(alignment & Qt::AlignVertical_Mask))
    {
        alignment |= Qt::AlignVCenter;
    }

    const int cycleWidth =
        textWidth + ScrollGap;

    const int offset =
        cycleWidth > 0
            ? m_offset % cycleWidth
            : 0;

    int x =
        textRect.left() - offset;

    while (x < visibleTextRect.right())
    {
        painter->drawText(
            QRect(
                x,
                textRect.top(),
                textWidth,
                textRect.height()
                ),
            alignment,
            opt.text
            );

        x += cycleWidth;
    }

    painter->restore();
}

bool SidebarMarqueeDelegate::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        m_tree
        && watched == m_tree->viewport()
        )
    {
        if (event->type() == QEvent::MouseMove)
        {
            auto* mouseEvent =
                static_cast<QMouseEvent*>(event);

            setHoveredIndex(
                m_tree->indexAt(
                    mouseEvent->pos()
                    )
                );
        }
        else if (event->type() == QEvent::Leave)
        {
            setHoveredIndex(
                QModelIndex()
                );
        }
    }

    return QStyledItemDelegate::eventFilter(
        watched,
        event
        );
}

void SidebarMarqueeDelegate::setHoveredIndex(
    const QModelIndex& index
    )
{
    const QModelIndex oldIndex =
        m_hoveredIndex;

    if (oldIndex == index)
    {
        return;
    }

    m_hoveredIndex =
        index;

    m_offset = 0;

    updateIndex(oldIndex);
    updateIndex(index);

    updateTimer();
}

void SidebarMarqueeDelegate::advanceMarquee()
{
    if (
        !m_enabled
        || !m_hoveredIndex.isValid()
        )
    {
        m_timer.stop();
        return;
    }

    const QModelIndex index =
        m_hoveredIndex;

    if (!isIndexOverflowing(index))
    {
        m_timer.stop();
        m_offset = 0;
        updateIndex(index);
        return;
    }

    ++m_offset;

    if (m_offset > 100000)
    {
        m_offset = 0;
    }

    updateIndex(index);
}

void SidebarMarqueeDelegate::updateTimer()
{
    if (
        m_enabled
        && m_hoveredIndex.isValid()
        && isIndexOverflowing(m_hoveredIndex)
        )
    {
        if (!m_timer.isActive())
        {
            m_timer.start();
        }
    }
    else
    {
        m_timer.stop();
    }
}

void SidebarMarqueeDelegate::updateIndex(
    const QModelIndex& index
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        || !index.isValid()
        )
    {
        return;
    }

    m_tree->viewport()->update(
        m_tree->visualRect(index)
        );
}

bool SidebarMarqueeDelegate::isIndexOverflowing(
    const QModelIndex& index
    ) const
{
    if (
        !m_tree
        || !index.isValid()
        )
    {
        return false;
    }

    QStyleOptionViewItem option;
    option.initFrom(
        m_tree->viewport()
        );
    option.widget =
        m_tree->viewport();
    option.rect =
        m_tree->visualRect(index);

    initStyleOption(
        &option,
        index
        );

    return isOverflowing(option);
}

bool SidebarMarqueeDelegate::isOverflowing(
    const QStyleOptionViewItem& option
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        || option.text.isEmpty()
        )
    {
        return false;
    }

    const QWidget* widget =
        option.widget;

    QStyle* style =
        widget
            ? widget->style()
            : QApplication::style();

    const QRect textRect =
        style->subElementRect(
            QStyle::SE_ItemViewItemText,
            &option,
            widget
            );

    const QRect visibleTextRect =
        clippedTextRect(textRect);

    if (!visibleTextRect.isValid())
    {
        return false;
    }

    return option.fontMetrics.horizontalAdvance(
               option.text
               )
           > visibleTextRect.width();
}

QRect SidebarMarqueeDelegate::clippedTextRect(
    const QRect& textRect
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        )
    {
        return textRect;
    }

    return textRect.intersected(
        m_tree->viewport()->rect()
        );
}
