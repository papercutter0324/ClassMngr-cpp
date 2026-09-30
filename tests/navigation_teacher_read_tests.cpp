#include "app/controllers/navigation_controller.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QLineEdit>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("navigation-teacher-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacherFixture(
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredName
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.preferredName = preferredName;
    teacher.roomNumber = QStringLiteral("Room 2");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Teacher Wi-Fi");
    teacher.wifiPassword = QStringLiteral("Teacher password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("teacher.zoom");
    teacher.zoomPassword = QStringLiteral("Zoom password");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.notes = QStringLiteral("Teacher notes");
    return teacher;
}

int persistTeacher(ApplicationServices& services, Teacher& teacher)
{
    const auto saved = services.databaseSession()->teacherRepository()
        ->createTeacher(teacher);
    if (!saved)
    {
        return -1;
    }
    teacher.id = *saved;
    return *saved;
}

}

class NavigationTeacherReadTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void rawNonpositiveAndMissingIdsReturnBeforeLeaveConfirmation();
    void successfulReadConfirmsBeforeLoadingAndShowingTeacher();
};

void NavigationTeacherReadTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void NavigationTeacherReadTests::
rawNonpositiveAndMissingIdsReturnBeforeLeaveConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Teacher selected = teacherFixture(
        QStringLiteral("김선택"),
        QStringLiteral("Selected Teacher"),
        QStringLiteral("Selected Display")
        );
    QVERIFY(persistTeacher(services, selected) > 0);

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    auto* page = pages.teacherPage();
    QVERIFY(page);
    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(selected);
    auto* notes = page->findChild<QTextEdit*>(QStringLiteral("teacherNotesEdit"));
    QVERIFY(notes);
    notes->setPlainText(QStringLiteral("Unsaved change"));
    QVERIFY(page->hasUnsavedChanges());

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    for (const int invalidId : {0, -2})
    {
        navigation.handleNavigation({
            .type = NodeType::Teacher,
            .teacherId = invalidId
        });
        QCOMPARE(prompts.unsavedChangesConfirmations.size(), 0);
        QVERIFY(page->hasUnsavedChanges());
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QCOMPARE(page->teacher().id, selected.id);
    }

    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = selected.id + 1000
    });
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 0);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, selected.id);
}

void NavigationTeacherReadTests::
successfulReadConfirmsBeforeLoadingAndShowingTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Teacher selected = teacherFixture(
        QStringLiteral("김선택"),
        QStringLiteral("Selected Teacher"),
        QStringLiteral("Selected Display")
        );
    Teacher target = teacherFixture(
        QStringLiteral("박대상"),
        QStringLiteral("Target Teacher"),
        QStringLiteral("Target Display")
        );
    QVERIFY(persistTeacher(services, selected) > 0);
    QVERIFY(persistTeacher(services, target) > 0);

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    auto* page = pages.teacherPage();
    QVERIFY(page);
    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(selected);
    auto* notes = page->findChild<QTextEdit*>(QStringLiteral("teacherNotesEdit"));
    QVERIFY(notes);
    notes->setPlainText(QStringLiteral("Unsaved change"));
    QVERIFY(page->hasUnsavedChanges());

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel);

    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = target.id
    });
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(prompts.unsavedChangesConfirmations.first().title,
        QStringLiteral("Unsaved Teacher Changes"));
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(page->teacher().id, selected.id);

    page->discardChanges();
    QVERIFY(!page->hasUnsavedChanges());
    pages.showPage(PageType::MyWorkspace);
    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = target.id
    });

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, target.id);
    QCOMPARE(page->teacher().teacherEn, target.teacherEn);
    QCOMPARE(page->teacher().preferredName, target.preferredName);
    auto* header = page->findChild<PageHeader*>();
    QVERIFY(header);
    QVERIFY(header->title().contains(QStringLiteral("Target Display")));
    QVERIFY(!page->hasUnsavedChanges());
}

QTEST_MAIN(NavigationTeacherReadTests)

#include "navigation_teacher_read_tests.moc"
