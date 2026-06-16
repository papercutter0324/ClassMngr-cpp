#pragma once

#include <QPersistentModelIndex>
#include <QStyledItemDelegate>
#include <QTimer>

class QTreeWidget;

class SidebarMarqueeDelegate : public QStyledItemDelegate
{
public:
    explicit SidebarMarqueeDelegate(
        QTreeWidget* tree,
        QObject* parent = nullptr
        );

    void setMarqueeEnabled(
        bool enabled
        );

    void resetMarquee();

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

private:
    static constexpr int ScrollGap = 32;

    void setHoveredIndex(
        const QModelIndex& index
        );

    void advanceMarquee();

    void updateTimer();

    void updateIndex(
        const QModelIndex& index
        ) const;

    bool isIndexOverflowing(
        const QModelIndex& index
        ) const;

    bool isOverflowing(
        const QStyleOptionViewItem& option
        ) const;

    QRect clippedTextRect(
        const QRect& textRect
        ) const;

    QTreeWidget* m_tree = nullptr;
    QTimer m_timer;
    QPersistentModelIndex m_hoveredIndex;
    int m_offset = 0;
    bool m_enabled = false;
};
