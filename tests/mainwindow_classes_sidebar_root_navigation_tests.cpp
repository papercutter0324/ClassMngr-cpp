#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/classes/ui/classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QMenu>
#include <QRect>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtTest>

#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

class UserPromptServiceScope final
{
public:
    explicit UserPromptServiceScope(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~UserPromptServiceScope()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

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
    void classesRootContextMenuAddClassCreatesAndOpensClass();

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

void MainWindowClassesSidebarRootNavigationTests::
classesRootContextMenuAddClassCreatesAndOpensClass()
{
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope(&prompts);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("classes-context-menu.tps"))
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
    ClassService* const classService = services->classService();
    QVERIFY(classService);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const priorPage = pages->myWorkspacePage();
    QVERIFY(priorPage);
    QVERIFY(!priorPage->hasUnsavedChanges());

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

    tree->scrollToItem(rootItem);
    QApplication::processEvents();
    const QRect rootItemRect = tree->visualItemRect(rootItem);
    QVERIFY(rootItemRect.isValid());
    QVERIFY(!rootItemRect.isEmpty());

    bool timerCallbackRan = false;
    bool popupObserved = false;
    bool addClassActionFound = false;
    bool addClassActionEnabled = false;
    bool addClassActionTriggered = false;
    QString menuFlowError;
    QTimer menuScript;
    menuScript.setSingleShot(true);
    QObject::connect(
        &menuScript,
        &QTimer::timeout,
        &window,
        [&]()
        {
            timerCallbackRan = true;
            QMenu* const menu = qobject_cast<QMenu*>(
                QApplication::activePopupWidget()
                );
            if (!menu)
            {
                menuFlowError = QStringLiteral(
                    "Right-click did not open an active Sidebar popup."
                    );
                return;
            }
            popupObserved = true;

            QAction* addClassAction = nullptr;
            for (QAction* action : menu->actions())
            {
                if (action->text() == QStringLiteral("Add Class"))
                {
                    addClassAction = action;
                    break;
                }
            }

            if (!addClassAction)
            {
                menuFlowError = QStringLiteral(
                    "The Classes context menu has no Add Class action."
                    );
            }
            else
            {
                addClassActionFound = true;
                addClassActionEnabled = addClassAction->isEnabled();
                if (!addClassActionEnabled)
                {
                    menuFlowError = QStringLiteral(
                        "The Add Class action is disabled with an open database."
                        );
                }
                else
                {
                    addClassAction->trigger();
                    addClassActionTriggered = true;
                }
            }

            menu->close();
        }
        );
    menuScript.start(0);
    QTest::mouseClick(
        tree->viewport(),
        Qt::RightButton,
        Qt::NoModifier,
        rootItemRect.center()
        );

    // QTest's synthetic mouse click does not produce the platform-generated
    // QContextMenuEvent on the offscreen plugin. Send that normal widget input
    // event through the viewport so Sidebar's production request connection
    // opens its menu; do not call the private handler or emit its signal.
    const QPoint contextPosition = rootItemRect.center();
    QContextMenuEvent contextMenuEvent(
        QContextMenuEvent::Mouse,
        contextPosition,
        tree->viewport()->mapToGlobal(contextPosition)
        );
    QApplication::sendEvent(tree->viewport(), &contextMenuEvent);
    QApplication::processEvents();
    menuScript.stop();

    if (!timerCallbackRan && menuFlowError.isEmpty())
    {
        menuFlowError = QStringLiteral(
            "The bounded context-menu callback did not run."
            );
    }

    QVERIFY2(timerCallbackRan, qPrintable(menuFlowError));
    QVERIFY2(popupObserved, qPrintable(menuFlowError));
    QVERIFY2(addClassActionFound, qPrintable(menuFlowError));
    QVERIFY2(addClassActionEnabled, qPrintable(menuFlowError));
    QVERIFY2(addClassActionTriggered, qPrintable(menuFlowError));
    QVERIFY(!QApplication::activeModalWidget());

    QVERIFY(!pages->isCurrentPage(PageType::MyWorkspace));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int createdClassId = classesPage->currentClassId();
    QVERIFY(createdClassId > 0);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);

    const auto persistedClass = classService->classroom(createdClassId);
    QVERIFY(persistedClass);
    QCOMPARE(persistedClass->id, createdClassId);

    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("classes")}
        );
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
}

QTEST_MAIN(MainWindowClassesSidebarRootNavigationTests)

#include "mainwindow_classes_sidebar_root_navigation_tests.moc"
