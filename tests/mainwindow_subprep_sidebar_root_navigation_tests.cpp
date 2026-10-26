#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QFileInfo>
#include <QJsonObject>
#include <QLineEdit>
#include <QRect>
#include <QSignalSpy>
#include <QTextEdit>
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
}

class MainWindowSubPrepSidebarRootNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void renderedSubPrepRootClickDispatchesToSubPrepPage();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowSubPrepSidebarRootNavigationTests::initTestCase()
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

void MainWindowSubPrepSidebarRootNavigationTests::
renderedSubPrepRootClickDispatchesToSubPrepPage()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("sub-prep-root.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    Teacher sourceTeacher;
    sourceTeacher.teacherKr = QStringLiteral("김서브프렙취소");
    sourceTeacher.teacherEn = QStringLiteral("Sub Prep Cancel Teacher");
    sourceTeacher.preferredRomanization = QStringLiteral("Seo Prep Cancel");
    sourceTeacher.preferredName = QStringLiteral("Sub Prep Cancel");
    sourceTeacher.roomNumber = QStringLiteral("Room 42");
    sourceTeacher.notes = QStringLiteral("Persisted Sub Prep cancel profile");
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

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    MainWindow window(
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
    const QString dirtyName = QStringLiteral("Dirty before Sub Prep Cancel");
    const QString dirtyNotes = QStringLiteral(
        "Keep this draft after Sub Prep Cancel"
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

    QTreeWidgetItem* const rootItem = findTopLevelItemByKey(
        tree,
        QStringLiteral("sub_prep")
        );
    QVERIFY(rootItem);
    QCOMPARE(
        rootItem->data(0, Qt::UserRole).toInt(),
        static_cast<int>(NodeType::Page)
        );

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    QVERIFY(clickTreeItem(tree, rootItem));

    QCOMPARE(routeSpy.count(), 1);
    const NavigationData cancelledRoute = qvariant_cast<NavigationData>(
        routeSpy.takeFirst().at(0)
        );
    QCOMPARE(cancelledRoute.type, NodeType::Page);
    QCOMPARE(cancelledRoute.path, QStringList{rootItem->text(0)});
    QCOMPARE(cancelledRoute.keys, QStringList{QStringLiteral("sub_prep")});
    QCOMPARE(cancelledRoute.routeKey, QStringLiteral("sub_prep"));
    QCOMPARE(cancelledRoute.classId, -1);
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
    QVERIFY(clickTreeItem(tree, rootItem));
    QCOMPARE(routeSpy.size(), 1);
    const NavigationData discardedRoute = qvariant_cast<NavigationData>(
        routeSpy.takeFirst().at(0)
        );
    QCOMPARE(discardedRoute.type, NodeType::Page);
    QCOMPARE(discardedRoute.keys, QStringList{QStringLiteral("sub_prep")});
    QCOMPARE(discardedRoute.routeKey, QStringLiteral("sub_prep"));
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());

    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::SubPrep));
    SubPrepPage* const subPrepPage = pages->subPrepPage();
    QVERIFY(subPrepPage);
    QCOMPARE(pages->currentWidget(), subPrepPage);
    QCOMPARE(
        subPrepPage->currentSectionKey(),
        QStringLiteral("sub_prep_important")
        );
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("sub_prep")}
        );
    QCOMPARE(sidebar->getSelectedTeacherId(), qint64{-1});
    QVERIFY(teacherPage == pages->teacherPage());
    QVERIFY(!teacherPage->hasUnsavedChanges());
    QCOMPARE(teacherName->text(), persistedName);
    QCOMPARE(teacherNotes->toPlainText(), persistedNotes);
    QCOMPARE(teacherProfileSnapshot(teacherPage->teacher()), teacherProfile);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
}

QTEST_MAIN(MainWindowSubPrepSidebarRootNavigationTests)

#include "mainwindow_subprep_sidebar_root_navigation_tests.moc"
