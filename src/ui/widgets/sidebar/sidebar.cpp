#include "sidebar.h"
#include "sidebar_definitions.h"
#include "sidebar_types.h"

#include <QDesktopServices>
#include <QMenu>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>



// =========================================================
// Constructor
// =========================================================

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    qRegisterMetaType<NavigationData>("NavigationData");

    setupUi();

    setupSignals();

    buildTree();
}



// =========================================================
// Setup UI
// =========================================================

void Sidebar::setupUi()
{
    auto *layout =
        new QVBoxLayout(this);

    m_tree =
        new QTreeWidget(this);

    m_tree->setHeaderHidden(true);

    m_tree->setUniformRowHeights(true);

    m_tree->setIndentation(12);

    layout->addWidget(m_tree);

    layout->setContentsMargins(
        5,
        8,
        5,
        18
        );
}



// =========================================================
// Setup Signals
// =========================================================

void Sidebar::setupSignals()
{
    connect(
        m_tree,
        &QTreeWidget::itemClicked,
        this,
        &Sidebar::onItemClicked
        );

    m_tree->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(
        m_tree,
        &QWidget::customContextMenuRequested,
        this,
        &Sidebar::showContextMenu
        );
}



// =========================================================
// Build Tree
// =========================================================

void Sidebar::buildTree()
{
    m_tree->clear();

    m_nodes.clear();



    // =====================================================
    // Build Top Level Nodes
    // =====================================================

    for (const auto &spec : TREE_STRUCTURE)
    {
        auto *item =
            createItem(
                spec.label,
                spec.type,
                spec.children.isEmpty()
                );



        // =================================================
        // Store URL
        // =================================================

        if (!spec.url.isEmpty())
        {
            item->setData(
                0,
                Qt::UserRole + 1,
                spec.url
                );
        }



        // =================================================
        // Children
        // =================================================

        for (const auto &child : spec.children)
        {
            auto *childItem =
                createItem(
                    child.label,
                    child.type,
                    child.children.isEmpty()
                    );

            if (!child.url.isEmpty())
            {
                childItem->setData(
                    0,
                    Qt::UserRole + 1,
                    child.url
                    );
            }

            item->addChild(childItem);
        }

        m_tree->addTopLevelItem(item);



        // =================================================
        // Key Registry
        // =================================================

        if (!spec.key.isEmpty())
        {
            m_nodes[spec.key] = item;
        }
    }
}



// =========================================================
// Create Item
// =========================================================

QTreeWidgetItem* Sidebar::createItem(
    const QString &label,
    NodeType type,
    bool selectable
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
        static_cast<int>(type)
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



// =========================================================
// Add Class
// =========================================================

void Sidebar::addClassNode(
    const QString &displayName,
    int classId
    )
{
    auto *item =
        createItem(
            displayName,
            NodeType::Class
            );

    item->setData(
        0,
        Qt::UserRole + 2,
        classId
        );



    // =====================================================
    // Add Template Children
    // =====================================================

    for (const auto &spec : CLASS_TEMPLATE)
    {
        auto *child =
            createItem(
                spec.label,
                spec.type,
                spec.children.isEmpty()
                );

        for (const auto &sub : spec.children)
        {
            auto *subChild =
                createItem(
                    sub.label,
                    sub.type
                    );

            child->addChild(subChild);
        }

        item->addChild(child);
    }

    m_nodes["classes"]->addChild(item);

    m_nodes["classes"]->setExpanded(true);

    m_classItems[classId] = item;
}



// =========================================================
// Clear Classes
// =========================================================

void Sidebar::clearClasses()
{
    if (!m_nodes.contains("classes"))
    {
        return;
    }

    m_nodes["classes"]->takeChildren();

    m_classItems.clear();
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

void Sidebar::addTeacherNode(
    const QString &displayName,
    int teacherId
    )
{
    auto *item =
        createItem(
            displayName,
            NodeType::Teacher
            );

    item->setData(
        0,
        Qt::UserRole + 3,
        teacherId
        );

    m_nodes["teachers"]->addChild(item);

    m_nodes["teachers"]->setExpanded(true);

    m_teacherItems[teacherId] = item;
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
}



// =========================================================
// Select Teacher
// =========================================================

void Sidebar::selectTeacher(
    int teacherId
    )
{
    if (!m_teacherItems.contains(teacherId))
    {
        return;
    }

    auto *item =
        m_teacherItems[teacherId];

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



    // =====================================================
    // Navigation Payload
    // =====================================================

    NavigationData data;

    data.path =
        getItemPath(item);

    data.type = type;



    // =====================================================
    // Class ID
    // =====================================================

    if (type == NodeType::Class)
    {
        data.classId =
            item->data(
                    0,
                    Qt::UserRole + 2
                           ).toInt();
    }
    else
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



// =========================================================
// Context Menu
// =========================================================

void Sidebar::showContextMenu(
    const QPoint &position
    )
{
    auto *item =
        m_tree->itemAt(position);

    QMenu menu(this);

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
        menu.addAction(
            tr("Add Class"),
            this,
            &Sidebar::addClassRequested
            );

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
        menu.addAction(
            tr("Add Teacher"),
            this,
            &Sidebar::addTeacherRequested
            );

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
        menu.addAction(
            tr("Add Teacher"),
            this,
            &Sidebar::addTeacherRequested
            );
    }



    // =====================================================
    // Empty Space
    // =====================================================

    else
    {
        menu.addAction(
            tr("Add Class"),
            this,
            &Sidebar::addClassRequested
            );

        menu.addAction(
            tr("Add Teacher"),
            this,
            &Sidebar::addTeacherRequested
            );
    }

    menu.exec(
        m_tree->viewport()
            ->mapToGlobal(position)
        );
}
