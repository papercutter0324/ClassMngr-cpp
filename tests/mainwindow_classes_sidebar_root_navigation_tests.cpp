#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_details_page.h"
#include "features/classes/ui/classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QScrollBar>
#include <QSqlError>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QRect>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTextEdit>
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
        QTreeWidgetItem* const child = parent->child(index);
        if (child->data(0, KeyRole).toString() == key)
        {
            return child;
        }
    }

    return nullptr;
}

QTreeWidgetItem* findTeacherItem(
    QTreeWidget* tree,
    const qint64 teacherId
    )
{
    QTreeWidgetItem* const campusStaff = findTopLevelItemByKey(
        tree,
        QStringLiteral("campus_staff")
        );
    QTreeWidgetItem* const teachers = findChildItemByKey(
        campusStaff,
        QStringLiteral("teachers_all_korean")
        );
    if (!teachers)
    {
        return nullptr;
    }

    for (int index = 0; index < teachers->childCount(); ++index)
    {
        QTreeWidgetItem* const teacher = teachers->child(index);
        if (teacher->data(0, Qt::UserRole + 3).toLongLong() == teacherId)
        {
            return teacher;
        }
    }

    return nullptr;
}

bool clickTreeItem(
    QTreeWidget* tree,
    QTreeWidgetItem* item
    )
{
    if (!tree || !item)
    {
        return false;
    }

    for (QTreeWidgetItem* parent = item->parent();
         parent;
         parent = parent->parent())
    {
        parent->setExpanded(true);
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
}

QJsonObject teacherProfileSnapshot(const Teacher& teacher)
{
    return {
        {QStringLiteral("id"), teacher.id},
        {QStringLiteral("teacherKr"), teacher.teacherKr},
        {QStringLiteral("teacherEn"), teacher.teacherEn},
        {QStringLiteral("preferredRomanization"), teacher.preferredRomanization},
        {QStringLiteral("preferredName"), teacher.preferredName},
        {QStringLiteral("roomNumber"), teacher.roomNumber},
        {QStringLiteral("birthday"), teacher.birthday},
        {QStringLiteral("phoneNumber"), teacher.phoneNumber},
        {QStringLiteral("wifiName"), teacher.wifiName},
        {QStringLiteral("wifiPassword"), teacher.wifiPassword},
        {QStringLiteral("internetType"), teacher.internetType},
        {QStringLiteral("zoomId"), teacher.zoomId},
        {QStringLiteral("zoomPassword"), teacher.zoomPassword},
        {QStringLiteral("projectionType"), teacher.projectionType},
        {QStringLiteral("notes"), teacher.notes}
    };
}
}

class MainWindowClassesSidebarRootNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void renderedClassesRootClickDispatchesToClassesPage();
    void classesRootCancelRestoresDirtyTeacherSelection();
    void detailsLeaveChoicesPreserveCancelSaveAndDiscardBehavior();
    void cleanDetailsLeaveRestoresSnapshotAndStaleDataReloads();
    void sameTurnCleanReentryRestoresBeforeDeferredDelete();
    void classesRootContextMenuAddClassCreatesAndOpensClass();
    void newClassActionCreatesAndOpensClass();
    void classReadFailurePreservesAddClassRoute();

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
classesRootCancelRestoresDirtyTeacherSelection()
{
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope(&prompts);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("classes-teacher-leave.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    Teacher sourceTeacher;
    sourceTeacher.teacherKr = QStringLiteral("김클래스취소");
    sourceTeacher.teacherEn = QStringLiteral("Classes Cancel Teacher");
    sourceTeacher.preferredRomanization = QStringLiteral("Class Cancel");
    sourceTeacher.preferredName = QStringLiteral("Classes Cancel");
    sourceTeacher.roomNumber = QStringLiteral("Room 43");
    sourceTeacher.notes = QStringLiteral("Persisted Classes cancel profile");
    TeacherRepository* const teacherRepository = seedServices
        .databaseSession()
        ->teacherRepository();
    QVERIFY(teacherRepository);
    const auto createdTeacher = teacherRepository->createTeacher(sourceTeacher);
    QVERIFY(createdTeacher.has_value());
    sourceTeacher.id = *createdTeacher;
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
    pages->setSaveMode(SaveMode::Manual);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    const QStringList teacherKeys{
        QStringLiteral("campus_staff"),
        QStringLiteral("teachers_all_korean"),
        QStringLiteral("teacher")
    };
    QTreeWidgetItem* const teacherItem = findTeacherItem(
        tree,
        sourceTeacher.id
        );
    QVERIFY(teacherItem);

    QSignalSpy routeSpy(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());
    QVERIFY(clickTreeItem(tree, teacherItem));
    QCOMPARE(routeSpy.size(), 1);
    const NavigationData teacherRoute = qvariant_cast<NavigationData>(
        routeSpy.takeFirst().at(0)
        );
    QCOMPARE(teacherRoute.type, NodeType::Teacher);
    QCOMPARE(teacherRoute.teacherId, qint64{sourceTeacher.id});
    QCOMPARE(teacherRoute.keys, teacherKeys);
    QVERIFY(pages->isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(sidebar->selectedKeys(), teacherKeys);
    QCOMPARE(sidebar->getSelectedTeacherId(), qint64{sourceTeacher.id});

    TeacherInfoPage* const teacherPage = pages->teacherPage();
    QVERIFY(teacherPage);
    QCOMPARE(teacherPage->teacherId(), qint64{sourceTeacher.id});
    const QJsonObject teacherProfile = teacherProfileSnapshot(
        teacherPage->teacher()
        );
    QLineEdit* const teacherName = teacherPage->findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit")
        );
    QTextEdit* const teacherNotes = teacherPage->findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit")
        );
    QVERIFY(teacherName);
    QVERIFY(teacherNotes);
    const QString persistedName = teacherName->text();
    const QString persistedNotes = teacherNotes->toPlainText();
    const QString dirtyName = QStringLiteral("Dirty before Classes Cancel");
    const QString dirtyNotes = QStringLiteral(
        "Keep this draft after Classes Cancel"
        );
    teacherName->setText(dirtyName);
    teacherNotes->setPlainText(dirtyNotes);
    QVERIFY(teacherPage->hasUnsavedChanges());

    QWidget* const sourcePageWidget = pages->currentWidget();
    QVERIFY(sourcePageWidget);
    const QString sourcePageIdentifier = pages->currentPageIdentifier();
    const QStringList sourceSelectedKeys = sidebar->selectedKeys();
    const qint64 sourceSelectedTeacherId = sidebar->getSelectedTeacherId();
    QCOMPARE(sourceSelectedKeys, teacherKeys);
    QCOMPARE(sourceSelectedTeacherId, qint64{sourceTeacher.id});

    QTreeWidgetItem* const classesRootItem = findTopLevelItemByKey(
        tree,
        QStringLiteral("classes")
        );
    QVERIFY(classesRootItem);
    QCOMPARE(
        classesRootItem->data(0, Qt::UserRole).toInt(),
        static_cast<int>(NodeType::Page)
        );

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    QVERIFY(clickTreeItem(tree, classesRootItem));

    QCOMPARE(routeSpy.count(), 1);
    const NavigationData cancelledRoute = qvariant_cast<NavigationData>(
        routeSpy.takeFirst().at(0)
        );
    QCOMPARE(cancelledRoute.type, NodeType::Page);
    QCOMPARE(cancelledRoute.keys, QStringList{QStringLiteral("classes")});
    QCOMPARE(cancelledRoute.routeKey, QStringLiteral("classes"));
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());

    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(pages->currentWidget(), sourcePageWidget);
    QCOMPARE(pages->currentPageIdentifier(), sourcePageIdentifier);
    QVERIFY(teacherPage == pages->teacherPage());
    QCOMPARE(teacherPage->teacherId(), qint64{sourceTeacher.id});
    QCOMPARE(teacherProfileSnapshot(teacherPage->teacher()), teacherProfile);
    QCOMPARE(teacherName->text(), dirtyName);
    QCOMPARE(teacherNotes->toPlainText(), dirtyNotes);
    QVERIFY(teacherPage->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), sourceSelectedKeys);
    QCOMPARE(sidebar->getSelectedTeacherId(), sourceSelectedTeacherId);

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    QVERIFY(clickTreeItem(tree, classesRootItem));
    QCOMPARE(routeSpy.count(), 1);
    const NavigationData discardedRoute = qvariant_cast<NavigationData>(
        routeSpy.takeFirst().at(0)
        );
    QCOMPARE(discardedRoute.type, NodeType::Page);
    QCOMPARE(discardedRoute.keys, QStringList{QStringLiteral("classes")});
    QCOMPARE(discardedRoute.routeKey, QStringLiteral("classes"));
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());

    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("classes")}
        );
    QCOMPARE(sidebar->getSelectedTeacherId(), qint64{-1});
    QVERIFY(teacherPage == pages->teacherPage());
    QCOMPARE(teacherPage->teacherId(), qint64{sourceTeacher.id});
    QCOMPARE(teacherProfileSnapshot(teacherPage->teacher()), teacherProfile);
    QVERIFY(!teacherPage->hasUnsavedChanges());
    QCOMPARE(teacherName->text(), persistedName);
    QCOMPARE(teacherNotes->toPlainText(), persistedNotes);
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
sameTurnCleanReentryRestoresBeforeDeferredDelete()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("classes-fast-reentry.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    const auto createdClass = seedServices.classService()->create(
        QStringLiteral("Fast Reentry Class")
        );
    QVERIFY(createdClass);
    const auto originalInfo = seedServices.classService()->classInfo(
        *createdClass
        );
    QVERIFY(originalInfo);
    ClassInfo seededInfo = *originalInfo;
    seededInfo.classGrade = QStringLiteral("E4");
    seededInfo.classLevel = QStringLiteral("Theseus");
    seededInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
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

    PageManager* const pages = window.pageManager();
    ApplicationServices* const services = window.services();
    QVERIFY(pages);
    QVERIFY(services);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QCOMPARE(classesPage->currentClassId(), *createdClass);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);

    ClassInfoRepository* const repository =
        services->databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const ClassesPageRuntimeMetrics metricsBeforeLeave =
        classesPage->runtimeMetrics();
    const int detailReadsBeforeLeave =
        repository->classDetailsPageReadMetrics().callCount;
    QVERIFY(detailReadsBeforeLeave > 0);
    QPointer<ClassDetailsPage> releasedDetails =
        classesPage->findChild<ClassDetailsPage*>();
    QVERIFY(releasedDetails);
    auto* const originalGrade = releasedDetails->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    auto* const originalLevel = releasedDetails->findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    QVERIFY(originalGrade);
    QVERIFY(originalLevel);
    QCOMPARE(originalGrade->currentText(), QStringLiteral("E4"));
    QCOMPARE(originalLevel->currentText(), QStringLiteral("Theseus"));

    pages->showPage(PageType::MyWorkspace);
    QVERIFY(!releasedDetails.isNull());
    QCOMPARE(classesPage->runtimeMetrics().instantiatedEditorCount, 0);
    QCOMPARE(
        classesPage->runtimeMetrics().selectedEditorDescendantWidgetCount,
        0
        );

    pages->showPage(PageType::Classes);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    QCOMPARE(
        repository->classDetailsPageReadMetrics().callCount,
        detailReadsBeforeLeave
        );
    QCOMPARE(classesPage->runtimeMetrics().instantiatedEditorCount, 1);
    QCOMPARE(classesPage->runtimeMetrics().loadedEditorClassCount, 1);
    QVERIFY(
        classesPage->runtimeMetrics().selectedEditorDescendantWidgetCount > 0
        );

    const QList<ClassDetailsPage*> detailsPages =
        classesPage->findChildren<ClassDetailsPage*>();
    QCOMPARE(detailsPages.size(), 2);
    ClassDetailsPage* restoredDetails = nullptr;
    for (ClassDetailsPage* details : detailsPages)
    {
        if (details != releasedDetails.data())
        {
            restoredDetails = details;
        }
    }
    QVERIFY(restoredDetails);
    QStackedWidget* const editorStack =
        classesPage->findChild<QStackedWidget*>();
    QVERIFY(editorStack);
    QCOMPARE(editorStack->currentWidget(), restoredDetails);
    QVERIFY(restoredDetails->isVisible());
    auto* const restoredGrade = restoredDetails->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    auto* const restoredLevel = restoredDetails->findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    QVERIFY(restoredGrade);
    QVERIFY(restoredLevel);
    QCOMPARE(restoredGrade->currentText(), QStringLiteral("E4"));
    QCOMPARE(restoredLevel->currentText(), QStringLiteral("Theseus"));

    QCoreApplication::sendPostedEvents(
        releasedDetails.data(),
        QEvent::DeferredDelete
        );
    QVERIFY(releasedDetails.isNull());
    QCOMPARE(
        classesPage->findChildren<ClassDetailsPage*>().size(),
        1
        );
    QCOMPARE(editorStack->currentWidget(), restoredDetails);
    QCOMPARE(restoredGrade->currentText(), QStringLiteral("E4"));
    QCOMPARE(restoredLevel->currentText(), QStringLiteral("Theseus"));
    QCOMPARE(classesPage->runtimeMetrics().instantiatedEditorCount, 1);
    QCOMPARE(classesPage->runtimeMetrics().loadedEditorClassCount, 1);
    QVERIFY(
        classesPage->runtimeMetrics().selectedEditorDescendantWidgetCount > 0
        );
    QCOMPARE(
        repository->classDetailsPageReadMetrics().callCount,
        detailReadsBeforeLeave
        );
    QCOMPARE(
        classesPage->runtimeMetrics().classQueryCount,
        metricsBeforeLeave.classQueryCount
        );
    QCOMPARE(
        classesPage->runtimeMetrics().classInfoQueryCount,
        metricsBeforeLeave.classInfoQueryCount
        );
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

void MainWindowClassesSidebarRootNavigationTests::
classReadFailurePreservesAddClassRoute()
{
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope(&prompts);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("new-class-read-failure.tps"))
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
    QVERIFY(window.isVisible());

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    ClassService* const classService = services->classService();
    QVERIFY(classService);
    const auto classesBefore = classService->classes();
    QVERIFY(classesBefore);
    QCOMPARE(classesBefore->size(), 0);

    QSqlQuery deleteInsertedClass(activeSession->database());
    QVERIFY2(
        deleteInsertedClass.exec(QStringLiteral(
            "CREATE TRIGGER delete_created_class_after_insert "
            "AFTER INSERT ON classes "
            "BEGIN DELETE FROM classes WHERE id = NEW.id; END"
            )),
        qPrintable(deleteInsertedClass.lastError().text())
        );

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QWidget* const currentPageWidget = pages->currentWidget();
    QVERIFY(currentPageWidget);
    const QString pageIdentifier = pages->currentPageIdentifier();
    const int instantiatedPageCount = pages->instantiatedPageCount();
    const bool classesPageInstantiated =
        pages->isPageInstantiated(PageType::Classes);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList selectedKeys = sidebar->selectedKeys();

    QAction* const newClassAction = window.actions().newClass;
    QVERIFY(newClassAction);
    QVERIFY(newClassAction->isEnabled());
    newClassAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Add Class"));
    QCOMPARE(
        warning.message,
        QStringLiteral("The created class could not be loaded.")
        );
    QVERIFY(warning.details.contains(
        QStringLiteral("no matching record exists.")
        ));

    const auto classesAfter = classService->classes();
    QVERIFY(classesAfter);
    QCOMPARE(classesAfter->size(), 0);
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentPageWidget);
    QCOMPARE(pages->currentPageIdentifier(), pageIdentifier);
    QCOMPARE(pages->instantiatedPageCount(), instantiatedPageCount);
    QCOMPARE(
        pages->isPageInstantiated(PageType::Classes),
        classesPageInstantiated
        );
    QCOMPARE(sidebar->selectedKeys(), selectedKeys);
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(!QApplication::activeModalWidget());
}

QTEST_MAIN(MainWindowClassesSidebarRootNavigationTests)

#include "mainwindow_classes_sidebar_root_navigation_tests.moc"
