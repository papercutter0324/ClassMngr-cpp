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
            "my_info",
            QObject::tr("My Info"),
            NodeType::Root,

            {
                {
                    "my_info_information",
                    QObject::tr("My Information"),
                    NodeType::Page
                },

                {
                    "my_info_calendar",
                    QObject::tr("Calendar"),
                    NodeType::Page
                },

                {
                    "my_info_schedule",
                    QObject::tr("Schedule"),
                    NodeType::Page
                },

                {
                    "my_info_class_information",
                    QObject::tr("Class Information"),
                    NodeType::Page
                },

                {
                    "my_info_class_roster",
                    QObject::tr("Rosters"),
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
                    "sub_prep_important",
                    QObject::tr("Important Information"),
                    NodeType::Page
                },

                {
                    "sub_prep_comments",
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
            QObject::tr("Korean Teachers"),
            NodeType::Root,

            {
                {
                    "teachers_mine",
                    QObject::tr("My Co-Teachers"),
                    NodeType::Root
                },

                {
                    "teachers_all_korean",
                    QObject::tr("All Teachers"),
                    NodeType::Root
                }
            }
        },

        {
            "document",
            QObject::tr("Documents"),
            NodeType::Root,

            {
                {
                    "document_guides",
                    QObject::tr("Guides"),
                    NodeType::Root,

                    {
                        {
                            "document_guides_lesson_planning",
                            QObject::tr("Lesson Planning"),
                            NodeType::Page
                        },

                        {
                            "document_guides_powerpoint_shortcuts",
                            QObject::tr("PowerPoint Shortcuts"),
                            NodeType::Page
                        }
                    }
                },

                {
                    "document_lesson_templates",
                    QObject::tr("Lesson Templates"),
                    NodeType::Root,

                    {
                        {
                            "document_lesson_templates_sp_wr",
                            QObject::tr("Speaking / Writing"),
                            NodeType::Page
                        },

                        {
                            "document_lesson_templates_skill",
                            QObject::tr("Skill / TBL"),
                            NodeType::Page
                        },

                        {
                            "document_lesson_templates_student_led",
                            QObject::tr("Student-Led Activities"),
                            NodeType::Page
                        },

                        {
                            "document_lesson_templates_ms_essay",
                            QObject::tr("Middle School OE"),
                            NodeType::Page
                        },

                        {
                            "document_lesson_templates_theseus_paragraph",
                            QObject::tr("Theseus Paragraph Writing"),
                            NodeType::Page
                        },

                        {
                            "document_lesson_templates_creo_writing",
                            QObject::tr("CREO"),
                            NodeType::Page
                        }
                    }
                },

                {
                    "document_online_essay",
                    QObject::tr("Online Essay"),
                    NodeType::Root,

                    {
                        {
                            "document_online_essay_topic_template",
                            QObject::tr("Essay Topic Template"),
                            NodeType::Page
                        },

                        {
                            "document_online_essay_brainstorm",
                            QObject::tr("Essay Brainstorm"),
                            NodeType::Page
                        },

                        {
                            "document_online_essay_theseus_explained",
                            QObject::tr("Theseus Paragraphs Explained"),
                            NodeType::Page
                        }
                    }
                },

                {
                    "document_speaking_evals",
                    QObject::tr("Speaking Evals"),
                    NodeType::Root,

                    {
                        {
                            "document_speaking_evals_tips_one_on_one",
                            QObject::tr("One-to-One Evaluations"),
                            NodeType::Page
                        },

                        {
                            "document_speaking_evals_tips_presentations",
                            QObject::tr("In-Class Presentations"),
                            NodeType::Page
                        },

                        {
                            "document_speaking_evals_tips_single_class",
                            QObject::tr("Finishing in One Class"),
                            NodeType::Page
                        },

                        {
                            "document_speaking_evals_topic_options",
                            QObject::tr("Topic Options"),
                            NodeType::Page
                        },

                        {
                            "document_speaking_evals_regular_template",
                            QObject::tr("Regular Template"),
                            NodeType::Page
                        },

                        {
                            "document_speaking_evals_athena_songs_template",
                            QObject::tr("Athena/Song's Template"),
                            NodeType::Page
                        },

                        {
                            "document_speaking_evals_winner_certificates",
                            QObject::tr("Winner Certificates"),
                            NodeType::Page
                        }
                    }
                },

                {
                    "document_book_reports",
                    QObject::tr("Book Reports"),
                    NodeType::Root,

                    {
                        {
                            "document_book_reports_grading_rubric",
                            QObject::tr("Grading Rubric"),
                            NodeType::Page
                        },

                        {
                            "document_book_reports_grading_rubric_40",
                            QObject::tr("Grading Rubric (40%)"),
                            NodeType::Page
                        },

                        {
                            "document_book_reports_student_info_handout",
                            QObject::tr("Student Info Handout"),
                            NodeType::Page
                        }
                    }
                },

                {
                    "document_training",
                    QObject::tr("Training"),
                    NodeType::Root,

                    {
                        {
                            "document_training_observation",
                            QObject::tr("Observation"),
                            NodeType::Page
                        },

                        {
                            "document_training_reflection",
                            QObject::tr("Reflection"),
                            NodeType::Page
                        },

                        {
                            "document_training_final_reflection",
                            QObject::tr("Final Reflection"),
                            NodeType::Page
                        }
                    }
                },

                {
                    "document_vacation_sub_prep",
                    QObject::tr("Vacation / Sub Prep"),
                    NodeType::Root,

                    {
                        {
                            "document_vacation_sub_prep_applying",
                            QObject::tr("Applying for Vacation"),
                            NodeType::Page
                        },

                        {
                            "document_vacation_sub_prep_guidelines",
                            QObject::tr("Vacation Guidelines"),
                            NodeType::Page
                        },

                        {
                            "document_vacation_sub_prep_request_form",
                            QObject::tr("Vacacation Request Form"),
                            NodeType::Page
                        },

                        {
                            "document_vacation_sub_prep_procedures",
                            QObject::tr("Sub Prep Procedures"),
                            NodeType::Page
                        },

                        {
                            "document_vacation_sub_prep_checklist",
                            QObject::tr("Sub Prep Checklist"),
                            NodeType::Page
                        },

                        {
                            "document_vacation_sub_prep_template",
                            QObject::tr("Sub Prep Template"),
                            NodeType::Page
                        }
                    }
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



// =========================================================
// Class Template
// =========================================================

QList<TreeNodeSpec> classTemplate()
{
    return
    {
        {
            "class_info",
            QObject::tr("Class Info"),
            NodeType::Page
        },

        {
            "class_roster",
            QObject::tr("Class Roster"),
            NodeType::Page
        },

        {
            "class_notes",
            QObject::tr("Class Notes"),
            NodeType::Page
        },

        {
            "student_evaluations",
            QObject::tr("Student Evaluations"),
            NodeType::ClassSection,

            {
                {
                    "speaking_winter",
                    QObject::tr("Winter"),
                    NodeType::Page
                },

                {
                    "speaking_speech_contest",
                    QObject::tr("Speech Contest"),
                    NodeType::Page
                },

                {
                    "speaking_summer",
                    QObject::tr("Summer"),
                    NodeType::Page
                },

                {
                    "speaking_fall",
                    QObject::tr("Fall"),
                    NodeType::Page
                }
            }
        }
};
}
