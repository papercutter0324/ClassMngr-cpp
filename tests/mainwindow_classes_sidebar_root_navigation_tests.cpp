#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "features/classes/ui/class_details_page.h"
#include "features/classes/ui/classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QMenu>
#include <QPointer>
#include <QScrollBar>
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
    void detailsLeaveChoicesPreserveCancelSaveAndDiscardBehavior();
    void cleanDetailsLeaveRestoresSnapshotAndStaleDataReloads();
    void classesRootContextMenuAddClassCreatesAndOpensClass();
    void newClassActionCreatesAndOpensClass();

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
detailsLeaveChoicesPreserveCancelSaveAndDiscardBehavior()
{
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope(&prompts);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("classes-details-leave.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    const auto createdClass = seedServices.classService()->create(
        QStringLiteral("Leave Choice Class")
        );
    QVERIFY(createdClass);
    const auto originalInfo = seedServices.classService()->classInfo(
        *createdClass
        );
    QVERIFY(originalInfo);
    ClassInfo seededInfo = *originalInfo;
    seededInfo.classGrade = QStringLiteral("E4");
    seededInfo.classLevel = QStringLiteral("Theseus");
    seededInfo.readingBook = QStringLiteral("Reading Explorer 1");
    seededInfo.essayBook = QStringLiteral("4A");
    seededInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Tuesday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Wednesday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Thursday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Friday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Saturday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Sunday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")}
    };
    QVERIFY(seedServices.classService()->saveClassInfo(seededInfo));
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
    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    const auto clickRoot = [tree](const QString& key)
    {
        QTreeWidgetItem* const item = findTopLevelItemByKey(tree, key);
        if (!item)
        {
            return false;
        }
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

    QVERIFY(clickRoot(QStringLiteral("classes")));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QCOMPARE(classesPage->currentClassId(), *createdClass);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    QPointer<ClassDetailsPage> details =
        classesPage->findChild<ClassDetailsPage*>();
    QVERIFY(details);
    auto* grade = details->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    auto* level = details->findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    auto* reading = details->findChild<QComboBox*>(
        QStringLiteral("classReadingBookCombo")
        );
    auto* essay = details->findChild<QComboBox*>(
        QStringLiteral("classEssayBookCombo")
        );
    QVERIFY(grade);
    QVERIFY(level);
    QVERIFY(reading);
    QVERIFY(essay);

    const auto setE5Details = [grade, level, reading, essay]()
    {
        grade->setCurrentText(QStringLiteral("E5"));
        level->setCurrentText(QStringLiteral("Artemis"));
        reading->setCurrentText(QStringLiteral("Reading Explorer 2"));
        essay->setCurrentText(QStringLiteral("5A"));
    };
    setE5Details();
    QVERIFY(details->hasUnsavedChanges());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    QVERIFY(clickRoot(QStringLiteral("my_workspace")));
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    QVERIFY(!details.isNull());
    QVERIFY(details->hasUnsavedChanges());
    QCOMPARE(grade->currentText(), QStringLiteral("E5"));

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Save
        );
    QVERIFY(clickRoot(QStringLiteral("my_workspace")));
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QTRY_VERIFY_WITH_TIMEOUT(details.isNull(), 2000);
    const auto persistedAfterSave = services->classService()->classInfo(
        *createdClass
        );
    QVERIFY(persistedAfterSave);
    QCOMPARE(persistedAfterSave->classGrade, QStringLiteral("E5"));
    QCOMPARE(persistedAfterSave->classLevel, QStringLiteral("Artemis"));

    QVERIFY(clickRoot(QStringLiteral("classes")));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    details = classesPage->findChild<ClassDetailsPage*>();
    QVERIFY(details);
    grade = details->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    level = details->findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    reading = details->findChild<QComboBox*>(
        QStringLiteral("classReadingBookCombo")
        );
    essay = details->findChild<QComboBox*>(
        QStringLiteral("classEssayBookCombo")
        );
    QVERIFY(grade);
    QVERIFY(level);
    QVERIFY(reading);
    QVERIFY(essay);
    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentText(QStringLiteral("Theseus"));
    reading->setCurrentText(QStringLiteral("Reading Explorer 1"));
    essay->setCurrentText(QStringLiteral("4A"));
    QVERIFY(details->hasUnsavedChanges());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    QVERIFY(clickRoot(QStringLiteral("my_workspace")));
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 3);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QTRY_VERIFY_WITH_TIMEOUT(details.isNull(), 2000);
    const auto persistedAfterDiscard = services->classService()->classInfo(
        *createdClass
        );
    QVERIFY(persistedAfterDiscard);
    QCOMPARE(persistedAfterDiscard->classGrade, QStringLiteral("E5"));
    QCOMPARE(persistedAfterDiscard->classLevel, QStringLiteral("Artemis"));
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
}

void MainWindowClassesSidebarRootNavigationTests::
cleanDetailsLeaveRestoresSnapshotAndStaleDataReloads()
{
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope(&prompts);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("classes-details-snapshot.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    const auto createdClass = seedServices.classService()->create(
        QStringLiteral("Snapshot Class")
        );
    QVERIFY(createdClass);
    const auto originalInfo = seedServices.classService()->classInfo(
        *createdClass
        );
    QVERIFY(originalInfo);
    ClassInfo seededInfo = *originalInfo;
    seededInfo.classGrade = QStringLiteral("E4");
    seededInfo.classLevel = QStringLiteral("Theseus");
    seededInfo.readingBook = QStringLiteral("Reading Explorer 1");
    seededInfo.essayBook = QStringLiteral("4A");
    seededInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Tuesday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Wednesday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Thursday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Friday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Saturday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")},
        {QStringLiteral("Sunday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")}
    };
    QVERIFY(seedServices.classService()->saveClassInfo(seededInfo));
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
    window.resize(1100, 560);
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    const auto clickRoot = [tree](const QString& key)
    {
        QTreeWidgetItem* const item = findTopLevelItemByKey(tree, key);
        if (!item)
        {
            return false;
        }
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

    QVERIFY(clickRoot(QStringLiteral("classes")));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QCOMPARE(classesPage->currentClassId(), *createdClass);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    const ClassesPageRuntimeMetrics beforeLeave =
        classesPage->runtimeMetrics();
    QVERIFY(beforeLeave.scheduleSectionAvailable);
    QCOMPARE(beforeLeave.currentScheduleRowCount, 7);
    ClassInfoRepository* const repository =
        services->databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const int detailReadsBeforeLeave =
        repository->classDetailsPageReadMetrics().callCount;
    QVERIFY(detailReadsBeforeLeave > 0);
    QPointer<ClassDetailsPage> details =
        classesPage->findChild<ClassDetailsPage*>();
    QVERIFY(details);
    auto* grade = details->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    QVERIFY(grade);
    QCOMPARE(grade->currentText(), QStringLiteral("E4"));
    auto* body = details->findChild<ScrollablePageBody*>();
    QVERIFY(body);
    QTRY_VERIFY_WITH_TIMEOUT(body->verticalScrollBar()->maximum() > 0, 2000);
    body->verticalScrollBar()->setValue(
        body->verticalScrollBar()->maximum() / 2
        );
    QApplication::processEvents();
    const int savedScrollPosition = body->verticalScrollBar()->value();
    QVERIFY(savedScrollPosition > 0);

    QVERIFY(clickRoot(QStringLiteral("my_workspace")));
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QTRY_VERIFY_WITH_TIMEOUT(details.isNull(), 2000);
    const ClassesPageRuntimeMetrics afterLeave =
        classesPage->runtimeMetrics();
    QCOMPARE(afterLeave.instantiatedEditorCount, 0);
    QCOMPARE(afterLeave.loadedEditorClassCount, 0);
    QCOMPARE(afterLeave.selectedEditorDescendantWidgetCount, 0);
    QVERIFY(!afterLeave.scheduleSectionAvailable);
    QCOMPARE(afterLeave.currentScheduleRowCount, 0);
    QCOMPARE(afterLeave.liveScheduleRowWidgetCount, 0);

    QVERIFY(clickRoot(QStringLiteral("classes")));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    details = classesPage->findChild<ClassDetailsPage*>();
    QVERIFY(details);
    auto* restoredGrade = details->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    QVERIFY(restoredGrade);
    QCOMPARE(restoredGrade->currentText(), QStringLiteral("E4"));
    body = details->findChild<ScrollablePageBody*>();
    QVERIFY(body);
    QTRY_COMPARE_WITH_TIMEOUT(
        body->verticalScrollBar()->value(),
        savedScrollPosition,
        2000
        );
    const ClassesPageRuntimeMetrics afterReentry =
        classesPage->runtimeMetrics();
    QCOMPARE(afterReentry.currentScheduleRowCount, 7);
    QCOMPARE(afterReentry.classQueryCount, beforeLeave.classQueryCount);
    QCOMPARE(afterReentry.classInfoQueryCount, beforeLeave.classInfoQueryCount);
    QCOMPARE(
        repository->classDetailsPageReadMetrics().callCount,
        detailReadsBeforeLeave
        );

    QVERIFY(clickRoot(QStringLiteral("my_workspace")));
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QTRY_VERIFY_WITH_TIMEOUT(details.isNull(), 2000);
    const int detailReadsBeforeStaleReturn =
        repository->classDetailsPageReadMetrics().callCount;
    auto externalInfo = services->classService()->classInfo(*createdClass);
    QVERIFY(externalInfo);
    externalInfo->classGrade = QStringLiteral("E5");
    externalInfo->classLevel = QStringLiteral("Artemis");
    externalInfo->readingBook = QStringLiteral("Reading Explorer 2");
    externalInfo->essayBook = QStringLiteral("5A");
    QVERIFY(services->classService()->saveClassInfo(*externalInfo));
    classesPage->markStale();

    QVERIFY(clickRoot(QStringLiteral("classes")));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    QCOMPARE(
        repository->classDetailsPageReadMetrics().callCount,
        detailReadsBeforeStaleReturn + 1
        );
    details = classesPage->findChild<ClassDetailsPage*>();
    QVERIFY(details);
    auto* refreshedGrade = details->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    QVERIFY(refreshedGrade);
    QCOMPARE(refreshedGrade->currentText(), QStringLiteral("E5"));

    refreshedGrade->setCurrentText(QStringLiteral("E4"));
    QVERIFY(details->hasUnsavedChanges());
    const QString dirtyGrade = refreshedGrade->currentText();
    classesPage->deactivate();
    QApplication::processEvents();
    QVERIFY(!details.isNull());
    QCOMPARE(refreshedGrade->currentText(), dirtyGrade);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(
        classesPage->runtimeMetrics().selectedEditorDescendantWidgetCount > 0
        );
    classesPage->activate();
    QApplication::processEvents();
    QCOMPARE(refreshedGrade->currentText(), dirtyGrade);
    classesPage->discardChanges();
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


void MainWindowClassesSidebarRootNavigationTests::newClassActionCreatesAndOpensClass()
{
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope(&prompts);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("new-class-action.tps"))
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
    const QString activeDatabasePath = services->currentDatabasePath();
    QCOMPARE(activeDatabasePath, workspacePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    ClassService* const classService = services->classService();
    QVERIFY(classService);

    const auto classesBefore = classService->classes();
    QVERIFY(classesBefore);
    const int classCountBefore = classesBefore->size();
    QCOMPARE(classCountBefore, 0);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    QVERIFY(!workspace->hasUnsavedChanges());

    QAction* const newClassAction = window.actions().newClass;
    QVERIFY(newClassAction);
    QVERIFY(newClassAction->isEnabled());
    newClassAction->trigger();
    QApplication::processEvents();

    const auto classesAfter = classService->classes();
    QVERIFY(classesAfter);
    QCOMPARE(classesAfter->size(), classCountBefore + 1);

    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int createdClassId = classesPage->currentClassId();
    QVERIFY(createdClassId > 0);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    QCOMPARE(classesAfter->constFirst().id, createdClassId);

    const auto persistedClass = classService->classroom(createdClassId);
    QVERIFY(persistedClass);
    QCOMPARE(persistedClass->id, createdClassId);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("classes")}
        );

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activeDatabasePath);

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(!QApplication::activeModalWidget());
}

QTEST_MAIN(MainWindowClassesSidebarRootNavigationTests)

#include "mainwindow_classes_sidebar_root_navigation_tests.moc"
