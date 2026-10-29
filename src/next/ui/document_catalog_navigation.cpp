#include "next/ui/document_catalog_navigation.h"

#include <QCoreApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int roleValue(const DocumentCatalogItemRole role) noexcept
{
    return static_cast<int>(role);
}

struct Sibling final
{
    std::int32_t order = 0;
    std::string id;
    const Application::DocumentFolderMetadata* folder = nullptr;
    const Application::DocumentEntryMetadata* document = nullptr;
};

[[nodiscard]] QString displayText(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

void appendChildren(
    QTreeWidgetItem& parentItem,
    const std::string& parentPath,
    const std::unordered_map<std::string, std::vector<Sibling>>& childrenByPath
    )
{
    const auto found = childrenByPath.find(parentPath);
    if (found == childrenByPath.cend())
    {
        return;
    }

    std::vector<Sibling> siblings = found->second;
    std::sort(
        siblings.begin(),
        siblings.end(),
        [](const Sibling& left, const Sibling& right) {
            if (left.order != right.order)
            {
                return left.order < right.order;
            }
            return left.id < right.id;
        }
        );

    for (const Sibling& sibling : siblings)
    {
        if (sibling.folder)
        {
            auto* const item = new QTreeWidgetItem(
                &parentItem,
                QStringList{displayText(sibling.folder->displayName)}
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::Kind),
                static_cast<int>(DocumentCatalogItemKind::Folder)
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::FolderId),
                displayText(sibling.folder->id.value())
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::Order),
                sibling.folder->order.rawValue()
                );
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            appendChildren(
                *item,
                sibling.folder->path.value(),
                childrenByPath
                );
            item->setExpanded(true);
            continue;
        }

        if (sibling.document)
        {
            auto* const item = new QTreeWidgetItem(
                &parentItem,
                QStringList{displayText(sibling.document->displayName)}
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::Kind),
                static_cast<int>(DocumentCatalogItemKind::Document)
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::DocumentId),
                displayText(sibling.document->id.value())
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::FolderId),
                displayText(sibling.document->folderId.value())
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::Order),
                sibling.document->order.rawValue()
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::Printable),
                sibling.document->printable
                );
            item->setData(
                0,
                roleValue(DocumentCatalogItemRole::Exportable),
                sibling.document->exportable
                );
        }
    }
}

}  // namespace

QTreeWidgetItem* DocumentCatalogNavigation::append(
    QTreeWidget& tree,
    const Application::DocumentCatalogProjection& projection
    )
{
    auto* const root = new QTreeWidgetItem(
        &tree,
        QStringList{QCoreApplication::translate("MenuBuilder", "Documents")}
        );
    root->setData(
        0,
        roleValue(DocumentCatalogItemRole::Kind),
        static_cast<int>(DocumentCatalogItemKind::Root)
        );
    root->setData(0, Qt::UserRole + 2, QStringLiteral("documents_catalog"));
    root->setFlags(root->flags() & ~Qt::ItemIsSelectable);

    std::unordered_map<std::string, std::vector<Sibling>> childrenByPath;
    childrenByPath.reserve(projection.folders().size() + projection.documents().size());
    std::unordered_map<std::string, std::string> folderPathsById;
    folderPathsById.reserve(projection.folders().size());

    for (const Application::DocumentFolderMetadata& folder : projection.folders())
    {
        folderPathsById.emplace(folder.id.value(), folder.path.value());
        childrenByPath[folder.parentPath.value()].push_back(Sibling{
            .order = folder.order.rawValue(),
            .id = folder.id.value(),
            .folder = &folder
        });
    }

    for (const Application::DocumentEntryMetadata& document : projection.documents())
    {
        const auto folderPath = folderPathsById.find(document.folderId.value());
        if (folderPath == folderPathsById.cend())
        {
            // Projection validation guarantees the folder exists. Retain a
            // defensive guard so an invalid future caller cannot form a row.
            continue;
        }
        childrenByPath[folderPath->second].push_back(Sibling{
            .order = document.order.rawValue(),
            .id = document.id.value(),
            .document = &document
        });
    }

    appendChildren(*root, {}, childrenByPath);
    root->setExpanded(true);
    return root;
}

std::optional<Domain::DocumentId> DocumentCatalogNavigation::documentIdFor(
    const QTreeWidgetItem* item
    )
{
    if (!item
        || item->data(0, roleValue(DocumentCatalogItemRole::Kind)).toInt()
            != static_cast<int>(DocumentCatalogItemKind::Document))
    {
        return std::nullopt;
    }

    const QByteArray value = item->data(
        0,
        roleValue(DocumentCatalogItemRole::DocumentId)
        ).toString().toUtf8();
    return Domain::DocumentId::fromString(
        std::string(value.constData(), static_cast<std::size_t>(value.size()))
        );
}

}  // namespace ClassMngr::Next::Ui
