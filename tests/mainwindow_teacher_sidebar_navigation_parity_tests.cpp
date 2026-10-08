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
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QJsonObject>
#include <QLineEdit>
#include <QRect>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtTest>

#include <array>
#include <utility>

namespace
{
Teacher teacherFixture(
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredName,
    const QString& notes
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.preferredRomanization = englishName + QStringLiteral(" Roman");
    teacher.preferredName = preferredName;
    teacher.roomNumber = QStringLiteral("Room 21");
    teacher.birthday = QStringLiteral("06-18");
    teacher.phoneNumber = QStringLiteral("010-4567-9876");
    teacher.wifiName = QStringLiteral("Teacher Wi-Fi");
    teacher.wifiPassword = QStringLiteral("Teacher Wi-Fi password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("teacher.zoom");
    teacher.zoomPassword = QStringLiteral("Teacher Zoom password");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = notes;
    return teacher;
}

int persistTeacher(
    ApplicationServices& services,
    Teacher& teacher
    )
{
    const auto saved = services.databaseSession()
        ->teacherRepository()
        ->createTeacher(teacher);
    if (!saved)
    {
        return -1;
    }

    teacher.id = *saved;
    return *saved;
}

bool assignTeacherToClass(
    ApplicationServices& services,
    const int teacherId
    )
{
    const auto created = services.classService()->create(
        QStringLiteral("F403 assigned class")
        );
    if (!created)
    {
        return false;
    }

    ClassInfo info;
    info.classId = *created;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Orion");
    info.classTimes = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("5:00 PM")
        }
    };
    return static_cast<bool>(
        services.databaseSession()
            ->classInfoRepository()
            ->saveClassInfo(info)
        );
}

QTreeWidgetItem* findItemByKey(
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
        if (child->data(0, Qt::UserRole + 4).toString() == key)
        {
            return child;
        }
    }

    return nullptr;
}

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
        if (item->data(0, Qt::UserRole + 4).toString() == key)
        {
            return item;
        }
    }

    return nullptr;
}

QTreeWidgetItem* findTeacherLeaf(
    QTreeWidget* tree,
    const QStringList& keyPath,
    const int teacherId
    )
{
    if (keyPath.isEmpty())
    {
        return nullptr;
    }

    QTreeWidgetItem* item = findTopLevelItemByKey(
        tree,
        keyPath.constFirst()
        );
    for (qsizetype index = 1; item && index < keyPath.size(); ++index)
    {
        const QString& key = keyPath.at(index);
        if (index == keyPath.size() - 1)
        {
            QTreeWidgetItem* teacherLeaf = nullptr;
            for (int childIndex = 0;
                 childIndex < item->childCount();
                 ++childIndex)
            {
                QTreeWidgetItem* const child = item->child(childIndex);
                if (child->data(0, Qt::UserRole + 4).toString() == key
                    && child->data(0, Qt::UserRole + 3).toInt()
                        == teacherId)
                {
                    teacherLeaf = child;
                    break;
                }
            }
            item = teacherLeaf;
        }
        else
        {
            item = findItemByKey(item, key);
        }
    }

    if (!item
        || item->data(0, Qt::UserRole).toInt()
            != static_cast<int>(NodeType::Teacher)
        || item->data(0, Qt::UserRole + 3).toInt() != teacherId)
    {
        return nullptr;
    }

    return item;
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

QJsonObject teacherSnapshot(const Teacher& teacher)
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

QJsonObject teacherFormSnapshot(TeacherInfoPage& page)
{
    QJsonObject snapshot;
    for (const QLineEdit* edit : page.findChildren<QLineEdit*>())
    {
        if (!edit->objectName().isEmpty())
        {
            snapshot.insert(
                QStringLiteral("line:%1").arg(edit->objectName()),
                edit->text()
                );
        }
    }
    for (const QComboBox* combo : page.findChildren<QComboBox*>())
    {
        if (!combo->objectName().isEmpty())
        {
            snapshot.insert(
                QStringLiteral("combo:%1").arg(combo->objectName()),
                QJsonObject{
                    {QStringLiteral("text"), combo->currentText()},
                    {QStringLiteral("data"), combo->currentData().toString()}
                }
                );
        }
    }
    for (const QTextEdit* edit : page.findChildren<QTextEdit*>())
    {
        if (!edit->objectName().isEmpty())
        {
            snapshot.insert(
                QStringLiteral("text:%1").arg(edit->objectName()),
                edit->toPlainText()
                );
        }
    }
    return snapshot;
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

class MainWindowTeacherSidebarNavigationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void refreshedTeacherLeavesDispatchCancelAndDiscard();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowTeacherSidebarNavigationParityTests::initTestCase()
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

void MainWindowTeacherSidebarNavigationParityTests::
refreshedTeacherLeavesDispatchCancelAndDiscard()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("teacher-sidebar-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));

    Teacher sourceTeacher = teacherFixture(
        QStringLiteral("김출발"),
        QStringLiteral("Source Teacher"),
        QStringLiteral("Source Display"),
        QStringLiteral("Persisted source profile")
        );
    Teacher assignedTeacher = teacherFixture(
        QStringLiteral("박배정"),
        QStringLiteral("Assigned Teacher"),
        QStringLiteral("Assigned Display"),
        QStringLiteral("Persisted assigned profile")
        );
    Teacher unassignedTeacher = teacherFixture(
        QStringLiteral("이무배정"),
        QStringLiteral("Unassigned Teacher"),
        QStringLiteral("Unassigned Display"),
        QStringLiteral("Persisted unassigned profile")
        );

    QVERIFY(persistTeacher(seedServices, sourceTeacher) > 0);
    QVERIFY(persistTeacher(seedServices, assignedTeacher) > 0);
    QVERIFY(persistTeacher(seedServices, unassignedTeacher) > 0);
    QVERIFY(assignTeacherToClass(seedServices, assignedTeacher.id));

    const auto persistedSource = seedServices.teacherService()->teacher(
        sourceTeacher.id
        );
    QVERIFY(persistedSource.has_value());
    const QJsonObject sourceProfileSnapshot =
        teacherSnapshot(*persistedSource);
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

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

    const QStringList coTeacherKeys{
        QStringLiteral("co_teachers"),
        QStringLiteral("teacher")
    };
    const QStringList koreanTeacherKeys{
        QStringLiteral("campus_staff"),
        QStringLiteral("teachers_all_korean"),
        QStringLiteral("teacher")
    };
    QTreeWidgetItem* const assignedLeaf = findTeacherLeaf(
        tree,
        coTeacherKeys,
        assignedTeacher.id
        );
    QTreeWidgetItem* const unassignedLeaf = findTeacherLeaf(
        tree,
        koreanTeacherKeys,
        unassignedTeacher.id
        );
    QTreeWidgetItem* const sourceLeaf = findTeacherLeaf(
        tree,
        koreanTeacherKeys,
        sourceTeacher.id
        );
    QVERIFY(assignedLeaf);
    QVERIFY(unassignedLeaf);
    QVERIFY(sourceLeaf);
    QCOMPARE(
        assignedLeaf->data(0, Qt::UserRole + 3).toInt(),
        assignedTeacher.id
        );
    QCOMPARE(
        unassignedLeaf->data(0, Qt::UserRole + 3).toInt(),
        unassignedTeacher.id
        );
    QVERIFY(sourceLeaf != unassignedLeaf);

    QSignalSpy routeSpy(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());

    struct TargetCase
    {
        QStringList keyPath;
        QTreeWidgetItem* leaf = nullptr;
        int teacherId = -1;
        QString expectedTeacherName;
        QString draftName;
        QString draftNotes;
    };
    const std::array targetCases{
        TargetCase{
            coTeacherKeys,
            assignedLeaf,
            assignedTeacher.id,
            assignedTeacher.teacherEn,
            QStringLiteral("Edited before Co-Teacher navigation"),
            QStringLiteral("Exact Co-Teacher route draft")
        },
        TargetCase{
            koreanTeacherKeys,
            unassignedLeaf,
            unassignedTeacher.id,
            unassignedTeacher.teacherEn,
            QStringLiteral("Edited before Korean Teacher navigation"),
            QStringLiteral("Exact Korean Teacher route draft")
        }
    };

    for (const TargetCase& targetCase : targetCases)
    {
        QVERIFY(clickTreeItem(tree, sourceLeaf));
        QCOMPARE(routeSpy.size(), 1);
        const NavigationData sourceRoute =
            qvariant_cast<NavigationData>(routeSpy.takeFirst().at(0));
        QCOMPARE(sourceRoute.type, NodeType::Teacher);
        QCOMPARE(sourceRoute.teacherId, sourceTeacher.id);
        QCOMPARE(sourceRoute.keys, koreanTeacherKeys);
        QCOMPARE(sourceRoute.routeKey, QStringLiteral("teacher"));
        QVERIFY(pages->isCurrentPage(PageType::TeacherInfo));

        TeacherInfoPage* const teacherPage = pages->teacherPage();
        QVERIFY(teacherPage);
        QCOMPARE(teacherPage->teacher().id, sourceTeacher.id);
        QLineEdit* const teacherName = teacherPage->findChild<QLineEdit*>(
            QStringLiteral("teacherEnEdit")
            );
        QTextEdit* const teacherNotes = teacherPage->findChild<QTextEdit*>(
            QStringLiteral("teacherNotesEdit")
            );
        QVERIFY(teacherName);
        QVERIFY(teacherNotes);

        teacherName->setText(targetCase.draftName);
        teacherNotes->setPlainText(targetCase.draftNotes);
        QVERIFY(teacherPage->hasUnsavedChanges());
        const QJsonObject dirtyForm = teacherFormSnapshot(*teacherPage);

        QVERIFY(targetCase.leaf);
        QVERIFY(targetCase.leaf != sourceLeaf);
        QCOMPARE(
            targetCase.leaf->data(0, Qt::UserRole + 3).toInt(),
            targetCase.teacherId
            );

        const int expectedPromptCount =
            static_cast<int>(prompts.unsavedChangesConfirmations.size()) + 1;
        prompts.scriptedUnsavedChangesChoices.enqueue(
            UnsavedChangesChoice::Cancel
            );
        QVERIFY(clickTreeItem(tree, targetCase.leaf));
        QCOMPARE(routeSpy.size(), 1);
        const NavigationData cancelledRoute =
            qvariant_cast<NavigationData>(routeSpy.takeFirst().at(0));
        QCOMPARE(cancelledRoute.type, NodeType::Teacher);
        QCOMPARE(cancelledRoute.teacherId, targetCase.teacherId);
        QCOMPARE(cancelledRoute.keys, targetCase.keyPath);
        QCOMPARE(cancelledRoute.routeKey, QStringLiteral("teacher"));
        QCOMPARE(
            prompts.unsavedChangesConfirmations.size(),
            expectedPromptCount
            );
        QVERIFY(prompts.messages.isEmpty());
        QVERIFY(prompts.asynchronousMessages.isEmpty());
        QVERIFY(prompts.confirmations.isEmpty());
        QVERIFY(prompts.actionPrompts.isEmpty());
        QVERIFY(services == window.services());
        QVERIFY(services->hasOpenDatabase());
        QVERIFY(services->databaseSession() == activeSession);
        QCOMPARE(services->currentDatabasePath(), workspacePath);
        QVERIFY(pages->isCurrentPage(PageType::TeacherInfo));
        QVERIFY(teacherPage == pages->teacherPage());
        QCOMPARE(teacherPage->teacher().id, sourceTeacher.id);
        QCOMPARE(teacherFormSnapshot(*teacherPage), dirtyForm);
        QVERIFY(teacherPage->hasUnsavedChanges());

        prompts.scriptedUnsavedChangesChoices.enqueue(
            UnsavedChangesChoice::Discard
            );
        QVERIFY(clickTreeItem(tree, targetCase.leaf));
        QCOMPARE(routeSpy.size(), 1);
        const NavigationData discardedRoute =
            qvariant_cast<NavigationData>(routeSpy.takeFirst().at(0));
        QCOMPARE(discardedRoute.type, NodeType::Teacher);
        QCOMPARE(discardedRoute.teacherId, targetCase.teacherId);
        QCOMPARE(discardedRoute.keys, targetCase.keyPath);
        QCOMPARE(discardedRoute.routeKey, QStringLiteral("teacher"));
        QCOMPARE(
            prompts.unsavedChangesConfirmations.size(),
            expectedPromptCount + 1
            );
        QVERIFY(prompts.messages.isEmpty());
        QVERIFY(prompts.asynchronousMessages.isEmpty());
        QVERIFY(prompts.confirmations.isEmpty());
        QVERIFY(prompts.actionPrompts.isEmpty());
        QVERIFY(services == window.services());
        QVERIFY(services->hasOpenDatabase());
        QVERIFY(services->databaseSession() == activeSession);
        QCOMPARE(services->currentDatabasePath(), workspacePath);
        QVERIFY(pages->isCurrentPage(PageType::TeacherInfo));
        QCOMPARE(teacherPage->teacher().id, targetCase.teacherId);
        QCOMPARE(
            teacherPage->teacher().teacherEn,
            targetCase.expectedTeacherName
            );
        QVERIFY(!teacherPage->hasUnsavedChanges());

        const auto persistedAfterDiscard = services->teacherService()->teacher(
            sourceTeacher.id
            );
        QVERIFY(persistedAfterDiscard.has_value());
        QCOMPARE(
            teacherSnapshot(*persistedAfterDiscard),
            sourceProfileSnapshot
            );
    }
}

QTEST_MAIN(MainWindowTeacherSidebarNavigationParityTests)

#include "mainwindow_teacher_sidebar_navigation_parity_tests.moc"
