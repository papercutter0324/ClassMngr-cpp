#pragma once

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>

class MarqueeItemDelegate : public QStyledItemDelegate
{
public:
    explicit MarqueeItemDelegate(
        QAbstractItemView* list,
        QObject* parent = nullptr
        )
        : QStyledItemDelegate(parent)
        , m_list(list)
    {
        m_timer.setInterval(30);

        connect(
            &m_timer,
            &QTimer::timeout,
            this,
            &MarqueeItemDelegate::advanceMarquee
            );

        if (m_list && m_list->viewport())
        {
            m_list->viewport()->setMouseTracking(true);
            m_list->viewport()->installEventFilter(this);
        }
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const bool scrolling =
            m_hoveredIndex == index
            && isOverflowing(opt);

        if (!scrolling)
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

        const QFontMetrics metrics(opt.font);
        const int textWidth =
            metrics.horizontalAdvance(opt.text);
        const int cycleWidth =
            textWidth + ScrollGap;

        if (cycleWidth <= 0)
        {
            return;
        }

        painter->save();
        painter->setClipRect(visibleTextRect);

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
        painter->setFont(opt.font);

        const int offset =
            m_offset % cycleWidth;
        const int baseline =
            textRect.top()
            + (textRect.height() + metrics.ascent() - metrics.descent()) / 2;

        int x =
            textRect.left() - offset;

        do
        {
            painter->drawText(
                QPoint(x, baseline),
                opt.text
                );

            x += cycleWidth;
        }
        while (x < visibleTextRect.right());

        painter->restore();
    }

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override
    {
        if (
            m_list
            && watched == m_list->viewport()
            )
        {
            if (event->type() == QEvent::MouseMove)
            {
                auto* mouseEvent =
                    static_cast<QMouseEvent*>(event);

                setHoveredIndex(
                    m_list->indexAt(
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

private:
    static constexpr int ScrollGap = 32;

    void setHoveredIndex(
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

    void advanceMarquee()
    {
        if (
            !m_hoveredIndex.isValid()
            || !isIndexOverflowing(m_hoveredIndex)
            )
        {
            m_timer.stop();
            m_offset = 0;
            updateIndex(m_hoveredIndex);
            return;
        }

        ++m_offset;

        if (m_offset > 100000)
        {
            m_offset = 0;
        }

        updateIndex(m_hoveredIndex);
    }

    void updateTimer()
    {
        if (
            m_hoveredIndex.isValid()
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

    void updateIndex(
        const QModelIndex& index
        ) const
    {
        if (
            !m_list
            || !m_list->viewport()
            || !index.isValid()
            )
        {
            return;
        }

        m_list->viewport()->update(
            m_list->visualRect(index)
            );
    }

    bool isIndexOverflowing(
        const QModelIndex& index
        ) const
    {
        if (
            !m_list
            || !index.isValid()
            )
        {
            return false;
        }

        QStyleOptionViewItem option;
        option.initFrom(
            m_list->viewport()
            );
        option.widget =
            m_list->viewport();
        option.rect =
            m_list->visualRect(index);

        initStyleOption(
            &option,
            index
            );

        return isOverflowing(option);
    }

    bool isOverflowing(
        const QStyleOptionViewItem& option
        ) const
    {
        if (
            !m_list
            || !m_list->viewport()
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

        return QFontMetrics(option.font).horizontalAdvance(option.text)
            > visibleTextRect.width();
    }

    QRect clippedTextRect(
        const QRect& textRect
        ) const
    {
        if (
            !m_list
            || !m_list->viewport()
            )
        {
            return textRect;
        }

        return textRect.intersected(
            m_list->viewport()->rect()
            );
    }

    QAbstractItemView* m_list = nullptr;
    QTimer m_timer;
    QPersistentModelIndex m_hoveredIndex;
    int m_offset = 0;
};

