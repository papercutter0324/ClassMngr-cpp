#include "sidebar.h"
#include "sidebar_definitions.h"
#include "sidebar_marquee_delegate.h"
#include "sidebar_types.h"
#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"

#include <QDesktopServices>
#include <QFontMetrics>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyleOption>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>



namespace
{
class SidebarTreeStyle final : public QProxyStyle
{
public:
    explicit SidebarTreeStyle(
        QObject* parent = nullptr
        )
        : QProxyStyle()
    {
        setParent(parent);
    }

    void drawPrimitive(
        PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr
        ) const override
    {
        if (element == PE_IndicatorBranch && option)
        {
            if (option->state & QStyle::State_Children)
            {
                drawBranchArrow(
                    option,
                    painter,
                    widget
                    );
            }

            return;
        }

        QProxyStyle::drawPrimitive(
            element,
            option,
            painter,
            widget
            );
    }

private:
    void drawBranchArrow(
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget
        ) const
    {
        if (!painter)
        {
            return;
        }

        const int indicatorSize =
            QProxyStyle::pixelMetric(
                QStyle::PM_IndicatorWidth,
                option,
                widget
                );

        QRectF indicatorRect(
            0.0,
            0.0,
            indicatorSize + 2.0,
            indicatorSize + 2.0
            );
        indicatorRect.moveCenter(
            option->rect.center()
            );

        const bool enabled =
            option->state & QStyle::State_Enabled;
        const bool selected =
            option->state & QStyle::State_Selected;
        const bool hovered =
            option->state & QStyle::State_MouseOver;

        const QPalette::ColorGroup colorGroup =
            enabled
                ? (option->state & QStyle::State_Active
                       ? QPalette::Active
                       : QPalette::Inactive)
                : QPalette::Disabled;

        QColor arrowColor =
            option->palette.color(
                colorGroup,
                selected
                    ? QPalette::HighlightedText
                    : hovered
                        ? QPalette::Link
                        : QPalette::Text
                );

        if (!selected && !hovered)
        {
            arrowColor.setAlpha(180);
        }

        painter->save();
        painter->setRenderHint(
            QPainter::Antialiasing,
            true
            );

        if (hovered)
        {
            QColor backgroundColor =
                option->palette.color(
                    colorGroup,
                    QPalette::Highlight
                    );
            backgroundColor.setAlpha(42);

            painter->setPen(Qt::NoPen);
            painter->setBrush(backgroundColor);
            painter->drawRoundedRect(
                indicatorRect.adjusted(
                    1.0,
                    1.0,
                    -1.0,
                    -1.0
                    ),
                4.0,
                4.0
                );
        }

        const QPointF center =
            indicatorRect.center();
        const qreal halfWidth = 3.5;
        const qreal halfHeight = 3.5;

        QPainterPath arrowPath;

        if (option->state & QStyle::State_Open)
        {
            arrowPath.moveTo(
                center.x() - halfWidth,
                center.y() - 1.5
                );
            arrowPath.lineTo(
                center.x(),
                center.y() + 2.0
                );
            arrowPath.lineTo(
                center.x() + halfWidth,
                center.y() - 1.5
                );
        }
        else if (option->direction == Qt::RightToLeft)
        {
            arrowPath.moveTo(
                center.x() + 2.0,
                center.y() - halfHeight
                );
            arrowPath.lineTo(
                center.x() - 1.5,
                center.y()
                );
            arrowPath.lineTo(
                center.x() + 2.0,
                center.y() + halfHeight
                );
        }
        else
        {
            arrowPath.moveTo(
                center.x() - 1.5,
                center.y() - halfHeight
                );
            arrowPath.lineTo(
                center.x() + 2.0,
                center.y()
                );
            arrowPath.lineTo(
                center.x() - 1.5,
                center.y() + halfHeight
                );
        }

        painter->setBrush(Qt::NoBrush);
        painter->setPen(
            QPen(
                arrowColor,
                1.8,
                Qt::SolidLine,
                Qt::RoundCap,
                Qt::RoundJoin
                )
            );
        painter->drawPath(
            arrowPath
            );
        painter->restore();
    }
};

bool itemContainsCurrentSelection(
    QTreeWidgetItem* root,
    QTreeWidgetItem* current
    )
{
    while (current)
    {
        if (current == root)
        {
            return true;
        }

        current =
            current->parent();
    }

    return false;
}

void expandParents(
    QTreeWidgetItem* item
    )
{
    auto* parent =
        item
            ? item->parent()
            : nullptr;

    while (parent)
    {
        parent->setExpanded(true);
        parent =
            parent->parent();
    }
}
}



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
    m_tree->setStyle(
        new SidebarTreeStyle(m_tree)
        );

    m_tree->setObjectName(
        QStringLiteral("sidebarTree")
        );
    m_tree->setStyleSheet(
        QStringLiteral(
            "QTreeWidget { border-width: %1px; border-style: solid; "
            "border-radius: %2px; }"
            ).arg(
                UiConstants::MainWindow::SidebarFrameWidth
                ).arg(
                UiConstants::MainWindow::SidebarFrameRadius
                )
        );

    m_tree->setFont(
        FontManager::getUiFont(
            FontManager::stdEnglishFont
            )
        );

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

    connect(
        m_tree,
        &QTreeWidget::currentItemChanged,
        this,
        [this](
            QTreeWidgetItem*,
            QTreeWidgetItem* previous
            )
        {
            m_previousCurrentItem =
                previous;
        }
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

void Sidebar::addClassNode(
    const QString &displayName,
    int classId
    )
{
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

    m_nodes["classes"]->addChild(item);

    m_classItems[classId] = item;

    updateTreeColumnWidth();
}

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
    const QStringList databaseNodeKeys{
        QStringLiteral("my_info"),
        QStringLiteral("sub_prep"),
        QStringLiteral("classes"),
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
        item == m_nodes.value("my_info")
        || item == m_nodes.value("sub_prep")
        || item == m_nodes.value("campus_info")
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
    auto* myInfoRoot =
        m_nodes.value("my_info", nullptr);

    if (!myInfoRoot)
    {
        return;
    }

    myInfoRoot->setExpanded(true);

    auto* sectionItem =
        childWithKey(
            myInfoRoot,
            sectionKey
            );

    if (!sectionItem)
    {
        return;
    }

    const QSignalBlocker blocker(m_tree);
    m_tree->setCurrentItem(sectionItem);
    m_tree->scrollToItem(sectionItem);
}

void Sidebar::selectSubPrepSection(
    const QString& sectionKey
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
        childWithKey(
            subPrepRoot,
            sectionKey
            );

    if (!sectionItem)
    {
        return;
    }

    m_tree->setCurrentItem(sectionItem);
    m_tree->scrollToItem(sectionItem);
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

        if (
            child->data(
                    0,
                    Qt::UserRole + 4
                    ).toString()
            == QStringLiteral("class_info")
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
