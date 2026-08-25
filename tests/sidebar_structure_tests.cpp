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
    void classesPageContainsNoIndividualEntries();
    void classesPageContextMenuOffersAddClass();
    void topLevelOrderAndSubPrepStructure();
    void documentCatalogBuildsLocalizedTree();
};

void SidebarStructureTests::classesPageContainsNoIndividualEntries()
{
    Sidebar sidebar;

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* myInformation =
        topLevelWithKey(tree, QStringLiteral("my_info_information"));
    QVERIFY(myInformation);
    QCOMPARE(myInformation->text(0), QStringLiteral("My Information"));
    QCOMPARE(myInformation->childCount(), 0);

    QTreeWidgetItem* classesPage =
        topLevelWithKey(tree, QStringLiteral("classes"));
    QVERIFY(classesPage);
    QCOMPARE(classesPage->text(0), QStringLiteral("Classes"));
    QCOMPARE(classesPage->childCount(), 0);

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
            Q_ARG(QTreeWidgetItem*, classesPage),
            Q_ARG(int, 0)
            )
        );
    QCOMPARE(selectionSpy.count(), 1);

    const NavigationData navigation =
        qvariant_cast<NavigationData>(
            selectionSpy.takeFirst().constFirst()
            );
    QCOMPARE(navigation.classId, -1);
    QCOMPARE(navigation.routeKey, QStringLiteral("classes"));
    QCOMPARE(
        navigation.keys,
        QStringList({QStringLiteral("classes")})
        );
}

void SidebarStructureTests::classesPageContextMenuOffersAddClass()
{
    Sidebar sidebar;
    sidebar.resize(420, 700);
    sidebar.show();

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree"));
    QVERIFY(tree);

    QTreeWidgetItem* classesPage =
        topLevelWithKey(tree, QStringLiteral("classes"));
    QVERIFY(classesPage);
    tree->scrollToItem(classesPage);

    QSignalSpy addClassSpy(&sidebar, &Sidebar::addClassRequested);
    bool foundAddClassAction = false;

    QTimer::singleShot(0, &sidebar, [&foundAddClassAction]()
    {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());

        if (!menu)
        {
            return;
        }

        for (QAction* action : menu->actions())
        {
            if (action->text() == QStringLiteral("Add Class"))
            {
                foundAddClassAction = true;
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
            Q_ARG(QPoint, tree->visualItemRect(classesPage).center())
            )
        );
    QVERIFY(foundAddClassAction);
    QCOMPARE(addClassSpy.count(), 1);
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
        QStringLiteral("sub_prep"),
        QStringLiteral("co_teachers"),
        QStringLiteral("campus_staff"),
        QStringLiteral("useful_links"),
        QStringLiteral("campus_info")
    };

    QCOMPARE(tree->topLevelItemCount(), expectedKeys.size());

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
