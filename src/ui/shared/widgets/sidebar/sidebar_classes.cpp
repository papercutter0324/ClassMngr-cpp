#include "sidebar_p.h"

void Sidebar::addClassNode(
    const QString &displayName,
    int classId
    )
{
    auto* classListRoot =
        m_nodes.value(
            QStringLiteral("my_info_class_list"),
            nullptr
            );

    if (!classListRoot)
    {
        return;
    }

    auto *item =
        createItem(
            displayName,
            NodeType::Class,
            true,
            QStringLiteral("class")
            );

    item->setData(
        0,
        Qt::UserRole + 2,
        classId
        );



    // =====================================================
    // Add Template Children
    // =====================================================

    for (const auto &spec : classTemplate())
    {
        auto *child =
            createItem(
                spec.label,
                spec.type,
                spec.children.isEmpty(),
                spec.key
                );

        for (const auto &sub : spec.children)
        {
            auto *subChild =
                createItem(
                    sub.label,
                    sub.type,
                    true,
                    sub.key
                    );

            child->addChild(subChild);
        }

        item->addChild(child);
    }

    classListRoot->addChild(item);

    m_classItems[classId] = item;

    updateTreeColumnWidth();
}

void Sidebar::clearClasses()
{
    auto* classListRoot =
        m_nodes.value(
            QStringLiteral("my_info_class_list"),
            nullptr
            );

    if (!classListRoot)
    {
        return;
    }

    classListRoot->takeChildren();

    m_classItems.clear();

    updateTreeColumnWidth();
}

void Sidebar::selectClass(
    int classId
    )
{
    if (!m_classItems.contains(classId))
    {
        return;
    }

    auto *item =
        m_classItems[classId];

    expandParents(item);

    m_tree->setCurrentItem(item);

    m_tree->scrollToItem(item);
}



// =========================================================
// Get Selected Class ID
// =========================================================

int Sidebar::getSelectedClassId() const
{
    auto *item =
        m_tree->currentItem();

    while (item)
    {
        const int itemClassId =
            item->data(
                    0,
                    Qt::UserRole + 2
                    ).toInt();

        if (itemClassId > 0)
        {
            return itemClassId;
        }

        NodeType type =
            static_cast<NodeType>(
                item->data(
                        0,
                        Qt::UserRole
                        ).toInt()
                );

        if (type == NodeType::Class)
        {
            return item->data(
                           0,
                           Qt::UserRole + 2
                           ).toInt();
        }

        item = item->parent();
    }

    return -1;
}



// =========================================================
// Add Teacher
// =========================================================
