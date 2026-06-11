#include "sidebar_definitions.h"

#include <QObject>



// =========================================================
// Tree Structure
// =========================================================

const QList<TreeNodeSpec>
    TREE_STRUCTURE =
    {
        {
            "my_info",
            QObject::tr("My Info"),
            NodeType::Root,

            {
                {
                    "",
                    QObject::tr("Schedule"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Sub Prep"),
                    NodeType::Page
                }
            }
        },

        {
            "classes",
            QObject::tr("Classes"),
            NodeType::Root
        },

        {
            "teachers",
            QObject::tr("Co-Teachers"),
            NodeType::Root
        },

        {
            "useful_links",
            QObject::tr("Useful Links"),
            NodeType::Root,

            {
                {
                    "",
                    QObject::tr("Dropbox"),
                    NodeType::Url,
                    {},
                    "https://www.dropbox.com"
                },

                {
                    "",
                    QObject::tr("Vacation Calendar"),
                    NodeType::Url,
                    {},
                    "https://docs.google.com"
                },

                {
                    "",
                    QObject::tr("Training Website"),
                    NodeType::Url,
                    {},
                    "https://sites.google.com"
                }
            }
        },

        {
            "campus_info",
            QObject::tr("Campus Info"),
            NodeType::Root,

            {
                {
                    "",
                    QObject::tr("Campus Directions"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Campus Information"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Campus Map"),
                    NodeType::Page
                }
            }
        }
};



// =========================================================
// Class Template
// =========================================================

const QList<TreeNodeSpec>
    CLASS_TEMPLATE =
    {
        {
            "",
            QObject::tr("Class Info"),
            NodeType::Page
        },

        {
            "",
            QObject::tr("Class Roster"),
            NodeType::Page
        },

        {
            "",
            QObject::tr("Student Evaluations"),
            NodeType::ClassSection,

            {
                {
                    "",
                    QObject::tr("Winter"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Speech Contest"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Summer"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Fall"),
                    NodeType::Page
                }
            }
        }
};
