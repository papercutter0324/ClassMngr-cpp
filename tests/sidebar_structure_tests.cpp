#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QSignalSpy>
#include <QTreeWidget>
#include <QtTest>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

QTreeWidgetItem* childWithKey(
    QTreeWidgetItem* parent,
    const QString& key
    )
{
    if (!parent)
    {
        return nullptr;
    }

    for (int index = 0; index < parent->childCount(); ++index)
    {
        QTreeWidgetItem* child = parent->child(index);

        if (child && child->data(0, KeyRole).toString() == key)
        {
            return child;
        }
    }

    return nullptr;
}

QTreeWidgetItem* topLevelWithKey(
    QTreeWidget* tree,
    const QString& key
    )
{
    if (!tree)
    {
        return nullptr;
    }

    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* item = tree->topLevelItem(index);

        if (item && item->data(0, KeyRole).toString() == key)
        {
            return item;
        }
    }

    return nullptr;
}
}

class SidebarStructureTests : public QObject
{
    Q_OBJECT

private slots:
    void classListOwnsDynamicClassesAndDeepLinks();
    void topLevelOrderAndSubPrepStructure();
};

void SidebarStructureTests::classListOwnsDynamicClassesAndDeepLinks()
{
    Sidebar sidebar;
    sidebar.addClassNode(QStringLiteral("E4 Perseus"), 42);

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* myInfo =
        topLevelWithKey(tree, QStringLiteral("my_info"));
    QVERIFY(myInfo);
    QVERIFY(
        !childWithKey(
            myInfo,
            QStringLiteral("my_info_calendar")
            )
        );
    QVERIFY(
        !childWithKey(
            myInfo,
            QStringLiteral("my_info_class_information")
            )
        );

    QTreeWidgetItem* classList =
        childWithKey(myInfo, QStringLiteral("my_info_class_list"));
    QVERIFY(classList);
    QCOMPARE(classList->text(0), QStringLiteral("Class List"));
    QCOMPARE(classList->childCount(), 1);
    classList->setExpanded(true);
    QVERIFY(
        sidebar.expandedRootKeys().contains(
            QStringLiteral("my_info_class_list")
            )
        );

    QTreeWidgetItem* classItem = classList->child(0);
    QCOMPARE(classItem->data(0, KeyRole).toString(), QStringLiteral("class"));
    QCOMPARE(classItem->childCount(), 4);
    QCOMPARE(
        childWithKey(classItem, QStringLiteral("class_details"))->text(0),
        QStringLiteral("Details")
        );
    QCOMPARE(
        childWithKey(classItem, QStringLiteral("class_roster"))->text(0),
        QStringLiteral("Roster")
        );
    QCOMPARE(
        childWithKey(classItem, QStringLiteral("class_notes"))->text(0),
        QStringLiteral("Notes")
        );
    QVERIFY(
        childWithKey(classItem, QStringLiteral("student_evaluations"))
        );

    QTreeWidgetItem* classesPage =
        topLevelWithKey(tree, QStringLiteral("classes"));
    QVERIFY(classesPage);
    QCOMPARE(classesPage->text(0), QStringLiteral("Classes"));
    QCOMPARE(
        static_cast<NodeType>(
            classesPage->data(0, Qt::UserRole).toInt()
            ),
        NodeType::Page
        );

    QSignalSpy selectionSpy(&sidebar, &Sidebar::itemSelected);
    QVERIFY(
        QMetaObject::invokeMethod(
            &sidebar,
            "onItemClicked",
            Qt::DirectConnection,
            Q_ARG(QTreeWidgetItem*, classItem),
            Q_ARG(int, 0)
            )
        );
    QCOMPARE(selectionSpy.count(), 1);

    const NavigationData navigation =
        qvariant_cast<NavigationData>(
            selectionSpy.takeFirst().constFirst()
            );
    QCOMPARE(navigation.classId, 42);
    QCOMPARE(navigation.routeKey, QStringLiteral("class_details"));
    QCOMPARE(
        navigation.keys,
        QStringList({
            QStringLiteral("my_info"),
            QStringLiteral("my_info_class_list"),
            QStringLiteral("class"),
            QStringLiteral("class_details")
        })
        );
}

void SidebarStructureTests::topLevelOrderAndSubPrepStructure()
{
    Sidebar sidebar;
    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    const QStringList expectedKeys{
        QStringLiteral("my_info"),
        QStringLiteral("my_info_calendar"),
        QStringLiteral("classes"),
        QStringLiteral("my_info_class_roster"),
        QStringLiteral("speaking_evaluations"),
        QStringLiteral("sub_prep")
    };

    for (int index = 0; index < expectedKeys.size(); ++index)
    {
        QTreeWidgetItem* item = tree->topLevelItem(index);
        QVERIFY(item);
        QCOMPARE(item->data(0, KeyRole).toString(), expectedKeys.at(index));
    }

    QTreeWidgetItem* calendar =
        topLevelWithKey(tree, QStringLiteral("my_info_calendar"));
    QVERIFY(calendar);
    QCOMPARE(calendar->text(0), QStringLiteral("Calendar"));
    QCOMPARE(
        static_cast<NodeType>(
            calendar->data(0, Qt::UserRole).toInt()
            ),
        NodeType::Page
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_calendar"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_info_calendar")})
        );

    QTreeWidgetItem* subPrep =
        topLevelWithKey(tree, QStringLiteral("sub_prep"));
    QVERIFY(subPrep);
    QCOMPARE(subPrep->childCount(), 0);
    QCOMPARE(
        static_cast<NodeType>(
            subPrep->data(0, Qt::UserRole).toInt()
            ),
        NodeType::Page
        );
}

QTEST_MAIN(SidebarStructureTests)

#include "sidebar_structure_tests.moc"
