#include "sidebar_p.h"

void Sidebar::addTeacherNode(
    const QString &displayName,
    int teacherId,
    bool myCoTeacher
    )
{
    auto* group = myCoTeacher
        ? m_nodes.value(QStringLiteral("co_teachers"), nullptr)
        : childWithKey(
            m_nodes.value(QStringLiteral("campus_staff"), nullptr),
            QStringLiteral("teachers_all_korean"));

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
    auto* coTeachers =
        m_nodes.value(QStringLiteral("co_teachers"), nullptr);
    auto* campusStaff =
        m_nodes.value(QStringLiteral("campus_staff"), nullptr);
    auto* koreanTeachers =
        childWithKey(campusStaff, QStringLiteral("teachers_all_korean"));

    if (!coTeachers || !koreanTeachers)
    {
        return;
    }

    coTeachers->takeChildren();
    koreanTeachers->takeChildren();

    m_teacherItems.clear();

    updateTreeColumnWidth();
}
