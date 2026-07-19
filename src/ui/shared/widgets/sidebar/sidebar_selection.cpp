#include "sidebar_p.h"

void Sidebar::selectTeacher(
    int teacherId
    )
{
    const auto items =
        m_teacherItems.value(teacherId);

    if (items.isEmpty())
    {
        return;
    }

    auto *item =
        items.first();

    expandParents(item);

    m_tree->setCurrentItem(item);

    m_tree->scrollToItem(item);
}



// =========================================================
// Get Selected Teacher ID
// =========================================================

int Sidebar::getSelectedTeacherId() const
{
    auto *item =
        m_tree->currentItem();

    if (!item)
    {
        return -1;
    }

    NodeType type =
        static_cast<NodeType>(
            item->data(
                    0,
                    Qt::UserRole
                    ).toInt()
            );

    if (type != NodeType::Teacher)
    {
        return -1;
    }

    return item->data(
                   0,
                   Qt::UserRole + 3
                   ).toInt();
}



// =========================================================
// Database-Backed Sections
// =========================================================

void Sidebar::setDatabaseSectionsVisible(
    bool visible
    )
{
    m_databaseSectionsVisible = visible;

    const QStringList databaseNodeKeys{
        QStringLiteral("my_info_information"),
        QStringLiteral("my_info_schedule"),
        QStringLiteral("my_info_calendar"),
        QStringLiteral("my_info_class_list"),
        QStringLiteral("my_info_class_roster"),
        QStringLiteral("sub_prep"),
        QStringLiteral("classes"),
        QStringLiteral("speaking_evaluations"),
        QStringLiteral("teachers")
    };

    bool currentSelectionHidden = false;

    for (const QString& key : databaseNodeKeys)
    {
        auto* item =
            m_nodes.value(key, nullptr);

        if (!item)
        {
            continue;
        }

        if (
            !visible
            && itemContainsCurrentSelection(
                item,
                m_tree->currentItem()
                )
            )
        {
            currentSelectionHidden = true;
        }

        item->setHidden(!visible);
    }

    if (currentSelectionHidden)
    {
        m_tree->clearSelection();
    }

    updateTreeColumnWidth();
}



// =========================================================
// Item Clicked
// =========================================================

void Sidebar::onItemClicked(
    QTreeWidgetItem *item,
    int column
    )
{
    Q_UNUSED(column);



    // =====================================================
    // Node Type
    // =====================================================

    NodeType type =
        static_cast<NodeType>(
            item->data(
                    0,
                    Qt::UserRole
                    ).toInt()
            );


    // =====================================================
    // Expand / Collapse Groups
    // =====================================================

    if (
        item == m_nodes.value("campus_info")
        )
    {
        const bool previousSelectionInsideGroup =
            itemContainsCurrentSelection(
                item,
                m_previousCurrentItem
                );

        const bool expanded =
            !item->isExpanded();

        item->setExpanded(
            expanded
            );

        if (!expanded && previousSelectionInsideGroup)
        {
            m_tree->clearSelection();
            return;
        }

        NavigationData data;
        data.path =
            getItemPath(item);
        data.keys =
            getItemKeys(item);
        data.routeKey =
            data.keys.isEmpty()
                ? QString()
                : data.keys.last();
        data.type =
            type;

        emit itemSelected(data);
        return;
    }

    if (
        type == NodeType::Root
        || type == NodeType::ClassSection
        )
    {
        if (item->childCount() > 0)
        {
            item->setExpanded(
                !item->isExpanded()
                );
        }

        m_tree->clearSelection();

        return;
    }



    // =====================================================
    // URL
    // =====================================================

    if (type == NodeType::Url)
    {
        QString url =
            item->data(
                    0,
                    Qt::UserRole + 1
                    ).toString();

        if (!url.isEmpty())
        {
            QDesktopServices::openUrl(
                QUrl(url)
                );
        }

        m_tree->clearSelection();

        return;
    }

    if (type == NodeType::Class)
    {
        item->setExpanded(true);

        if (auto* classInfoItem = classDetailsChildForClass(item))
        {
            item =
                classInfoItem;

            m_tree->setCurrentItem(item);
            m_tree->scrollToItem(item);

            type =
                static_cast<NodeType>(
                    item->data(
                            0,
                            Qt::UserRole
                            ).toInt()
                    );
        }
    }



    // =====================================================
    // Navigation Payload
    // =====================================================

    NavigationData data;

    data.path =
        getItemPath(item);
    data.keys =
        getItemKeys(item);
    data.routeKey =
        data.keys.isEmpty()
            ? QString()
            : data.keys.last();

    data.type = type;



    // =====================================================
    // Class ID
    // =====================================================

    const int directClassId =
        item->data(
                0,
                Qt::UserRole + 2
                ).toInt();

    if (directClassId > 0)
    {
        data.classId =
            directClassId;
    }

    if (data.classId <= 0 && type == NodeType::Class)
    {
        data.classId =
            item->data(
                    0,
                    Qt::UserRole + 2
                           ).toInt();
    }
    else if (data.classId <= 0)
    {
        auto* parent =
            item->parent();

        while (parent)
        {
            NodeType parentType =
                static_cast<NodeType>(
                    parent->data(
                            0,
                            Qt::UserRole
                            ).toInt()
                    );

            if (parentType == NodeType::Class)
            {
                data.classId =
                    parent->data(
                            0,
                            Qt::UserRole + 2
                            ).toInt();

                break;
            }

            parent =
                parent->parent();
        }
    }



    // =====================================================
    // Teacher ID
    // =====================================================

    if (type == NodeType::Teacher)
    {
        data.teacherId =
            item->data(
                    0,
                    Qt::UserRole + 3
                    ).toInt();
    }

    emit itemSelected(data);
}

void Sidebar::selectMyInfoSection(
    const QString& sectionKey
    )
{
    if (auto* topLevelSection = m_nodes.value(sectionKey, nullptr))
    {
        const QSignalBlocker blocker(m_tree);
        m_tree->setCurrentItem(topLevelSection);
        m_tree->scrollToItem(topLevelSection);
    }
}

void Sidebar::selectCampusSection(
    const QString& sectionKey
    )
{
    auto* campusRoot =
        m_nodes.value("campus_info", nullptr);

    if (!campusRoot)
    {
        return;
    }

    campusRoot->setExpanded(true);

    auto* sectionItem =
        childWithKey(
            campusRoot,
            sectionKey
            );

    if (!sectionItem)
    {
        return;
    }

    m_tree->setCurrentItem(sectionItem);
    m_tree->scrollToItem(sectionItem);
}



// =========================================================
// Item Path
// =========================================================

QStringList Sidebar::getItemPath(
    QTreeWidgetItem *item
    ) const
{
    QStringList path;

    while (item)
    {
        path.prepend(
            item->text(0)
            );

        item = item->parent();
    }

    return path;
}

QStringList Sidebar::getItemKeys(
    QTreeWidgetItem* item
    ) const
{
    QStringList keys;

    while (item)
    {
        const QString key =
            item->data(
                    0,
                    Qt::UserRole + 4
                    ).toString();

        if (!key.isEmpty())
        {
            keys.prepend(key);
        }

        item = item->parent();
    }

    return keys;
}



// =========================================================
// Is Class Item
// =========================================================

bool Sidebar::isClassItem(
    QTreeWidgetItem *item
    ) const
{
    while (item)
    {
        NodeType type =
            static_cast<NodeType>(
                item->data(
                        0,
                        Qt::UserRole
                        ).toInt()
                );

        if (type == NodeType::Class)
        {
            return true;
        }

        item = item->parent();
    }

    return false;
}

QTreeWidgetItem* Sidebar::classDetailsChildForClass(
    QTreeWidgetItem* classItem
    ) const
{
    if (!classItem)
    {
        return nullptr;
    }

    QTreeWidgetItem* fallbackPageItem =
        nullptr;

    for (int i = 0; i < classItem->childCount(); ++i)
    {
        auto* child =
            classItem->child(i);

        if (!child)
        {
            continue;
        }

        const NodeType type =
            static_cast<NodeType>(
                child->data(
                        0,
                        Qt::UserRole
                        ).toInt()
                );

        if (type != NodeType::Page)
        {
            continue;
        }

        if (!fallbackPageItem)
        {
            fallbackPageItem =
                child;
        }

        if (
            child->data(
                    0,
                    Qt::UserRole + 4
                    ).toString()
            == QStringLiteral("class_details")
            )
        {
            return child;
        }
    }

    return fallbackPageItem;
}

QTreeWidgetItem* Sidebar::childWithKey(
    QTreeWidgetItem* item,
    const QString& key
    ) const
{
    if (!item)
    {
        return nullptr;
    }

    for (int index = 0; index < item->childCount(); ++index)
    {
        auto* child =
            item->child(index);

        if (
            child
            && child->data(
                    0,
                    Qt::UserRole + 4
                    ).toString() == key
            )
        {
            return child;
        }
    }

    return nullptr;
}

QTreeWidgetItem* Sidebar::childWithKeyAndClassId(
    QTreeWidgetItem* item,
    const QString& key,
    int classId
    ) const
{
    if (!item || classId <= 0)
    {
        return nullptr;
    }

    for (int index = 0; index < item->childCount(); ++index)
    {
        auto* child =
            item->child(index);

        if (
            child
            && child->data(
                    0,
                    Qt::UserRole + 4
                    ).toString() == key
            && child->data(
                    0,
                    Qt::UserRole + 2
                    ).toInt() == classId
            )
        {
            return child;
        }
    }

    return nullptr;
}

QTreeWidgetItem* Sidebar::childWithText(
    QTreeWidgetItem* item,
    const QString& text
    ) const
{
    if (!item)
    {
        return nullptr;
    }

    for (int index = 0; index < item->childCount(); ++index)
    {
        auto* child =
            item->child(index);

        if (child && child->text(0) == text)
        {
            return child;
        }
    }

    return nullptr;
}



// =========================================================
// Context Menu
// =========================================================
