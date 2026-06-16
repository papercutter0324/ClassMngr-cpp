#include "sidebar.h"
#include "sidebar_definitions.h"
#include "sidebar_marquee_delegate.h"
#include "sidebar_types.h"

#include <QDesktopServices>
#include <QFontMetrics>
#include <QHeaderView>
#include <QMenu>
#include <QResizeEvent>
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
// Overflow Display
// =========================================================

void Sidebar::setOverflowTooltipsEnabled(
    bool enabled
    )
{
    if (m_overflowTooltipsEnabled == enabled)
    {
        return;
    }

    m_overflowTooltipsEnabled = enabled;

    updateOverflowTooltips();
}

void Sidebar::setOverflowMarqueeEnabled(
    bool enabled
    )
{
    if (m_overflowMarqueeEnabled == enabled)
    {
        return;
    }

    m_overflowMarqueeEnabled = enabled;

    if (m_marqueeDelegate)
    {
        m_marqueeDelegate->setMarqueeEnabled(
            enabled
            );
    }
}



// =========================================================
// Resize
// =========================================================

void Sidebar::resizeEvent(
    QResizeEvent* event
    )
{
    QWidget::resizeEvent(event);

    updateOverflowTooltips();

    if (m_marqueeDelegate)
    {
        m_marqueeDelegate->resetMarquee();
    }
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

    m_tree->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );

    m_tree->setTextElideMode(
        Qt::ElideNone
        );

    m_tree->setWordWrap(false);

    m_tree->header()->setSectionResizeMode(
        0,
        QHeaderView::ResizeToContents
        );

    m_tree->header()->setStretchLastSection(false);

    m_marqueeDelegate =
        new SidebarMarqueeDelegate(
            m_tree,
            m_tree
            );

    m_tree->setItemDelegate(
        m_marqueeDelegate
        );

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

    connect(
        m_tree,
        &QTreeWidget::itemExpanded,
        this,
        [this](QTreeWidgetItem*)
        {
            updateTreeColumnWidth();

            if (m_marqueeDelegate)
            {
                m_marqueeDelegate->resetMarquee();
            }
        }
        );

    connect(
        m_tree,
        &QTreeWidget::itemCollapsed,
        this,
        [this](QTreeWidgetItem*)
        {
            updateTreeColumnWidth();

            if (m_marqueeDelegate)
            {
                m_marqueeDelegate->resetMarquee();
            }
        }
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

    updateTreeColumnWidth();
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

    updateTreeColumnWidth();
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

    updateTreeColumnWidth();
}



// =========================================================
// Update Tree Column Width
// =========================================================

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

    const QFontMetrics metrics(
        m_tree->font()
        );

    const int textWidth =
        metrics.horizontalAdvance(
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
        item == m_nodes.value("my_info")
        || item == m_nodes.value("sub_prep")
        )
    {
        const bool expanded =
            !item->isExpanded();

        item->setExpanded(
            expanded
            );

        if (!expanded)
        {
            m_tree->clearSelection();
            return;
        }

        NavigationData data;
        data.path =
            getItemPath(item);
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

        if (auto* classInfoItem = classInfoChildForClass(item))
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

void Sidebar::selectMyInfoSection(
    const QString& sectionName
    )
{
    auto* myInfoRoot =
        m_nodes.value("my_info", nullptr);

    if (!myInfoRoot)
    {
        return;
    }

    myInfoRoot->setExpanded(true);

    auto* sectionItem =
        childWithText(
            myInfoRoot,
            sectionName
            );

    if (!sectionItem)
    {
        return;
    }

    m_tree->setCurrentItem(sectionItem);
    m_tree->scrollToItem(sectionItem);
}

void Sidebar::selectSubPrepSection(
    const QString& sectionName
    )
{
    auto* subPrepRoot =
        m_nodes.value("sub_prep", nullptr);

    if (!subPrepRoot)
    {
        return;
    }

    subPrepRoot->setExpanded(true);

    auto* sectionItem =
        childWithText(
            subPrepRoot,
            sectionName
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

QTreeWidgetItem* Sidebar::classInfoChildForClass(
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

        if (child->text(0) == tr("Class Info"))
        {
            return child;
        }
    }

    return fallbackPageItem;
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
    // Class Root
    // =====================================================

    else if (item == m_nodes["classes"])
    {
        menu.addAction(
            tr("Add Class"),
            this,
            &Sidebar::addClassRequested
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
