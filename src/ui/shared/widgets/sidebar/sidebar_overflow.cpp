#include "sidebar_p.h"

int Sidebar::defaultWidthForTopLevelLabels() const
{
    if (!m_tree)
    {
        return UiConstants::MainWindow::SidebarStartupLabelPadding;
    }

    const QFontMetrics metrics(m_tree->font());
    int longestLabelWidth = 0;

    for (int index = 0;
         index < m_tree->topLevelItemCount();
         ++index)
    {
        const QTreeWidgetItem* item =
            m_tree->topLevelItem(index);

        if (item)
        {
            longestLabelWidth = qMax(
                longestLabelWidth,
                metrics.horizontalAdvance(item->text(0))
                );
        }
    }

    return longestLabelWidth
        + UiConstants::MainWindow::SidebarStartupLabelPadding;
}

void Sidebar::updateTreeColumnWidth()
{
    if (!m_tree)
    {
        return;
    }

    m_tree->resizeColumnToContents(0);

    updateOverflowTooltips();
}



// =========================================================
// Update Overflow Tooltips
// =========================================================

void Sidebar::updateOverflowTooltips()
{
    if (!m_tree)
    {
        return;
    }

    auto* root =
        m_tree->invisibleRootItem();

    if (!root)
    {
        return;
    }

    for (int i = 0; i < root->childCount(); ++i)
    {
        updateItemOverflowTooltips(
            root->child(i)
            );
    }
}

void Sidebar::updateItemOverflowTooltips(
    QTreeWidgetItem* item
    )
{
    if (!item)
    {
        return;
    }

    if (
        m_overflowTooltipsEnabled
        && isItemTextOverflowing(item)
        )
    {
        item->setToolTip(
            0,
            item->text(0)
            );
    }
    else
    {
        item->setToolTip(
            0,
            QString()
            );
    }

    for (int i = 0; i < item->childCount(); ++i)
    {
        updateItemOverflowTooltips(
            item->child(i)
            );
    }
}

bool Sidebar::isItemTextOverflowing(
    QTreeWidgetItem* item
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        || !item
        || item->text(0).isEmpty()
        )
    {
        return false;
    }

    const int textWidth =
        m_marqueeDelegate
            ? m_marqueeDelegate->textWidth(
                  item->text(0)
                  )
            : QFontMetrics(
                  m_tree->font()
                  ).horizontalAdvance(
                      item->text(0)
                      );

    const QRect itemRect =
        m_tree->visualItemRect(item);

    const int textLeft =
        itemRect.isValid()
            ? qMax(0, itemRect.left())
            : itemDepth(item) * m_tree->indentation() + 24;

    const int availableWidth =
        m_tree->viewport()->width()
        - textLeft
        - 8;

    return textWidth > availableWidth;
}

int Sidebar::itemDepth(
    QTreeWidgetItem* item
    ) const
{
    int depth = 0;

    if (!item)
    {
        return depth;
    }

    auto* parent =
        item->parent();

    while (parent)
    {
        ++depth;
        parent =
            parent->parent();
    }

    return depth;
}



// =========================================================
// Select Teacher
// =========================================================

