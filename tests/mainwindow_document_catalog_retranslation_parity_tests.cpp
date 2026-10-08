#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QAction>
#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include <utility>

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

QStringList keyPath(
    const QString& first,
    const QString& second = QString(),
    const QString& third = QString()
    )
{
    QStringList keys{first};
    if (!second.isEmpty())
    {
        keys.append(second);
    }
    if (!third.isEmpty())
    {
        keys.append(third);
    }
    return keys;
}
}

class MainWindowDocumentCatalogRetranslationParityTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void languageActionsPreserveDocumentSidebarExpansion();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowDocumentCatalogRetranslationParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
    ClassMngr::Next::Platform::SettingsManagerLanguagePreferencesPort()
        .write(ClassMngr::Next::Application::LanguagePreference::English);
    SettingsManager::instance().sync();

    qRegisterMetaType<NavigationData>();
}

void MainWindowDocumentCatalogRetranslationParityTests::
    languageActionsPreserveDocumentSidebarExpansion()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));
    QCOMPARE(languageService.loadedLocaleName(), QStringLiteral("en_US"));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    Sidebar* sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));

    const QStringList selectedKeys = sidebar->selectedKeys();
    QCOMPARE(
        selectedKeys,
        keyPath(
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
            )
        );
    const QString currentPageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!currentPageIdentifier.isEmpty());

    QTreeWidgetItem* documents =
        topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    QCOMPARE(documents->text(0), QStringLiteral("Documents"));
    QCOMPARE(documents->childCount(), 7);
    QTreeWidgetItem* guides =
        childWithKey(documents, QStringLiteral("document_guides"));
    QVERIFY(guides);
    QTreeWidgetItem* vacation = childWithKey(
        documents,
        QStringLiteral("document_vacation_sub_prep")
        );
    QVERIFY(vacation);
    QTreeWidgetItem* collapsedFolder = childWithKey(
        documents,
        QStringLiteral("document_book_reports")
        );
    QVERIFY(collapsedFolder);

    documents->setExpanded(true);
    guides->setExpanded(true);
    vacation->setExpanded(true);
    collapsedFolder->setExpanded(false);

    const QList<QStringList> expandedKeyPaths =
        sidebar->expandedItemKeyPaths();
    const QStringList documentsPath{QStringLiteral("document")};
    const QStringList guidesPath{
        QStringLiteral("document"),
        QStringLiteral("document_guides")
    };
    const QStringList vacationPath{
        QStringLiteral("document"),
        QStringLiteral("document_vacation_sub_prep")
    };
    const QStringList collapsedFolderPath{
        QStringLiteral("document"),
        QStringLiteral("document_book_reports")
    };
    QVERIFY(expandedKeyPaths.contains(documentsPath));
    QVERIFY(expandedKeyPaths.contains(guidesPath));
    QVERIFY(expandedKeyPaths.contains(vacationPath));
    QVERIFY(!expandedKeyPaths.contains(collapsedFolderPath));

    QSignalSpy routeEvents(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeEvents.isValid());
    QVERIFY(pages->pdfViewerPage() == nullptr);
    QVERIFY(!pages->isPageInstantiated(PageType::PdfViewer));
    QVERIFY(
        ResourcePackManager::instance()
            .activeRoot(QStringLiteral("documents"))
            .isEmpty()
        );

    QAction* const koreanAction = window.actions().languageState
        ? window.actions().languageState->action(Language::Korean)
        : nullptr;
    QVERIFY(koreanAction);
    koreanAction->trigger();
    QApplication::processEvents();

    documents = topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    guides = childWithKey(documents, QStringLiteral("document_guides"));
    QVERIFY(guides);
    vacation = childWithKey(
        documents,
        QStringLiteral("document_vacation_sub_prep")
        );
    QVERIFY(vacation);
    collapsedFolder = childWithKey(
        documents,
        QStringLiteral("document_book_reports")
        );
    QVERIFY(collapsedFolder);

    QCOMPARE(languageService.currentLanguage(), Language::Korean);
    QCOMPARE(languageService.loadedLocaleName(), QStringLiteral("ko_KR"));
    QCOMPARE(documents->text(0), QStringLiteral("문서"));
    QCOMPARE(guides->text(0), QStringLiteral("안내서"));
    QCOMPARE(sidebar->selectedKeys(), selectedKeys);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QCOMPARE(sidebar->expandedItemKeyPaths(), expandedKeyPaths);
    QVERIFY(documents->isExpanded());
    QVERIFY(guides->isExpanded());
    QVERIFY(vacation->isExpanded());
    QVERIFY(!collapsedFolder->isExpanded());
    QCOMPARE(routeEvents.count(), 0);
    QVERIFY(pages->pdfViewerPage() == nullptr);
    QVERIFY(!pages->isPageInstantiated(PageType::PdfViewer));
    QVERIFY(
        ResourcePackManager::instance()
            .activeRoot(QStringLiteral("documents"))
            .isEmpty()
        );

    QAction* const englishAction = window.actions().languageState
        ? window.actions().languageState->action(Language::English)
        : nullptr;
    QVERIFY(englishAction);
    englishAction->trigger();
    QApplication::processEvents();

    documents = topLevelWithKey(tree, QStringLiteral("document"));
    QVERIFY(documents);
    guides = childWithKey(documents, QStringLiteral("document_guides"));
    QVERIFY(guides);
    vacation = childWithKey(
        documents,
        QStringLiteral("document_vacation_sub_prep")
        );
    QVERIFY(vacation);
    collapsedFolder = childWithKey(
        documents,
        QStringLiteral("document_book_reports")
        );
    QVERIFY(collapsedFolder);

    QCOMPARE(languageService.currentLanguage(), Language::English);
    QCOMPARE(languageService.loadedLocaleName(), QStringLiteral("en_US"));
    QCOMPARE(documents->text(0), QStringLiteral("Documents"));
    QCOMPARE(guides->text(0), QStringLiteral("Guides"));
    QCOMPARE(sidebar->selectedKeys(), selectedKeys);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QCOMPARE(sidebar->expandedItemKeyPaths(), expandedKeyPaths);
    QVERIFY(documents->isExpanded());
    QVERIFY(guides->isExpanded());
    QVERIFY(vacation->isExpanded());
    QVERIFY(!collapsedFolder->isExpanded());
    QCOMPARE(routeEvents.count(), 0);
    QVERIFY(pages->pdfViewerPage() == nullptr);
    QVERIFY(!pages->isPageInstantiated(PageType::PdfViewer));
    QVERIFY(
        ResourcePackManager::instance()
            .activeRoot(QStringLiteral("documents"))
            .isEmpty()
        );
}

QTEST_MAIN(MainWindowDocumentCatalogRetranslationParityTests)

#include "mainwindow_document_catalog_retranslation_parity_tests.moc"
