#include "sidebar_p.h"

void Sidebar::buildTree()
{
    m_previousCurrentItem =
        nullptr;

    m_tree->clear();

    m_nodes.clear();
    m_classItems.clear();
    m_teacherItems.clear();



    // =====================================================
    // Build Top Level Nodes
    // =====================================================

    for (const auto &spec : treeStructure())
    {
        auto *item =
            createItem(
                spec
                );

        m_tree->addTopLevelItem(item);



        // =================================================
        // Key Registry
        // =================================================

        if (!spec.key.isEmpty())
        {
            m_nodes[spec.key] = item;
        }
    }

    updateTreeColumnWidth();
}

void Sidebar::rebuildTree()
{
    buildTree();
}

QStringList Sidebar::expandedRootKeys() const
{
    QStringList keys;

    for (
        auto it = m_nodes.cbegin();
        it != m_nodes.cend();
        ++it
        )
    {
        if (it.value() && it.value()->isExpanded())
        {
            keys.append(
                it.key()
                );
        }
    }

    return keys;
}

void Sidebar::restoreExpandedRootKeys(
    const QStringList& keys
    )
{
    for (
        auto it = m_nodes.cbegin();
        it != m_nodes.cend();
        ++it
        )
    {
        if (it.value())
        {
            it.value()->setExpanded(
                keys.contains(it.key())
                );
        }
    }

}

QStringList Sidebar::selectedKeys() const
{
    return getItemKeys(
        m_tree ? m_tree->currentItem() : nullptr
        );
}

void Sidebar::selectByKeys(
    const QStringList& keys,
    int classId,
    int teacherId
    )
{
    if (keys.isEmpty())
    {
        return;
    }

    if (teacherId > 0 && keys.contains(QStringLiteral("teacher")))
    {
        selectTeacher(teacherId);
        return;
    }

    QTreeWidgetItem* item =
        m_nodes.value(keys.first(), nullptr);

    if (!item)
    {
        return;
    }

    for (int index = 1; index < keys.size(); ++index)
    {
        const QString key =
            keys.at(index);

        if (key == QStringLiteral("class"))
        {
            item =
                m_classItems.value(classId, nullptr);
        }
        else if (
            key == QStringLiteral("class_roster")
            && classId > 0
            )
        {
            auto* rosterItem =
                childWithKeyAndClassId(
                    item,
                    key,
                    classId
                    );

            item =
                rosterItem
                    ? rosterItem
                    : childWithKey(
                        item,
                        key
                        );
        }
        else
        {
            item =
                childWithKey(
                    item,
                    key
                    );
        }

        if (!item)
        {
            return;
        }

        if (item->parent())
        {
            item->parent()->setExpanded(true);
        }
    }

    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
}



// =========================================================
// Create Item
// =========================================================

QTreeWidgetItem* Sidebar::createItem(
    const QString &label,
    NodeType type,
    bool selectable,
    const QString& key
    )
{
    auto *item =
        new QTreeWidgetItem();

    item->setText(
        0,
        label
        );

    item->setData(
        0,
        Qt::UserRole,
        std::to_underlying(type)
        );

    item->setData(
        0,
        Qt::UserRole + 4,
        key
        );

    if (!selectable)
    {
        item->setFlags(
            item->flags()
            & ~Qt::ItemIsSelectable
            );
    }

    return item;
}

QTreeWidgetItem* Sidebar::createItem(
    const TreeNodeSpec& spec
    )
{
    auto* item =
        createItem(
            spec.label,
            spec.type,
            spec.children.isEmpty(),
            spec.key
            );

    if (!spec.url.isEmpty())
    {
        item->setData(
            0,
            Qt::UserRole + 1,
            spec.url
            );
    }

    for (const auto& child : spec.children)
    {
        item->addChild(
            createItem(child)
            );
    }

    return item;
}



// =========================================================
// Add Class
// =========================================================
