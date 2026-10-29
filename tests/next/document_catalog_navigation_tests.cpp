#include "next/application/document_catalog_projection.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Ui;

namespace
{

Domain::DocumentId documentId(const char* value)
{
    return *Domain::DocumentId::fromString(value);
}

Domain::DocumentFolderId folderId(const char* value)
{
    return *Domain::DocumentFolderId::fromString(value);
}

Application::DocumentCatalogProjection projectionForLocale(
    const QString& locale
    )
{
    using Application::DocumentCatalogPath;
    using Application::DocumentEntryKey;
    using Application::DocumentFolderKey;
    using Application::DocumentFolderOrder;
    using Application::DocumentOrder;
    using Application::DocumentCatalogProjectionInput;

    const bool korean = locale.startsWith(QStringLiteral("ko"));
    DocumentCatalogProjectionInput input;
    input.folders = {
        {
            folderId("folder-zulu"),
            DocumentCatalogPath("Zulu"),
            DocumentFolderKey("folder-zulu"),
            korean ? "줄루" : "Zulu",
            DocumentFolderOrder(20),
            DocumentCatalogPath("")
        },
        {
            folderId("folder-alpha-nested"),
            DocumentCatalogPath("Alpha/Nested"),
            DocumentFolderKey("folder-alpha-nested"),
            korean ? "중첩" : "Nested",
            DocumentFolderOrder(10),
            DocumentCatalogPath("Alpha")
        },
        {
            folderId("folder-alpha"),
            DocumentCatalogPath("Alpha"),
            DocumentFolderKey("folder-alpha"),
            korean ? "알파" : "Alpha",
            DocumentFolderOrder(20),
            DocumentCatalogPath("")
        }
    };
    input.documents = {
        {
            documentId("document-zulu"),
            folderId("folder-alpha"),
            DocumentCatalogPath("Alpha/zulu.pdf"),
            DocumentEntryKey("document-zulu"),
            korean ? "제브라" : "Zulu document",
            DocumentOrder(15),
            false,
            true,
            Domain::DocumentContentReference("resource://documents/Alpha/zulu.pdf"),
            Domain::DocumentContentReference("resource://documents/Alpha/zulu.docx")
        },
        {
            documentId("document-beta"),
            folderId("folder-zulu"),
            DocumentCatalogPath("Zulu/beta.pdf"),
            DocumentEntryKey("document-beta"),
            korean ? "베타" : "Beta document",
            DocumentOrder(1),
            true,
            false,
            Domain::DocumentContentReference("resource://documents/Zulu/beta.pdf"),
            std::nullopt
        },
        {
            documentId("document-inner"),
            folderId("folder-alpha-nested"),
            DocumentCatalogPath("Alpha/Nested/inner.pdf"),
            DocumentEntryKey("document-inner"),
            korean ? "내부 문서" : "Inner document",
            DocumentOrder(0),
            true,
            true,
            Domain::DocumentContentReference(
                "resource://documents/Alpha/Nested/inner.pdf"
                ),
            Domain::DocumentContentReference(
                "resource://documents/Alpha/Nested/inner.docx"
                )
        },
        {
            documentId("document-alpha"),
            folderId("folder-alpha"),
            DocumentCatalogPath("Alpha/alpha.pdf"),
            DocumentEntryKey("document-alpha"),
            korean ? "알파 문서" : "Alpha document",
            DocumentOrder(10),
            true,
            false,
            Domain::DocumentContentReference("resource://documents/Alpha/alpha.pdf"),
            std::nullopt
        }
    };

    auto result = Application::DocumentCatalogProjection::create(std::move(input));
    if (!result)
    {
        qFatal("Invalid document catalog test projection: %s",
               result.error().message.c_str());
    }
    return std::move(result.value());
}

QTreeWidgetItem* findItemByRole(
    QTreeWidget* tree,
    const DocumentCatalogItemRole role,
    const QString& value
    )
{
    if (!tree)
    {
        return nullptr;
    }

    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(0, static_cast<int>(role)).toString() == value)
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

QTreeWidgetItem* findCatalogRoot(QTreeWidget* tree)
{
    if (!tree)
    {
        return nullptr;
    }

    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* const item = tree->topLevelItem(index);
        if (item->data(0, static_cast<int>(DocumentCatalogItemRole::Kind)).toInt()
            == static_cast<int>(DocumentCatalogItemKind::Root))
        {
            return item;
        }
    }
    return nullptr;
}

}  // namespace

class DocumentCatalogNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rendersNestedSiblingsByOrderThenStableId();
    void documentSelectionEmitsTypedStableIdAndPreservesFlags();
    void replacingLocalizedProjectionUpdatesLabelsAndKeepsSelection();
    void resourceCatalogConstructionLoadsMetadataWithoutBodyDiagnostics();

private:
    Test::MemoryPreferences m_preferences;
    std::unique_ptr<Resources::ResourceLoader> m_loader;
};

void DocumentCatalogNavigationTests::initTestCase()
{
    const auto catalog = Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
        );
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    m_loader = std::make_unique<Resources::ResourceLoader>(std::move(*catalog));
}

void DocumentCatalogNavigationTests::rendersNestedSiblingsByOrderThenStableId()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    shell.setDocumentCatalogProjection(projectionForLocale(QStringLiteral("en_US")));
    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* const root = findCatalogRoot(tree);
    QVERIFY(root);
    QCOMPARE(root->text(0), QStringLiteral("Documents"));
    QCOMPARE(root->childCount(), 2);
    QCOMPARE(root->child(0)->text(0), QStringLiteral("Alpha"));
    QCOMPARE(root->child(1)->text(0), QStringLiteral("Zulu"));

    QTreeWidgetItem* const alpha = root->child(0);
    QCOMPARE(alpha->childCount(), 3);
    QCOMPARE(alpha->child(0)->text(0), QStringLiteral("Alpha document"));
    QCOMPARE(
        alpha->child(1)->data(
            0,
            static_cast<int>(DocumentCatalogItemRole::Kind)
            ).toInt(),
        static_cast<int>(DocumentCatalogItemKind::Folder)
        );
    QCOMPARE(alpha->child(1)->text(0), QStringLiteral("Nested"));
    QCOMPARE(alpha->child(2)->text(0), QStringLiteral("Zulu document"));
    QCOMPARE(alpha->child(1)->childCount(), 1);
    QCOMPARE(alpha->child(1)->child(0)->text(0), QStringLiteral("Inner document"));
    QCOMPARE(
        Ui::DocumentCatalogNavigation::documentIdFor(alpha->child(1)),
        std::nullopt
        );
}

void DocumentCatalogNavigationTests::
documentSelectionEmitsTypedStableIdAndPreservesFlags()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    shell.setDocumentCatalogProjection(projectionForLocale(QStringLiteral("en_US")));
    std::vector<Domain::DocumentId> selectedIds;
    shell.setDocumentCatalogSelectionHandler(
        [&selectedIds](const Domain::DocumentId& id) {
            selectedIds.push_back(id);
        }
        );

    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* const selectedItem = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-alpha")
        );
    QVERIFY(selectedItem);
    QCOMPARE(
        selectedItem->data(
            0,
            static_cast<int>(DocumentCatalogItemRole::Kind)
            ).toInt(),
        static_cast<int>(DocumentCatalogItemKind::Document)
        );
    QVERIFY(selectedItem->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Printable)
        ).toBool());
    QVERIFY(!selectedItem->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Exportable)
        ).toBool());

    const QTreeWidgetItem* const exportable = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-zulu")
        );
    QVERIFY(exportable);
    QVERIFY(!exportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Printable)
        ).toBool());
    QVERIFY(exportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Exportable)
        ).toBool());

    const QTreeWidgetItem* const printableAndExportable = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-inner")
        );
    QVERIFY(printableAndExportable);
    QVERIFY(printableAndExportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Printable)
        ).toBool());
    QVERIFY(printableAndExportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Exportable)
        ).toBool());

    tree->setCurrentItem(selectedItem);
    QCOMPARE(selectedIds.size(), std::size_t(1));
    QCOMPARE(selectedIds.front(), documentId("document-alpha"));
    QCOMPARE(
        Ui::DocumentCatalogNavigation::documentIdFor(tree->currentItem()),
        std::optional<Domain::DocumentId>(documentId("document-alpha"))
        );
}

void DocumentCatalogNavigationTests::
replacingLocalizedProjectionUpdatesLabelsAndKeepsSelection()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    shell.setDocumentCatalogProjection(projectionForLocale(QStringLiteral("en_US")));
    QString requestedLocale;
    shell.setDocumentCatalogProjectionLoader(
        [&requestedLocale](const QString& locale) {
            requestedLocale = locale;
            return Domain::Result<Application::DocumentCatalogProjection>::success(
                projectionForLocale(locale)
                );
        }
        );
    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* selectedItem = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-alpha")
        );
    QVERIFY(selectedItem);
    tree->setCurrentItem(selectedItem);

    const auto refreshed = shell.refreshDocumentCatalogProjection(
        QStringLiteral("ko_KR")
        );
    QVERIFY(refreshed);
    QCOMPARE(requestedLocale, QStringLiteral("ko_KR"));
    QCOMPARE(
        Ui::DocumentCatalogNavigation::documentIdFor(tree->currentItem()),
        std::optional<Domain::DocumentId>(documentId("document-alpha"))
        );
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("알파 문서"));
    QCOMPARE(findCatalogRoot(tree)->child(0)->text(0), QStringLiteral("알파"));
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        QVERIFY((*iterator)->text(0) != QStringLiteral("Alpha document"));
        ++iterator;
    }
}

void DocumentCatalogNavigationTests::
resourceCatalogConstructionLoadsMetadataWithoutBodyDiagnostics()
{
    const auto catalog = Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
        );
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    const Platform::ResourceDocumentCatalogPort port(loader);
    const auto projection = port.projection(QStringLiteral("en_US"));
    QVERIFY(projection);
    QVERIFY(!projection.value().folders().empty());
    QVERIFY(!projection.value().documents().empty());
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QCOMPARE(
        diagnostics.front().resourceId.value(),
        QStringLiteral("document.catalog-metadata")
        );
    QVERIFY(diagnostics.front().succeeded);
}

QTEST_MAIN(DocumentCatalogNavigationTests)
#include "document_catalog_navigation_tests.moc"
