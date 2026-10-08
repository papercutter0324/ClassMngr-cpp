#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "next/application/document_content_session.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageLogContext>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QVector>
#include <QtTest>

#include <cstdio>
#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;
constexpr auto RequestedDocumentId = "document_guides_lesson_planning";
constexpr auto ExpectedDocumentReference =
    "resource://documents/Guides/DYB Lesson Planning Guide.pdf";
constexpr auto ExpectedDocumentRelativePath =
    "Guides/DYB Lesson Planning Guide.pdf";

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

QStringList itemKeyPath(
    QTreeWidgetItem* item
    )
{
    QStringList keys;
    for (QTreeWidgetItem* current = item; current; current = current->parent())
    {
        keys.prepend(current->data(0, KeyRole).toString());
    }
    return keys;
}

QVector<QString>* capturedWarnings = nullptr;
QtMessageHandler previousMessageHandler = nullptr;

void captureQtWarning(
    const QtMsgType type,
    const QMessageLogContext& context,
    const QString& message
    )
{
    if (capturedWarnings && (type == QtWarningMsg || type == QtCriticalMsg))
    {
        capturedWarnings->append(message);
    }
    if (previousMessageHandler)
    {
        previousMessageHandler(type, context, message);
    }
    else
    {
        std::fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
    }
}

class ScopedWarningCapture final
{
public:
    explicit ScopedWarningCapture(QVector<QString>& warnings)
        : m_previous(qInstallMessageHandler(captureQtWarning))
    {
        capturedWarnings = &warnings;
        previousMessageHandler = m_previous;
    }

    ~ScopedWarningCapture()
    {
        qInstallMessageHandler(m_previous);
        capturedWarnings = nullptr;
        previousMessageHandler = nullptr;
    }

    ScopedWarningCapture(const ScopedWarningCapture&) = delete;
    ScopedWarningCapture& operator=(const ScopedWarningCapture&) = delete;

private:
    QtMessageHandler m_previous = nullptr;
};
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
    QVERIFY(!pages->isDatabaseOpen());

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

    QVERIFY(!pages->pdfViewerPage());
    const QString documentId = QString::fromLatin1(RequestedDocumentId);
    const QStringList expectedDocumentKeyPath = keyPath(
        QStringLiteral("document"),
        QStringLiteral("document_guides"),
        documentId
        );
    documents->setExpanded(true);
    guides->setExpanded(true);
    QTreeWidgetItem* const documentLeaf = childWithKey(guides, documentId);
    QVERIFY(documentLeaf);
    QCOMPARE(itemKeyPath(documentLeaf), expectedDocumentKeyPath);
    QCOMPARE(
        documentLeaf->data(0, Qt::UserRole).toInt(),
        static_cast<int>(NodeType::Page)
        );
    QVERIFY(documentLeaf->flags() & Qt::ItemIsSelectable);
    QApplication::processEvents();
    tree->scrollToItem(documentLeaf);
    QApplication::processEvents();
    const QRect documentLeafRect = tree->visualItemRect(documentLeaf);
    QVERIFY(documentLeafRect.isValid());
    QVERIFY(!documentLeafRect.isEmpty());
    const QRect visibleDocumentLeafRect = documentLeafRect.intersected(
        tree->viewport()->rect()
        );
    QVERIFY(!visibleDocumentLeafRect.isEmpty());

    QSignalSpy documentRouteEvents(sidebar, &Sidebar::itemSelected);
    QVERIFY(documentRouteEvents.isValid());
    QVector<QString> warnings;
    {
        ScopedWarningCapture warningCapture(warnings);
        QTest::mouseClick(
            tree->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            visibleDocumentLeafRect.center()
            );
        QCOMPARE(documentRouteEvents.count(), 1);
        const QList<QVariant> routeArguments = documentRouteEvents.at(0);
        QCOMPARE(routeArguments.size(), 1);
        const NavigationData route = qvariant_cast<NavigationData>(
            routeArguments.at(0)
            );
        QVERIFY(route.type == NodeType::Page);
        QCOMPARE(route.keys, expectedDocumentKeyPath);
        QCOMPARE(route.routeKey, documentId);
        QCOMPARE(sidebar->selectedKeys(), expectedDocumentKeyPath);

        QTRY_VERIFY(pages->isCurrentPage(PageType::PdfViewer));
        QTRY_VERIFY(pages->pdfViewerPage() != nullptr);
        PdfViewerPage* const viewer = pages->pdfViewerPage();
        QTRY_VERIFY(viewer->hasLoadedDocument());
        QTRY_VERIFY(
            viewer->documentContentSnapshot().phase()
                == ClassMngr::Next::Application::DocumentContentPhase::Ready
            );
        QCOMPARE(window.pageManager()->currentWidget(),
            static_cast<QWidget*>(viewer));

        const QString documentsRoot = ResourcePackManager::instance()
            .activeRoot(QStringLiteral("documents"));
        QVERIFY(!documentsRoot.isEmpty());
        const QString expectedPdfPath = QDir(documentsRoot).filePath(
            QString::fromLatin1(ExpectedDocumentRelativePath)
            );
        QVERIFY(QFile::exists(expectedPdfPath));
        QCOMPARE(viewer->currentFilePath(), expectedPdfPath);
        const auto contentSnapshot = viewer->documentContentSnapshot();
        QVERIFY(contentSnapshot.reference().has_value());
        QCOMPARE(
            QString::fromUtf8(contentSnapshot.reference()->value().c_str()),
            QString::fromLatin1(ExpectedDocumentReference)
            );
        QVERIFY(viewer->outputCapabilities().printEnabled);
        QVERIFY(viewer->outputCapabilities().saveAsEnabled);
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        for (QWidget* topLevel : QApplication::topLevelWidgets())
        {
            const auto* messageBox = qobject_cast<QMessageBox*>(topLevel);
            QVERIFY(!messageBox || !messageBox->isVisible());
        }
    }
    QVERIFY2(warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst()));
}

QTEST_MAIN(MainWindowDocumentCatalogRetranslationParityTests)

#include "mainwindow_document_catalog_retranslation_parity_tests.moc"
