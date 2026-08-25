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
    // Teacher Items
    // =====================================================

    if (type == NodeType::Teacher)
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

    else if (
        item == m_nodes.value(QStringLiteral("co_teachers"))
        || item == m_nodes.value(QStringLiteral("campus_staff"))
        )
    {
        addTeacherAction();
    }



    // =====================================================
    // Class Root
    // =====================================================

    else if (item == m_nodes["classes"])
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
