#ifndef SIDEBAR_DEFINITIONS_H
#define SIDEBAR_DEFINITIONS_H

#include "sidebar_types.h"

#include <QList>
#include <QString>



// =========================================================
// Tree Node Spec
// =========================================================

struct TreeNodeSpec
{
    QString key;

    QString label;

    NodeType type;

    QList<TreeNodeSpec> children;

    QString url;

    TreeNodeSpec(
        const QString &key = "",
        const QString &label = "",
        NodeType type = NodeType::Page,
        const QList<TreeNodeSpec> &children = {},
        const QString &url = ""
        )
        :
        key(key),
        label(label),
        type(type),
        children(children),
        url(url)
    {
    }
};



// =========================================================
// Tree Definitions
// =========================================================

extern const QList<TreeNodeSpec>
    TREE_STRUCTURE;

extern const QList<TreeNodeSpec>
    CLASS_TEMPLATE;

#endif // SIDEBAR_DEFINITIONS_H