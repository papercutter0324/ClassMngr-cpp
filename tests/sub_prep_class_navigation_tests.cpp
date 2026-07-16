#include "features/sub_prep/ui/sub_prep_class_navigation.h"
#include "ui/shared/widgets/sidebar/sidebar_definitions.h"

#include <QtTest>

namespace
{
ClassTime classTime(
    const QString& day,
    const QString& startTime
    )
{
    ClassTime time;
    time.day = day;
    time.startTime = startTime;
    return time;
}

SubPrepClassNavigation::ClassEntry classEntry(
    int classId,
    const QString& grade,
    const QString& level,
    const QList<ClassTime>& regularTimes = {},
    const QList<ClassTime>& intensiveTimes = {},
    const QString& teacherEn = {}
    )
{
    SubPrepClassNavigation::ClassEntry entry;
    entry.classId = classId;
    entry.grade = grade;
    entry.level = level;
    entry.regularTimes = regularTimes;
    entry.intensiveTimes = intensiveTimes;
    entry.teacherEn = teacherEn;
    return entry;
}
}

class SubPrepClassNavigationTests : public QObject
{
    Q_OBJECT

private slots:
    void sixOrFewerClassesUseFlatTabs();
    void forcedGroupingUsesGradeAndLevelRows();
    void moreThanSixClassesUseOrderedGradeGroups();
    void gradeModeLabelsUseLevelAndSchedule();
    void classTabsSortByLevelThenDay();
    void duplicateLabelsAreDisambiguated();
    void sidebarDefinesClassListAndClassesPage();
};

void SubPrepClassNavigationTests::sixOrFewerClassesUseFlatTabs()
{
    const QList<SubPrepClassNavigation::ClassEntry> entries{
        classEntry(
            1,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {
                classTime(QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
                classTime(QStringLiteral("Wednesday"), QStringLiteral("4:00 PM")),
                classTime(QStringLiteral("Friday"), QStringLiteral("4:00 PM"))
            }
            ),
        classEntry(
            2,
            QStringLiteral("M1"),
            QStringLiteral("Solis"),
            {
                classTime(QStringLiteral("Tuesday"), QStringLiteral("5:00 PM")),
                classTime(QStringLiteral("Thursday"), QStringLiteral("5:00 PM"))
            }
            )
    };

    const SubPrepClassNavigation::Model model =
        SubPrepClassNavigation::build(entries);

    QCOMPARE(
        static_cast<int>(model.mode),
        static_cast<int>(SubPrepClassNavigation::Mode::Flat)
        );
    QCOMPARE(model.flatClasses.size(), 2);
    QCOMPARE(
        model.flatClasses.first().label,
        QStringLiteral("E4 Perseus • M/W/F 4:00")
        );
    QCOMPARE(
        model.flatClasses.at(1).label,
        QStringLiteral("M1 Solis • T/Th 5:00")
        );
}

void SubPrepClassNavigationTests::forcedGroupingUsesGradeAndLevelRows()
{
    const QList<SubPrepClassNavigation::ClassEntry> entries{
        classEntry(
            1,
            QStringLiteral("M1"),
            QStringLiteral("Solis")
            ),
        classEntry(
            2,
            QStringLiteral("E4"),
            QStringLiteral("Perseus")
            ),
        classEntry(
            3,
            QString(),
            QStringLiteral("Custom")
            )
    };

    const SubPrepClassNavigation::Model model =
        SubPrepClassNavigation::build(
            entries,
            SubPrepClassNavigation::GroupingPolicy::AlwaysGradeGrouped
            );

    QCOMPARE(model.mode, SubPrepClassNavigation::Mode::GradeGrouped);
    QCOMPARE(model.gradeGroups.size(), 3);
    QCOMPARE(model.gradeGroups.at(0).label, QStringLiteral("E4"));
    QCOMPARE(model.gradeGroups.at(0).classes.first().classId, 2);
    QCOMPARE(model.gradeGroups.at(1).label, QStringLiteral("M1"));
    QCOMPARE(model.gradeGroups.at(1).classes.first().classId, 1);
    QCOMPARE(model.gradeGroups.at(2).label, QStringLiteral("Other"));
    QCOMPARE(model.gradeGroups.at(2).classes.first().classId, 3);
}

void SubPrepClassNavigationTests::moreThanSixClassesUseOrderedGradeGroups()
{
    const QList<SubPrepClassNavigation::ClassEntry> entries{
        classEntry(1, QStringLiteral("M2"), QStringLiteral("Ursa")),
        classEntry(2, QStringLiteral("E5"), QStringLiteral("Apollo")),
        classEntry(3, QStringLiteral("E4"), QStringLiteral("Theseus")),
        classEntry(4, QStringLiteral("M1"), QStringLiteral("Major")),
        classEntry(5, QStringLiteral(""), QStringLiteral("Custom")),
        classEntry(6, QStringLiteral("E6"), QStringLiteral("Gaia")),
        classEntry(7, QStringLiteral("M3"), QStringLiteral("Song's"))
    };

    const SubPrepClassNavigation::Model model =
        SubPrepClassNavigation::build(entries);

    QCOMPARE(
        static_cast<int>(model.mode),
        static_cast<int>(SubPrepClassNavigation::Mode::GradeGrouped)
        );

    QStringList labels;

    for (const SubPrepClassNavigation::GradeGroup& group : model.gradeGroups)
    {
        labels.append(group.label);
    }

    QCOMPARE(
        labels,
        QStringList({
            QStringLiteral("E4"),
            QStringLiteral("E5"),
            QStringLiteral("E6"),
            QStringLiteral("M1"),
            QStringLiteral("M2"),
            QStringLiteral("M3"),
            QStringLiteral("Other")
        })
        );
}

void SubPrepClassNavigationTests::gradeModeLabelsUseLevelAndSchedule()
{
    QList<SubPrepClassNavigation::ClassEntry> entries{
        classEntry(
            1,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {
                classTime(QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
                classTime(QStringLiteral("Wednesday"), QStringLiteral("4:00 PM")),
                classTime(QStringLiteral("Friday"), QStringLiteral("4:00 PM"))
            }
            ),
        classEntry(
            2,
            QStringLiteral("E4"),
            QStringLiteral("Odysseus"),
            {},
            {
                classTime(QStringLiteral("Tuesday"), QStringLiteral("10:00 AM")),
                classTime(QStringLiteral("Thursday"), QStringLiteral("10:00 AM"))
            }
            ),
        classEntry(
            3,
            QStringLiteral("E4"),
            QStringLiteral("Hercules")
            )
    };

    for (int index = 4; index <= 7; ++index)
    {
        entries.append(
            classEntry(
                index,
                QStringLiteral("M1"),
                QStringLiteral("Major")
                )
            );
    }

    const SubPrepClassNavigation::Model model =
        SubPrepClassNavigation::build(entries);

    QCOMPARE(
        static_cast<int>(model.mode),
        static_cast<int>(SubPrepClassNavigation::Mode::GradeGrouped)
        );
    QCOMPARE(model.gradeGroups.first().label, QStringLiteral("E4"));
    QCOMPARE(model.gradeGroups.first().classes.size(), 3);
    QCOMPARE(
        model.gradeGroups.first().classes.first().label,
        QStringLiteral("Perseus • M/W/F 4:00")
        );
    QCOMPARE(
        model.gradeGroups.first().classes.at(1).label,
        QStringLiteral("Odysseus • Int T/Th 10:00")
        );
    QCOMPARE(
        model.gradeGroups.first().classes.at(2).label,
        QStringLiteral("Hercules • No time")
        );
}

void SubPrepClassNavigationTests::classTabsSortByLevelThenDay()
{
    QList<SubPrepClassNavigation::ClassEntry> entries{
        classEntry(
            1,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {classTime(QStringLiteral("Friday"), QStringLiteral("4:00 PM"))}
            ),
        classEntry(
            2,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {classTime(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
            ),
        classEntry(
            3,
            QStringLiteral("E4"),
            QStringLiteral("Theseus"),
            {classTime(QStringLiteral("Sunday"), QStringLiteral("4:00 PM"))}
            ),
        classEntry(
            4,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {classTime(QStringLiteral("Sunday"), QStringLiteral("4:00 PM"))}
            )
    };

    for (int index = 5; index <= 8; ++index)
    {
        entries.append(
            classEntry(
                index,
                QStringLiteral("M1"),
                QStringLiteral("Major")
                )
            );
    }

    const SubPrepClassNavigation::Model model =
        SubPrepClassNavigation::build(entries);

    QCOMPARE(
        static_cast<int>(model.mode),
        static_cast<int>(SubPrepClassNavigation::Mode::GradeGrouped)
        );
    QCOMPARE(model.gradeGroups.first().label, QStringLiteral("E4"));
    QCOMPARE(model.gradeGroups.first().classes.size(), 4);
    QCOMPARE(model.gradeGroups.first().classes.at(0).classId, 3);
    QCOMPARE(model.gradeGroups.first().classes.at(1).classId, 2);
    QCOMPARE(model.gradeGroups.first().classes.at(2).classId, 1);
    QCOMPARE(model.gradeGroups.first().classes.at(3).classId, 4);
}

void SubPrepClassNavigationTests::duplicateLabelsAreDisambiguated()
{
    const QList<SubPrepClassNavigation::ClassEntry> entries{
        classEntry(
            1,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {classTime(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))},
            {},
            QStringLiteral("Alice")
            ),
        classEntry(
            2,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {classTime(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))},
            {},
            QStringLiteral("Bob")
            ),
        classEntry(
            3,
            QStringLiteral("E4"),
            QStringLiteral("Perseus"),
            {classTime(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))},
            {},
            QStringLiteral("Bob")
            )
    };

    const SubPrepClassNavigation::Model model =
        SubPrepClassNavigation::build(entries);

    QCOMPARE(model.flatClasses.size(), 3);
    QCOMPARE(
        model.flatClasses.first().label,
        QStringLiteral("E4 Perseus • M 4:00 • Alice")
        );
    QCOMPARE(
        model.flatClasses.at(1).label,
        QStringLiteral("E4 Perseus • M 4:00 • Bob #2")
        );
    QCOMPARE(
        model.flatClasses.at(2).label,
        QStringLiteral("E4 Perseus • M 4:00 • Bob #3")
        );
}

void SubPrepClassNavigationTests::sidebarDefinesClassListAndClassesPage()
{
    const QList<TreeNodeSpec> tree = treeStructure();

    const auto findNode =
        [](const QList<TreeNodeSpec>& nodes, const QString& key)
            -> const TreeNodeSpec*
        {
            for (const TreeNodeSpec& node : nodes)
            {
                if (node.key == key)
                {
                    return &node;
                }
            }

            return nullptr;
        };

    const TreeNodeSpec* myInfo =
        findNode(tree, QStringLiteral("my_info"));
    QVERIFY(myInfo);

    const TreeNodeSpec* classList =
        findNode(
            myInfo->children,
            QStringLiteral("my_info_class_list")
            );
    QVERIFY(classList);
    QCOMPARE(classList->label, QStringLiteral("Class List"));
    QCOMPARE(classList->type, NodeType::Root);

    const TreeNodeSpec* classes =
        findNode(tree, QStringLiteral("classes"));
    QVERIFY(classes);
    QCOMPARE(classes->label, QStringLiteral("Classes"));
    QCOMPARE(classes->type, NodeType::Page);

    const QList<TreeNodeSpec> classNodes = classTemplate();
    QCOMPARE(classNodes.size(), 4);
    QCOMPARE(classNodes.at(0).key, QStringLiteral("class_details"));
    QCOMPARE(classNodes.at(0).label, QStringLiteral("Details"));
    QCOMPARE(classNodes.at(1).label, QStringLiteral("Roster"));
    QCOMPARE(classNodes.at(2).label, QStringLiteral("Notes"));
    QCOMPARE(
        classNodes.at(3).key,
        QStringLiteral("student_evaluations")
        );
    QCOMPARE(classNodes.at(3).children.size(), 4);
}

QTEST_APPLESS_MAIN(SubPrepClassNavigationTests)

#include "sub_prep_class_navigation_tests.moc"
