#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/campus/ui/campus_dashboard_page.h"
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

#include <array>
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

QTreeWidgetItem* findChildItemByKey(
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
        QTreeWidgetItem* const item = parent->child(index);
        if (item->data(0, KeyRole).toString() == key)
        {
            return item;
        }
    }

    return nullptr;
}
}

class MainWindowCampusSidebarNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void renderedCampusRootAndSectionsDispatchToCampusDashboard();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowCampusSidebarNavigationTests::initTestCase()
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

void MainWindowCampusSidebarNavigationTests::
renderedCampusRootAndSectionsDispatchToCampusDashboard()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("campus-sidebar.tps"))
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
        QStringLiteral("campus_info")
        );
    QVERIFY(rootItem);
    QCOMPARE(
        rootItem->data(0, Qt::UserRole).toInt(),
        static_cast<int>(NodeType::Root)
        );

    QSignalSpy routeSpy(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());

    const auto clickItem = [tree](QTreeWidgetItem* item)
    {
        tree->scrollToItem(item);
        QApplication::processEvents();
        const QRect itemRect = tree->visualItemRect(item);
        if (!itemRect.isValid() || itemRect.isEmpty())
        {
            return false;
        }

        QTest::mouseClick(
            tree->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            itemRect.center()
            );
        QApplication::processEvents();
        return true;
    };

    QVERIFY(clickItem(rootItem));
    QCOMPARE(routeSpy.count(), 1);
    NavigationData route = qvariant_cast<NavigationData>(
        routeSpy.at(0).at(0)
        );
    QCOMPARE(route.type, NodeType::Root);
    QCOMPARE(route.path, QStringList{rootItem->text(0)});
    QCOMPARE(route.keys, QStringList{QStringLiteral("campus_info")});
    QCOMPARE(route.routeKey, QStringLiteral("campus_info"));
    QCOMPARE(route.classId, -1);

    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    CampusDashboardPage* const campusPage = pages->campusDashboard();
    QVERIFY(campusPage);
    QCOMPARE(pages->currentWidget(), static_cast<QWidget*>(campusPage));
    QCOMPARE(
        campusPage->currentSectionKey(),
        QStringLiteral("campus_information")
        );
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
        })
        );
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    struct CampusSection final
    {
        const char* key;
        const char* sectionKey;
    };

    constexpr std::array sections{
        CampusSection{"campus_information", "campus_information"},
        CampusSection{"campus_directions", "campus_directions"},
        CampusSection{"campus_address", "campus_address"},
        CampusSection{"campus_housing", "campus_housing"},
        CampusSection{"campus_map", "campus_map"}
    };

    for (const CampusSection& section : sections)
    {
        const QString sectionKey = QString::fromLatin1(section.key);
        const QString currentSectionKey =
            QString::fromLatin1(section.sectionKey);
        QTreeWidgetItem* const sectionItem = findChildItemByKey(
            rootItem,
            sectionKey
            );
        QVERIFY(sectionItem);
        QCOMPARE(
            sectionItem->data(0, Qt::UserRole).toInt(),
            static_cast<int>(NodeType::Page)
            );

        routeSpy.clear();
        QVERIFY(clickItem(sectionItem));
        QCOMPARE(routeSpy.count(), 1);
        route = qvariant_cast<NavigationData>(routeSpy.at(0).at(0));
        QCOMPARE(route.type, NodeType::Page);
        QCOMPARE(
            route.path,
            (QStringList{rootItem->text(0), sectionItem->text(0)})
            );
        QCOMPARE(
            route.keys,
            (QStringList{QStringLiteral("campus_info"), sectionKey})
            );
        QCOMPARE(route.routeKey, sectionKey);
        QCOMPARE(route.classId, -1);

        QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
        QCOMPARE(pages->currentWidget(), static_cast<QWidget*>(campusPage));
        QCOMPARE(campusPage->currentSectionKey(), currentSectionKey);
        QCOMPARE(
            sidebar->selectedKeys(),
            (QStringList{QStringLiteral("campus_info"), currentSectionKey})
            );
        QVERIFY(services->hasOpenDatabase());
        QCOMPARE(services->databaseSession(), activeSession);
        QCOMPARE(services->currentDatabasePath(), workspacePath);
    }
}

QTEST_MAIN(MainWindowCampusSidebarNavigationTests)

#include "mainwindow_campus_sidebar_navigation_tests.moc"
