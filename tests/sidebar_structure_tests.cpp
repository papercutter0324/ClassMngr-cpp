#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/constants/gui_constants.h"
#include "features/documents/document_catalog.h"
#include "next/application/document_catalog_projection.h"

#include <QApplication>
#include <QDir>
#include <QFontMetrics>
#include <QMenu>
#include <QSignalSpy>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

#include <algorithm>
#include <optional>
#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

using DocumentCatalogProjection =
    ClassMngr::Next::Application::DocumentCatalogProjection;
using DocumentCatalogProjectionInput =
    ClassMngr::Next::Application::DocumentCatalogProjectionInput;
using DocumentContentReference =
    ClassMngr::Next::Application::DocumentContentReference;

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

QStringList subtreeKeys(
    QTreeWidgetItem* root
    )
{
    QStringList keys;

    if (!root)
    {
        return keys;
    }

    for (int index = 0; index < root->childCount(); ++index)
    {
        QTreeWidgetItem* child = root->child(index);

        if (!child)
        {
            continue;
        }

        keys.append(child->data(0, KeyRole).toString());
        keys.append(subtreeKeys(child));
    }

    return keys;
}

int documentPageCount(
    QTreeWidgetItem* root
    )
{
    if (!root)
    {
        return 0;
    }

    int count = 0;

    for (int index = 0; index < root->childCount(); ++index)
    {
        QTreeWidgetItem* child = root->child(index);

        if (!child)
        {
            continue;
        }

        const auto type = static_cast<NodeType>(
            child->data(0, Qt::UserRole).toInt()
            );

        count += type == NodeType::Page
            ? 1
            : documentPageCount(child);
    }

    return count;
}

std::optional<DocumentCatalogProjection> projectionFromCatalog(
    const DocumentCatalog& catalog,
    const QString& localeName
    )
{
    DocumentCatalogProjectionInput input;

    for (const DocumentFolderDefinition& folder : catalog.folders())
    {
        const auto folderId =
            ClassMngr::Next::Domain::DocumentFolderId::fromString(
                folder.id.toUtf8().toStdString()
                );

        if (!folderId)
        {
            return std::nullopt;
        }

        input.folders.push_back({
            *folderId,
            folder.path.toUtf8().toStdString(),
            folder.id.toUtf8().toStdString(),
            folder.sidebarNames.forLocale(localeName)
                .toUtf8().toStdString(),
            folder.order,
            folder.parentPath.toUtf8().toStdString()
        });
    }

    for (const DocumentDefinition& document : catalog.documents())
    {
        const auto documentId =
            ClassMngr::Next::Domain::DocumentId::fromString(
                document.id.toUtf8().toStdString()
                );
        const auto folder = std::find_if(
            catalog.folders().cbegin(),
            catalog.folders().cend(),
            [&document](const DocumentFolderDefinition& candidate)
            {
                return candidate.path == document.pdf.path;
            }
            );

        if (!documentId || folder == catalog.folders().cend())
        {
            return std::nullopt;
        }

        const auto folderId =
            ClassMngr::Next::Domain::DocumentFolderId::fromString(
                folder->id.toUtf8().toStdString()
                );

        if (!folderId)
        {
            return std::nullopt;
        }

        const QString relativePdfPath = QDir::fromNativeSeparators(
            QDir(document.pdf.path).filePath(document.pdf.fileName)
            );
        const bool exportable =
            document.exportingEnabled && document.exportFile.has_value();
        std::optional<DocumentContentReference> exportReference;

        if (exportable)
        {
            const QString relativeExportPath = QDir::fromNativeSeparators(
                QDir(document.exportFile->path).filePath(
                    document.exportFile->fileName
                    )
                );
            exportReference.emplace(
                "resource://documents/"
                    + relativeExportPath.toUtf8().toStdString()
                );
        }

        input.documents.push_back({
            *documentId,
            *folderId,
            relativePdfPath.toUtf8().toStdString(),
            document.id.toUtf8().toStdString(),
            document.sidebarNames.forLocale(localeName)
                .toUtf8().toStdString(),
            document.order,
            document.printingEnabled,
            exportable,
            DocumentContentReference(
                "resource://documents/"
                    + relativePdfPath.toUtf8().toStdString()
                ),
            std::move(exportReference)
        });
    }

    const auto result =
        DocumentCatalogProjection::create(std::move(input));

    if (!result)
    {
        return std::nullopt;
    }

    return result.value();
}

std::optional<DocumentCatalogProjection> recursiveExpansionProjection(
    const bool korean
    )
{
    using ClassMngr::Next::Domain::DocumentFolderId;
    using ClassMngr::Next::Domain::DocumentId;

    const auto rootFolderId = DocumentFolderId::fromString(
        "test_recursive_root"
        );
    const auto nestedFolderId = DocumentFolderId::fromString(
        "test_recursive_nested"
        );
    const auto siblingFolderId = DocumentFolderId::fromString(
        "test_recursive_sibling"
        );
    const auto nestedDocumentId = DocumentId::fromString(
        "test_recursive_nested_document"
        );
    const auto siblingDocumentId = DocumentId::fromString(
        "test_recursive_sibling_document"
        );

    if (!rootFolderId || !nestedFolderId || !siblingFolderId
        || !nestedDocumentId || !siblingDocumentId)
    {
        return std::nullopt;
    }

    DocumentCatalogProjectionInput input;
    input.folders = {
        {
            *rootFolderId,
            "synthetic/root",
            "document_test_root",
            korean ? "깊은 폴더" : "Deep Folder",
            0,
            ""
        },
        {
            *nestedFolderId,
            "synthetic/root/nested",
            "document_test_nested",
            korean ? "중첩 폴더" : "Nested Folder",
            0,
            "synthetic/root"
        },
        {
            *siblingFolderId,
            "synthetic/root/sibling",
            "document_test_sibling",
            korean ? "접힌 형제 폴더" : "Collapsed Sibling",
            1,
            "synthetic/root"
        }
    };
    input.documents = {
        {
            *nestedDocumentId,
            *nestedFolderId,
            "synthetic/nested.pdf",
            "document_test_nested_leaf",
            korean ? "중첩 문서" : "Nested Document",
            0,
            true,
            false,
            DocumentContentReference(
                "resource://documents/synthetic/nested.pdf"
                ),
            std::nullopt
        },
        {
            *siblingDocumentId,
            *siblingFolderId,
            "synthetic/sibling.pdf",
            "document_test_sibling_leaf",
            korean ? "형제 문서" : "Sibling Document",
            0,
            true,
            false,
            DocumentContentReference(
                "resource://documents/synthetic/sibling.pdf"
                ),
            std::nullopt
        }
    };

    const auto result = DocumentCatalogProjection::create(std::move(input));
    if (!result)
    {
        return std::nullopt;
    }

    return result.value();
}
}

class SidebarStructureTests : public QObject
{
    Q_OBJECT

private slots:
    void classesPageContainsNoIndividualEntries();
    void classesPageContextMenuOffersAddClass();
    void topLevelOrderAndSubPrepStructure();
    void defaultWidthAccommodatesLongestTopLevelLabel();
    void documentCatalogBuildsLocalizedTree();
    void recursiveExpandedKeyPathsSurviveLocalizedCatalogRebuild();
};

void SidebarStructureTests::classesPageContainsNoIndividualEntries()
{
    Sidebar sidebar;

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* workspace =
        topLevelWithKey(tree, QStringLiteral("my_workspace"));
    QVERIFY(workspace);
    QCOMPARE(workspace->text(0), QStringLiteral("My Workspace"));
    QCOMPARE(workspace->childCount(), 0);

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
        QStringLiteral("my_workspace"),
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

    QTreeWidgetItem* workspace =
        topLevelWithKey(tree, QStringLiteral("my_workspace"));
    QVERIFY(workspace);
    QCOMPARE(workspace->text(0), QStringLiteral("My Workspace"));
    QCOMPARE(
        static_cast<NodeType>(
            workspace->data(0, Qt::UserRole).toInt()
            ),
        NodeType::Page
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_calendar"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_workspace")})
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_information"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_workspace")})
        );

    sidebar.selectMyInfoSection(QStringLiteral("my_info_schedule"));
    QCOMPARE(
        sidebar.selectedKeys(),
        QStringList({QStringLiteral("my_workspace")})
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

void SidebarStructureTests::defaultWidthAccommodatesLongestTopLevelLabel()
{
    Sidebar sidebar;
    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    int longestLabelWidth = 0;

    for (int index = 0;
         index < tree->topLevelItemCount();
         ++index)
    {
        QTreeWidgetItem* item = tree->topLevelItem(index);
        QVERIFY(item);

        longestLabelWidth = qMax(
            longestLabelWidth,
            QFontMetrics(tree->font()).horizontalAdvance(item->text(0))
            );
    }

    QCOMPARE(
        sidebar.defaultWidthForTopLevelLabels(),
        longestLabelWidth
            + UiConstants::MainWindow::SidebarStartupLabelPadding
        );
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

    auto koreanProjection = projectionFromCatalog(
        *catalog,
        QStringLiteral("ko_KR")
        );
    QVERIFY(koreanProjection.has_value());

    Sidebar sidebar;
    sidebar.setDocumentCatalog(
        *koreanProjection,
        QStringLiteral("ko_KR")
        );
    koreanProjection.reset();
    sidebar.rebuildTree();

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* documents =
        topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    QCOMPARE(documents->childCount(), 7);
    QCOMPARE(documentPageCount(documents), 30);
    const QStringList koreanKeys = subtreeKeys(documents);

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

    auto englishProjection = projectionFromCatalog(
        *catalog,
        QStringLiteral("en_US")
        );
    QVERIFY(englishProjection.has_value());

    sidebar.setDocumentCatalog(
        std::move(*englishProjection),
        QStringLiteral("en_US")
        );
    englishProjection.reset();

    documents =
        topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    QCOMPARE(documents->childCount(), 7);
    QCOMPARE(documentPageCount(documents), 30);
    QCOMPARE(subtreeKeys(documents), koreanKeys);
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

    Sidebar emptySidebar;
    emptySidebar.setDocumentCatalog(
        {},
        QStringLiteral("en_US")
        );
    auto* emptyTree = emptySidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(emptyTree);
    QCOMPARE(emptyTree->topLevelItemCount(), 7);
    QVERIFY(!topLevelWithKey(emptyTree, QStringLiteral("document")));
}

void SidebarStructureTests::recursiveExpandedKeyPathsSurviveLocalizedCatalogRebuild()
{
    // The shipped catalog has Documents -> folder children -> document leaves.
    // This synthetic projection adds another folder level to verify that the
    // shared key-path API preserves expanded descendants at arbitrary depth.
    const auto englishProjection = recursiveExpansionProjection(false);
    const auto koreanProjection = recursiveExpansionProjection(true);
    QVERIFY(englishProjection.has_value());
    QVERIFY(koreanProjection.has_value());

    Sidebar sidebar;
    sidebar.setDocumentCatalog(
        *englishProjection,
        QStringLiteral("en_US")
        );
    sidebar.rebuildTree();

    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* documents =
        topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    QTreeWidgetItem* rootFolder =
        childWithKey(documents, QStringLiteral("document_test_root"));
    QVERIFY(rootFolder);
    QTreeWidgetItem* nestedFolder =
        childWithKey(rootFolder, QStringLiteral("document_test_nested"));
    QVERIFY(nestedFolder);
    QTreeWidgetItem* collapsedSibling =
        childWithKey(rootFolder, QStringLiteral("document_test_sibling"));
    QVERIFY(collapsedSibling);
    QCOMPARE(nestedFolder->child(0)->text(0), QStringLiteral("Nested Document"));

    sidebar.selectCampusSection(QStringLiteral("campus_information"));
    const QStringList selectedKeys = sidebar.selectedKeys();
    QVERIFY(!selectedKeys.isEmpty());

    documents->setExpanded(true);
    rootFolder->setExpanded(true);
    nestedFolder->setExpanded(true);
    collapsedSibling->setExpanded(false);
    rootFolder->setExpanded(false);

    const QList<QStringList> expandedKeyPaths =
        sidebar.expandedItemKeyPaths();
    const QStringList documentsPath{QStringLiteral("document")};
    const QStringList nestedPath{
        QStringLiteral("document"),
        QStringLiteral("document_test_root"),
        QStringLiteral("document_test_nested")
    };
    const QStringList rootFolderPath{
        QStringLiteral("document"),
        QStringLiteral("document_test_root")
    };
    const QStringList siblingPath{
        QStringLiteral("document"),
        QStringLiteral("document_test_root"),
        QStringLiteral("document_test_sibling")
    };
    QVERIFY(expandedKeyPaths.contains(documentsPath));
    QVERIFY(expandedKeyPaths.contains(nestedPath));
    QVERIFY(!expandedKeyPaths.contains(rootFolderPath));
    QVERIFY(!expandedKeyPaths.contains(siblingPath));
    QVERIFY(!rootFolder->isExpanded());
    QVERIFY(nestedFolder->isExpanded());
    QVERIFY(!collapsedSibling->isExpanded());

    sidebar.setDocumentCatalog(
        *koreanProjection,
        QStringLiteral("ko_KR")
        );
    sidebar.rebuildTree();

    documents = topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    rootFolder = childWithKey(
        documents,
        QStringLiteral("document_test_root")
        );
    QVERIFY(rootFolder);
    nestedFolder = childWithKey(
        rootFolder,
        QStringLiteral("document_test_nested")
        );
    QVERIFY(nestedFolder);
    collapsedSibling = childWithKey(
        rootFolder,
        QStringLiteral("document_test_sibling")
        );
    QVERIFY(collapsedSibling);
    QCOMPARE(rootFolder->text(0), QStringLiteral("깊은 폴더"));
    QCOMPARE(nestedFolder->text(0), QStringLiteral("중첩 폴더"));
    QCOMPARE(nestedFolder->child(0)->text(0), QStringLiteral("중첩 문서"));

    sidebar.restoreExpandedItemKeyPaths(expandedKeyPaths);
    sidebar.selectByKeys(selectedKeys);
    QCOMPARE(sidebar.expandedItemKeyPaths(), expandedKeyPaths);
    QCOMPARE(sidebar.selectedKeys(), selectedKeys);

    documents->setExpanded(true);
    QVERIFY(documents->isExpanded());
    QVERIFY(!rootFolder->isExpanded());
    QVERIFY(nestedFolder->isExpanded());
    QVERIFY(!collapsedSibling->isExpanded());
}

QTEST_MAIN(SidebarStructureTests)

#include "sidebar_structure_tests.moc"
