#include "sidebar_p.h"

#include <algorithm>
#include <optional>

namespace
{
using DocumentCatalogProjection =
    ClassMngr::Next::Application::DocumentCatalogProjection;
using DocumentFolderId =
    ClassMngr::Next::Domain::DocumentFolderId;

struct OrderedDocumentNode
{
    int order = 0;
    QString id;
    TreeNodeSpec spec;
};

QList<TreeNodeSpec> documentChildren(
    const DocumentCatalogProjection& projection,
    const QString& parentPath = QString(),
    const std::optional<DocumentFolderId>& parentFolderId = std::nullopt
    )
{
    QList<OrderedDocumentNode> nodes;

    for (const auto& folder : projection.folders())
    {
        if (QString::fromUtf8(folder.parentPath.c_str()) != parentPath)
        {
            continue;
        }

        QList<TreeNodeSpec> children =
            documentChildren(
                projection,
                QString::fromUtf8(folder.path.c_str()),
                folder.id
                );

        if (children.isEmpty())
        {
            continue;
        }

        nodes.append({
            folder.order,
            QString::fromUtf8(folder.id.value().c_str()),
            {
                QString::fromUtf8(folder.key.c_str()),
                QString::fromUtf8(folder.displayName.c_str()),
                NodeType::Root,
                children
            }
        });
    }

    for (const auto& document : projection.documents())
    {
        if (!parentFolderId || document.folderId != *parentFolderId)
        {
            continue;
        }

        nodes.append({
            document.order,
            QString::fromUtf8(document.id.value().c_str()),
            {
                QString::fromUtf8(document.key.c_str()),
                QString::fromUtf8(document.displayName.c_str()),
                NodeType::Page
            }
        });
    }

    std::sort(
        nodes.begin(),
        nodes.end(),
        [](const OrderedDocumentNode& left, const OrderedDocumentNode& right)
        {
            return left.order != right.order
                ? left.order < right.order
                : left.id < right.id;
        }
        );

    QList<TreeNodeSpec> result;
    result.reserve(nodes.size());

    for (const auto& node : nodes)
    {
        result.append(node.spec);
    }

    return result;
}
}

void Sidebar::buildTree()
{
    m_previousCurrentItem =
        nullptr;

    m_tree->clear();

    m_nodes.clear();
    m_teacherItems.clear();



    // =====================================================
    // Build Top Level Nodes
    // =====================================================

    QList<TreeNodeSpec> structure =
        treeStructure();

    const QList<TreeNodeSpec> documents =
        documentChildren(
            m_documentCatalogProjection
            );

    auto documentsIt =
        std::find_if(
            structure.begin(),
            structure.end(),
            [](const TreeNodeSpec& spec)
            {
                return spec.key == QStringLiteral("document");
            }
            );

    if (documentsIt != structure.end())
    {
        structure.erase(documentsIt);
    }

    if (!documents.isEmpty())
    {
        const auto insertionPoint =
            std::find_if(
                structure.begin(),
                structure.end(),
                [](const TreeNodeSpec& spec)
                {
                    return spec.key == QStringLiteral("useful_links");
                }
                );

        structure.insert(
            insertionPoint,
            {
                QStringLiteral("document"),
                QObject::tr("Documents"),
                NodeType::Root,
                documents
            }
            );
    }

    for (const auto &spec : structure)
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

void Sidebar::setDocumentCatalog(
    ClassMngr::Next::Application::DocumentCatalogProjection projection,
    const QString& localeName
    )
{
    m_documentCatalogProjection =
        std::move(projection);
    m_documentLocaleName =
        localeName;

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
    int teacherId
    )
{
    if (keys.isEmpty())
    {
        return;
    }

    QStringList normalizedKeys = keys;

    if (
        normalizedKeys.first() == QStringLiteral("my_info_information")
        || normalizedKeys.first() == QStringLiteral("my_info_schedule")
        || normalizedKeys.first() == QStringLiteral("my_info_calendar")
        )
    {
        normalizedKeys[0] = QStringLiteral("my_workspace");
    }

    if (teacherId > 0 && normalizedKeys.contains(QStringLiteral("teacher")))
    {
        selectTeacher(teacherId);
        return;
    }

    QTreeWidgetItem* item =
        m_nodes.value(normalizedKeys.first(), nullptr);

    if (!item)
    {
        return;
    }

    for (int index = 1; index < normalizedKeys.size(); ++index)
    {
        const QString key =
            normalizedKeys.at(index);

        item =
            childWithKey(
                item,
                key
                );

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
