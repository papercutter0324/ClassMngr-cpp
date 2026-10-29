#pragma once

#include "next/application/document_catalog_projection.h"

#include <optional>

class QTreeWidget;
class QTreeWidgetItem;

namespace ClassMngr::Next::Ui
{

// Roles belong to the document-catalog branch rendered in the application
// sidebar. They keep identity and action metadata attached to leaves without
// making document bytes or viewer objects part of the navigation model.
enum class DocumentCatalogItemRole : int
{
    Kind = 0x0100 + 20,
    DocumentId,
    FolderId,
    Order,
    Printable,
    Exportable
};

enum class DocumentCatalogItemKind : int
{
    Root,
    Folder,
    Document
};

class DocumentCatalogNavigation final
{
public:
    // Appends one localized Documents root and its folder/document descendants
    // to the supplied tree. Sibling order follows metadata order, then stable
    // identifier, matching the Legacy catalog's deterministic ordering.
    static QTreeWidgetItem* append(
        QTreeWidget& tree,
        const Application::DocumentCatalogProjection& projection
        );

    [[nodiscard]] static std::optional<Domain::DocumentId> documentIdFor(
        const QTreeWidgetItem* item
        );
};

}  // namespace ClassMngr::Next::Ui
