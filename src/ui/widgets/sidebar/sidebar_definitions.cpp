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
                    QObject::tr("My Information"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Class Schedule"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Monthly Calendar"),
                    NodeType::Page
                }
            }
        },

        {
            "sub_prep",
            QObject::tr("Sub Prep"),
            NodeType::Root,

            {
                {
                    "",
                    QObject::tr("Important Information"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Class Information"),
                    NodeType::Page
                },

                {
                    "",
                    QObject::tr("Sub Comments"),
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
                    "https://docs.google.com/spreadsheets/d/14eL7sVctYRWXkyAFb7OePbHEdmZNTdxiv6dxd4Qo-fQ/"
                },

                {
                    "",
                    QObject::tr("Yearly Calendar"),
                    NodeType::Url,
                    {},
                    "https://docs.google.com/spreadsheets/d/18O05g7nlnsoUwrWhArZkptJFp3LMbSytdgKDjNaoMU4/edit"
                },

                {
                    "",
                    QObject::tr("Training Website"),
                    NodeType::Url,
                    {},
                    "https://sites.google.com/view/dybtraining/home"
                },

                {
                    "",
                    QObject::tr("NET Website"),
                    NodeType::Url,
                    {},
                    "https://sites.google.com/view/dybnet/home"
                },

                {
                    "",
                    QObject::tr("LMS Website"),
                    NodeType::Url,
                    {},
                    "https://lms.choisun.co.kr/login/login.php"
                },

                {
                    "",
                    QObject::tr("Highlights Library"),
                    NodeType::Url,
                    {},
                    "https://library.highlights.com/member/login/?sType=t"
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
            QObject::tr("Class Notes"),
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
