#include "sidebar_p.h"

void Sidebar::addTeacherNode(
    const QString &displayName,
    int teacherId,
    bool myCoTeacher
    )
{
    auto* teachersRoot =
        m_nodes.value(
            QStringLiteral("teachers"),
            nullptr
            );

    auto* group =
        teachersRoot
            ? childWithKey(
                teachersRoot,
                myCoTeacher
                    ? QStringLiteral("teachers_mine")
                    : QStringLiteral("teachers_all_korean")
                )
            : nullptr;

    if (!group)
    {
        return;
    }

    auto *item =
        createItem(
            displayName,
            NodeType::Teacher,
            true,
            QStringLiteral("teacher")
            );

    item->setData(
        0,
        Qt::UserRole + 3,
        teacherId
        );

    group->addChild(item);

    m_teacherItems[teacherId].append(item);

    updateTreeColumnWidth();
}



// =========================================================
// Clear Teachers
// =========================================================

void Sidebar::clearTeachers()
{
    if (!m_nodes.contains("teachers"))
    {
        return;
    }

    m_nodes["teachers"]->takeChildren();

    m_teacherItems.clear();

    for (const auto& child : treeStructure())
    {
        if (child.key != QStringLiteral("teachers"))
        {
            continue;
        }

        for (const auto& group : child.children)
        {
            auto* groupItem =
                createItem(
                    group.label,
                    group.type,
                    false,
                    group.key
                    );

            m_nodes["teachers"]->addChild(groupItem);
        }
        break;
    }

    updateTreeColumnWidth();
}

void Sidebar::setAllKoreanTeachersVisible(
    bool visible
    )
{
    auto* teachersRoot =
        m_nodes.value(
            QStringLiteral("teachers"),
            nullptr
            );

    auto* allTeachers =
        teachersRoot
            ? childWithKey(
                teachersRoot,
                QStringLiteral("teachers_all_korean")
                )
            : nullptr;

    if (!allTeachers)
    {
        return;
    }

    if (
        !visible
        && itemContainsCurrentSelection(
            allTeachers,
            m_tree->currentItem()
            )
        )
    {
        m_tree->clearSelection();
    }

    allTeachers->setHidden(!visible);
    updateTreeColumnWidth();
}



// =========================================================
// Update Tree Column Width
// =========================================================

