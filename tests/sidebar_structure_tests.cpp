#include "ui/shared/widgets/sidebar/sidebar.h"
#include "features/documents/document_catalog.h"

#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QTimer>
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
    void classContextMenuOffersExportForClickedClass();
    void topLevelOrderAndSubPrepStructure();
    void documentCatalogBuildsLocalizedTree();
};

void SidebarStructureTests::classListOwnsDynamicClassesAndDeepLinks()
{
    Sidebar sidebar;
    sidebar.addClassNode(QStringLiteral("E4 Perseus"), 42);

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* myInformation =
        topLevelWithKey(tree, QStringLiteral("my_info_information"));
    QVERIFY(myInformation);
    QCOMPARE(myInformation->text(0), QStringLiteral("My Information"));
    QCOMPARE(myInformation->childCount(), 0);

    QTreeWidgetItem* classList =
        topLevelWithKey(tree, QStringLiteral("my_info_class_list"));
    QVERIFY(classList);
    QCOMPARE(classList->text(0), QStringLiteral("Individual Class List"));
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
            QStringLiteral("my_info_class_list"),
            QStringLiteral("class"),
            QStringLiteral("class_details")
        })
        );
}

void SidebarStructureTests::classContextMenuOffersExportForClickedClass()
{
    Sidebar sidebar;
    sidebar.resize(420, 700);
    sidebar.addClassNode(QStringLiteral("Alpha"), 11);
    sidebar.addClassNode(QStringLiteral("Beta"), 22);
    sidebar.show();

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree"));
    QVERIFY(tree);

    QTreeWidgetItem* classList =
        topLevelWithKey(tree, QStringLiteral("my_info_class_list"));
    QVERIFY(classList);
    classList->setExpanded(true);
    QTreeWidgetItem* betaClass = classList->child(1);
    QVERIFY(betaClass);
    tree->scrollToItem(betaClass);

    QSignalSpy exportSpy(&sidebar, &Sidebar::exportClassRequested);
    bool foundExportAction = false;

    QTimer::singleShot(0, &sidebar, [&foundExportAction]()
    {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());

        if (!menu)
        {
            return;
        }

        for (QAction* action : menu->actions())
        {
            if (action->text() == QStringLiteral("Export Class"))
            {
                foundExportAction = true;
                action->trigger();
                menu->close();
                return;
            }
        }

        menu->close();
    });

    QVERIFY(
        QMetaObject::invokeMethod(
            &sidebar,
            "showContextMenu",
            Qt::DirectConnection,
            Q_ARG(QPoint, tree->visualItemRect(betaClass).center())
            )
        );
    QVERIFY(foundExportAction);
    QCOMPARE(exportSpy.count(), 1);
    QCOMPARE(exportSpy.takeFirst().constFirst().toInt(), 22);
}

void SidebarStructureTests::topLevelOrderAndSubPrepStructure()
{
    Sidebar sidebar;
    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    const QStringList expectedKeys{
        QStringLiteral("my_info_information"),
        QStringLiteral("my_info_schedule"),
        QStringLiteral("my_info_calendar"),
        QStringLiteral("classes"),
        QStringLiteral("my_info_class_roster"),
        QStringLiteral("speaking_evaluations"),
        QStringLiteral("sub_prep"),
        QStringLiteral("co_teachers"),
        QStringLiteral("campus_staff"),
        QStringLiteral("my_info_class_list")
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

    QTreeWidgetItem* schedule =
        topLevelWithKey(tree, QStringLiteral("my_info_schedule"));
    QVERIFY(schedule);
    QCOMPARE(schedule->text(0), QStringLiteral("Schedule"));
    QCOMPARE(
        static_cast<NodeType>(
            schedule->data(0, Qt::UserRole).toInt()
            ),
        NodeType::Page
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_calendar"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_info_calendar")})
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_information"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_info_information")})
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_schedule"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_info_schedule")})
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

    QTreeWidgetItem* coTeachers =
        topLevelWithKey(tree, QStringLiteral("co_teachers"));
    QVERIFY(coTeachers);
    QCOMPARE(coTeachers->text(0), QStringLiteral("Co-Teachers"));

    QTreeWidgetItem* campusStaff =
        topLevelWithKey(tree, QStringLiteral("campus_staff"));
    QVERIFY(campusStaff);
    QCOMPARE(campusStaff->text(0), QStringLiteral("Campus Staff"));
    QCOMPARE(campusStaff->childCount(), 3);
    QCOMPARE(campusStaff->child(0)->data(0, KeyRole).toString(),
             QStringLiteral("teachers_all_korean"));
    QCOMPARE(campusStaff->child(0)->text(0), QStringLiteral("Korean Teachers"));
    QCOMPARE(campusStaff->child(1)->data(0, KeyRole).toString(),
             QStringLiteral("native_english_teachers"));
    QCOMPARE(campusStaff->child(2)->data(0, KeyRole).toString(),
             QStringLiteral("gs_team"));
}

void SidebarStructureTests::documentCatalogBuildsLocalizedTree()
{
    const auto catalog =
        DocumentCatalog::loadFromRoot(
            QStringLiteral(CLASSMNGR_TEST_DOCUMENTS_PATH)
            );

    if (!catalog)
    {
        QFAIL(qPrintable(catalog.error()));
    }
    QCOMPARE(catalog->documents().size(), 30);
    QVERIFY(catalog->warnings().isEmpty());

    const DocumentDefinition* lessonTemplate =
        catalog->document(
            QStringLiteral("document_lesson_templates_sp_wr")
            );
    QVERIFY(lessonTemplate);
    QVERIFY(!lessonTemplate->printingEnabled);
    QVERIFY(lessonTemplate->exportingEnabled);
    QVERIFY(lessonTemplate->exportFile.has_value());
    QCOMPARE(
        lessonTemplate->exportFile->fileName,
        QStringLiteral("SP+WR Template.pptx")
        );

    const DocumentDefinition* vacationRequest =
        catalog->document(
            QStringLiteral("document_vacation_sub_prep_request_form")
            );
    QVERIFY(vacationRequest);
    QVERIFY(vacationRequest->printingEnabled);
    QVERIFY(vacationRequest->exportingEnabled);
    QVERIFY(vacationRequest->exportFile.has_value());
    QCOMPARE(
        vacationRequest->exportFile->absoluteFilePath,
        vacationRequest->pdf.absoluteFilePath
        );

    Sidebar sidebar;
    sidebar.setDocumentCatalog(
        &*catalog,
        QStringLiteral("ko_KR")
        );

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* documents =
        topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    QCOMPARE(documents->childCount(), 7);

    QTreeWidgetItem* guides =
        childWithKey(
            documents,
            QStringLiteral("document_guides")
            );
    QVERIFY(guides);
    QCOMPARE(guides->text(0), QStringLiteral("안내서"));
    QCOMPARE(
        guides->child(0)->data(0, KeyRole).toString(),
        QStringLiteral("document_guides_lesson_planning")
        );
    QCOMPARE(guides->child(0)->text(0), QStringLiteral("수업 계획"));

    QTreeWidgetItem* vacation =
        childWithKey(
            documents,
            QStringLiteral("document_vacation_sub_prep")
            );
    QVERIFY(vacation);
    QTreeWidgetItem* requestForm =
        childWithKey(
            vacation,
            QStringLiteral("document_vacation_sub_prep_request_form")
            );
    QVERIFY(requestForm);

    sidebar.setDocumentCatalog(
        &*catalog,
        QStringLiteral("en_US")
        );

    documents =
        topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    vacation =
        childWithKey(
            documents,
            QStringLiteral("document_vacation_sub_prep")
            );
    requestForm =
        childWithKey(
            vacation,
            QStringLiteral("document_vacation_sub_prep_request_form")
            );
    QVERIFY(requestForm);
    QCOMPARE(requestForm->text(0), QStringLiteral("Vacation Request Form"));
}

QTEST_MAIN(SidebarStructureTests)

#include "sidebar_structure_tests.moc"
