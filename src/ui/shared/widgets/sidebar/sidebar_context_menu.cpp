#include "sidebar_p.h"

void Sidebar::showContextMenu(
    const QPoint &position
    )
{
    auto *item =
        m_tree->itemAt(position);

    QMenu menu(this);

    const auto addClassAction =
        [this, &menu]()
        {
            auto* action =
                menu.addAction(
                    tr("Add Class"),
                    this,
                    &Sidebar::addClassRequested
                    );
            action->setEnabled(m_databaseSectionsVisible);
        };

    const auto addTeacherAction =
        [this, &menu]()
        {
            auto* action =
                menu.addAction(
                    tr("Add Teacher"),
                    this,
                    &Sidebar::addTeacherRequested
                    );
            action->setEnabled(m_databaseSectionsVisible);
        };

    NodeType type = NodeType::Root;

    if (item)
    {
        type =
            static_cast<NodeType>(
                item->data(
                        0,
                        Qt::UserRole
                        ).toInt()
                );
    }



    // =====================================================
    // Class Items
    // =====================================================

    if (isClassItem(item))
    {
        addClassAction();

        QTreeWidgetItem* classItem = item;

        while (
            classItem
            && static_cast<NodeType>(
                classItem->data(0, Qt::UserRole).toInt()) != NodeType::Class
            )
        {
            classItem = classItem->parent();
        }

        const int classId = classItem
            ? classItem->data(0, Qt::UserRole + 2).toInt()
            : -1;

        auto* exportAction = menu.addAction(tr("Export Class"));
        exportAction->setEnabled(
            m_databaseSectionsVisible && classId > 0);
        connect(exportAction, &QAction::triggered, this, [this, classId]()
        {
            emit exportClassRequested(classId);
        });

        menu.addAction(
            tr("Delete Class"),
            this,
            &Sidebar::deleteClassRequested
            );
    }



    // =====================================================
    // Teacher Items
    // =====================================================

    else if (type == NodeType::Teacher)
    {
        addTeacherAction();

        menu.addAction(
            tr("Delete Teacher"),
            this,
            &Sidebar::deleteTeacherRequested
            );
    }



    // =====================================================
    // Teacher Root
    // =====================================================

    else if (item == m_nodes["teachers"])
    {
        addTeacherAction();
    }



    // =====================================================
    // Class Root
    // =====================================================

    else if (
        item == m_nodes["classes"]
        || item == m_nodes["my_info_class_list"]
        )
    {
        addClassAction();
    }



    // =====================================================
    // Empty Space
    // =====================================================

    else
    {
        addClassAction();
        addTeacherAction();
    }

    menu.exec(
        m_tree->viewport()
            ->mapToGlobal(position)
        );
}
