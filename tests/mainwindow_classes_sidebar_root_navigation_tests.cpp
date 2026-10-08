#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QFileInfo>
#include <QRect>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtTest>

#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

QTreeWidgetItem* findTopLevelItemByKey(
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
        QTreeWidgetItem* const item = tree->topLevelItem(index);
        if (item->data(0, KeyRole).toString() == key)
        {
            return item;
        }
    }

    return nullptr;
}
}

class MainWindowClassesSidebarRootNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void renderedClassesRootClickDispatchesToClassesPage();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowClassesSidebarRootNavigationTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
    qRegisterMetaType<NavigationData>();
}

void MainWindowClassesSidebarRootNavigationTests::
renderedClassesRootClickDispatchesToClassesPage()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("classes-root.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* const rootItem = findTopLevelItemByKey(
        tree,
        QStringLiteral("classes")
        );
    QVERIFY(rootItem);
    QCOMPARE(
        rootItem->data(0, Qt::UserRole).toInt(),
        static_cast<int>(NodeType::Page)
        );

    QSignalSpy routeSpy(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());

    tree->scrollToItem(rootItem);
    QApplication::processEvents();
    const QRect rootItemRect = tree->visualItemRect(rootItem);
    QVERIFY(rootItemRect.isValid());
    QVERIFY(!rootItemRect.isEmpty());
    QTest::mouseClick(
        tree->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        rootItemRect.center()
        );
    QApplication::processEvents();

    QCOMPARE(routeSpy.count(), 1);
    const NavigationData route = qvariant_cast<NavigationData>(
        routeSpy.at(0).at(0)
        );
    QCOMPARE(route.type, NodeType::Page);
    QCOMPARE(route.path, QStringList{rootItem->text(0)});
    QCOMPARE(route.keys, QStringList{QStringLiteral("classes")});
    QCOMPARE(route.routeKey, QStringLiteral("classes"));
    QCOMPARE(route.classId, -1);

    QVERIFY(pages->isCurrentPage(PageType::Classes));
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("classes")}
        );
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
}

QTEST_MAIN(MainWindowClassesSidebarRootNavigationTests)

#include "mainwindow_classes_sidebar_root_navigation_tests.moc"
