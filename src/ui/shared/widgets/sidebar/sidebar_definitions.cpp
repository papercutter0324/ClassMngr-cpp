#include "sidebar_definitions.h"

#include <QObject>



// =========================================================
// Tree Structure
// =========================================================

QList<TreeNodeSpec> treeStructure()
{
    return
    {
        {
            "my_info_information",
            QObject::tr("My Information"),
            NodeType::Page
        },

        {
            "my_info_schedule",
            QObject::tr("Schedule"),
            NodeType::Page
        },

        {
            "my_info_calendar",
            QObject::tr("Calendar"),
            NodeType::Page
        },

        {
            "classes",
            QObject::tr("Classes"),
            NodeType::Page
        },

        {
            "sub_prep",
            QObject::tr("Sub Prep"),
            NodeType::Page
        },

        {
            "co_teachers",
            QObject::tr("Co-Teachers"),
            NodeType::Root
        },

        {
            "campus_staff",
            QObject::tr("Campus Staff"),
            NodeType::Root,
            {
                {
                    "teachers_all_korean",
                    QObject::tr("Korean Teachers"),
                    NodeType::Root
                },
                {
                    "native_english_teachers",
                    QObject::tr("Native English Teachers"),
                    NodeType::Page
                },
                {
                    "gs_team",
                    QObject::tr("GS Team"),
                    NodeType::Page
                }
            }
        },

        {
            "useful_links",
            QObject::tr("Useful Links"),
            NodeType::Root,

            {
                {
                    "useful_dropbox",
                    QObject::tr("Dropbox"),
                    NodeType::Url,
                    {},
                    "https://www.dropbox.com"
                },

                {
                    "useful_vacation_calendar",
                    QObject::tr("Vacation Calendar"),
                    NodeType::Url,
                    {},
                    "https://docs.google.com/spreadsheets/d/14eL7sVctYRWXkyAFb7OePbHEdmZNTdxiv6dxd4Qo-fQ/"
                },

                {
                    "useful_yearly_calendar",
                    QObject::tr("Yearly Calendar"),
                    NodeType::Url,
                    {},
                    "https://docs.google.com/spreadsheets/d/18O05g7nlnsoUwrWhArZkptJFp3LMbSytdgKDjNaoMU4/edit"
                },

                {
                    "useful_training_website",
                    QObject::tr("Training Website"),
                    NodeType::Url,
                    {},
                    "https://sites.google.com/view/dybtraining/home"
                },

                {
                    "useful_net_website",
                    QObject::tr("NET Website"),
                    NodeType::Url,
                    {},
                    "https://sites.google.com/view/dybnet/home"
                },

                {
                    "useful_lms_website",
                    QObject::tr("LMS Website"),
                    NodeType::Url,
                    {},
                    "https://lms.choisun.co.kr/login/login.php"
                },

                {
                    "useful_highlights_library",
                    QObject::tr("Highlights Library"),
                    NodeType::Url,
                    {},
                    "https://library.highlights.com/member/login/?sType=t"
                }
            }
        },

        {
            "campus_info",
            QObject::tr("Campus Directory"),
            NodeType::Root,

            {
                {
                    "campus_information",
                    QObject::tr("Information"),
                    NodeType::Page
                },

                {
                    "campus_directions",
                    QObject::tr("Directions"),
                    NodeType::Page
                },

                {
                    "campus_address",
                    QObject::tr("Address"),
                    NodeType::Page
                },

                {
                    "campus_housing",
                    QObject::tr("Housing"),
                    NodeType::Page
                },

                {
                    "campus_map",
                    QObject::tr("Maps"),
                    NodeType::Page
                }
            }
        }
};
}
