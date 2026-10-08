#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_definitions.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QRect>
#include <QSignalSpy>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QtTest>

#include <algorithm>
#include <array>
#include <utility>

class CapturedUrlHandler final : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;

public slots:
    void capture(const QUrl& url)
    {
        urls.append(url);
    }
};

class HttpsUrlHandlerRegistration final
{
public:
    explicit HttpsUrlHandlerRegistration(CapturedUrlHandler* receiver)
    {
        QDesktopServices::setUrlHandler(
            QStringLiteral("https"),
            receiver,
            "capture"
            );
    }

    ~HttpsUrlHandlerRegistration()
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
    }

    HttpsUrlHandlerRegistration(const HttpsUrlHandlerRegistration&) = delete;
    HttpsUrlHandlerRegistration& operator=(
        const HttpsUrlHandlerRegistration&
        ) = delete;
};

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
    void usefulLinksUrlLeavesHandOffToQtWithoutNavigation();

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

    QTabBar* const tabBar = campusPage->findChild<QTabBar*>();
    QVERIFY(tabBar);

    QSignalSpy sectionSpy(
        campusPage,
        &CampusDashboardPage::sectionChanged
        );
    QVERIFY(sectionSpy.isValid());
    routeSpy.clear();

    struct CampusTab final
    {
        const char* tabText;
        const char* sectionKey;
    };

    constexpr std::array tabs{
        CampusTab{"Information", "campus_information"},
        CampusTab{"Address", "campus_address"},
        CampusTab{"Directions", "campus_directions"},
        CampusTab{"Housing", "campus_housing"},
        CampusTab{"Maps", "campus_map"}
    };

    for (const CampusTab& tab : tabs)
    {
        const QString tabText = QString::fromLatin1(tab.tabText);
        const QString sectionKey = QString::fromLatin1(tab.sectionKey);
        int tabIndex = -1;
        for (int index = 0; index < tabBar->count(); ++index)
        {
            if (tabBar->tabText(index) == tabText)
            {
                tabIndex = index;
                break;
            }
        }
        QVERIFY2(tabIndex >= 0, qPrintable(tabText));

        const QRect tabRect = tabBar->tabRect(tabIndex);
        QVERIFY(!tabRect.isEmpty());
        sectionSpy.clear();
        QTest::mouseClick(
            tabBar,
            Qt::LeftButton,
            Qt::NoModifier,
            tabRect.center()
            );
        QApplication::processEvents();

        QCOMPARE(sectionSpy.count(), 1);
        QCOMPARE(
            sectionSpy.at(0).at(0).toString(),
            sectionKey
            );
        QCOMPARE(routeSpy.count(), 0);
        QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
        QCOMPARE(pages->currentWidget(), static_cast<QWidget*>(campusPage));
        QCOMPARE(campusPage->currentSectionKey(), sectionKey);
        QCOMPARE(
            sidebar->selectedKeys(),
            (QStringList{QStringLiteral("campus_info"), sectionKey})
            );
        QVERIFY(services->hasOpenDatabase());
        QCOMPARE(services->databaseSession(), activeSession);
        QCOMPARE(services->currentDatabasePath(), workspacePath);
    }
}

void MainWindowCampusSidebarNavigationTests::
usefulLinksUrlLeavesHandOffToQtWithoutNavigation()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("useful-links-sidebar.tps"))
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

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* const campusRootItem = findTopLevelItemByKey(
        tree,
        QStringLiteral("campus_info")
        );
    QVERIFY(campusRootItem);

    QTreeWidgetItem* const usefulLinksRootItem = findTopLevelItemByKey(
        tree,
        QStringLiteral("useful_links")
        );
    QVERIFY(usefulLinksRootItem);
    QCOMPARE(
        usefulLinksRootItem->data(0, Qt::UserRole).toInt(),
        static_cast<int>(NodeType::Root)
        );

    QSignalSpy routeSpy(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());

    QSignalSpy itemClickedSpy(tree, &QTreeWidget::itemClicked);
    QVERIFY(itemClickedSpy.isValid());

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

    // Establish a concrete current page so URL clicks can prove that they do
    // not dispatch a navigation event or replace the active widget.
    QVERIFY(clickItem(campusRootItem));
    QCOMPARE(itemClickedSpy.count(), 1);
    QCOMPARE(routeSpy.count(), 1);
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QWidget* const pageBeforeUrls = pages->currentWidget();
    QVERIFY(pageBeforeUrls);
    routeSpy.clear();
    itemClickedSpy.clear();

    CapturedUrlHandler urlHandler;
    HttpsUrlHandlerRegistration scopedHandler(&urlHandler);

    usefulLinksRootItem->setExpanded(true);
    QApplication::processEvents();
    QVERIFY(usefulLinksRootItem->isExpanded());

    struct ExpectedUrlLeaf final
    {
        const char* key;
        const char* destination;
    };

    constexpr std::array expectedLeaves{
        ExpectedUrlLeaf{"useful_dropbox", "https://www.dropbox.com"},
        ExpectedUrlLeaf{
            "useful_vacation_calendar",
            "https://docs.google.com/spreadsheets/d/14eL7sVctYRWXkyAFb7OePbHEdmZNTdxiv6dxd4Qo-fQ/"
        },
        ExpectedUrlLeaf{
            "useful_yearly_calendar",
            "https://docs.google.com/spreadsheets/d/18O05g7nlnsoUwrWhArZkptJFp3LMbSytdgKDjNaoMU4/edit"
        },
        ExpectedUrlLeaf{
            "useful_training_website",
            "https://sites.google.com/view/dybtraining/home"
        },
        ExpectedUrlLeaf{
            "useful_net_website",
            "https://sites.google.com/view/dybnet/home"
        },
        ExpectedUrlLeaf{
            "useful_lms_website",
            "https://lms.choisun.co.kr/login/login.php"
        },
        ExpectedUrlLeaf{
            "useful_highlights_library",
            "https://library.highlights.com/member/login/?sType=t"
        }
    };

    const QList<TreeNodeSpec> definitions = treeStructure();
    const auto usefulLinksDefinition = std::find_if(
        definitions.cbegin(),
        definitions.cend(),
        [](const TreeNodeSpec& definition)
        {
            return definition.key == QStringLiteral("useful_links");
        }
        );
    QVERIFY(usefulLinksDefinition != definitions.cend());
    QCOMPARE(
        usefulLinksDefinition->type,
        NodeType::Root
        );
    QCOMPARE(
        usefulLinksDefinition->children.size(),
        static_cast<qsizetype>(expectedLeaves.size())
        );
    QCOMPARE(
        usefulLinksRootItem->childCount(),
        static_cast<int>(expectedLeaves.size())
        );

    for (std::size_t index = 0; index < expectedLeaves.size(); ++index)
    {
        const ExpectedUrlLeaf& expected = expectedLeaves[index];
        const TreeNodeSpec& definition = usefulLinksDefinition->children.at(
            static_cast<qsizetype>(index)
            );
        const QString expectedKey = QString::fromLatin1(expected.key);
        const QString expectedDestination =
            QString::fromLatin1(expected.destination);

        QCOMPARE(definition.key, expectedKey);
        QCOMPARE(definition.type, NodeType::Url);
        QCOMPARE(definition.url, expectedDestination);

        QTreeWidgetItem* const leafItem = usefulLinksRootItem->child(
            static_cast<int>(index)
            );
        QVERIFY(leafItem);
        QCOMPARE(leafItem->data(0, KeyRole).toString(), expectedKey);
        QCOMPARE(
            leafItem->data(0, Qt::UserRole).toInt(),
            static_cast<int>(NodeType::Url)
            );
        QCOMPARE(
            leafItem->data(0, Qt::UserRole + 1).toString(),
            expectedDestination
            );
    }

    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QCOMPARE(pages->currentWidget(), pageBeforeUrls);

    for (std::size_t index = 0; index < expectedLeaves.size(); ++index)
    {
        const ExpectedUrlLeaf& expected = expectedLeaves[index];
        QTreeWidgetItem* const leafItem = usefulLinksRootItem->child(
            static_cast<int>(index)
            );
        const QString expectedKey = QString::fromLatin1(expected.key);
        const QString expectedDestination =
            QString::fromLatin1(expected.destination);
        const QUrl expectedUrl(expectedDestination);
        QVERIFY(expectedUrl.isValid());

        itemClickedSpy.clear();
        QVERIFY(clickItem(leafItem));

        QCOMPARE(itemClickedSpy.count(), 1);
        QCOMPARE(urlHandler.urls.size(), static_cast<qsizetype>(index + 1));
        QCOMPARE(urlHandler.urls.at(static_cast<qsizetype>(index)), expectedUrl);
        QCOMPARE(
            urlHandler.urls.at(static_cast<qsizetype>(index)).toString(
                QUrl::FullyEncoded
                ),
            expectedDestination
            );
        QCOMPARE(routeSpy.count(), 0);
        QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
        QCOMPARE(pages->currentWidget(), pageBeforeUrls);
        QVERIFY(tree->selectedItems().isEmpty());
        QCOMPARE(
            sidebar->selectedKeys(),
            (QStringList{
                QStringLiteral("useful_links"),
                expectedKey
            })
            );
    }

    QCOMPARE(
        urlHandler.urls.size(),
        static_cast<qsizetype>(expectedLeaves.size())
        );
    QCOMPARE(routeSpy.count(), 0);
}

QTEST_MAIN(MainWindowCampusSidebarNavigationTests)

#include "mainwindow_campus_sidebar_navigation_tests.moc"
