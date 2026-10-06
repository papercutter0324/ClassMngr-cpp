#include "app/controllers/sidebar_controller.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "fakes/fake_user_prompt_service.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <memory>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sidebar-teacher-delete-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

QString jsonBool(const bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

int createTeacher(
    TeacherRepository* repository,
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredName
    )
{
    if (!repository)
    {
        return -1;
    }
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.preferredName = preferredName;
    const auto created = repository->createTeacher(teacher);
    return created ? *created : -1;
}

class DeleteFixture final
{
public:
    [[nodiscard]] bool open()
    {
        if (!services.openDatabase(databasePath(directory)))
        {
            return false;
        }
        TeacherRepository* const repository =
            services.databaseSession()->teacherRepository();
        targetId = createTeacher(
            repository,
            QStringLiteral("Target Korean"),
            QStringLiteral("Target English"),
            QStringLiteral("Target Display")
            );
        siblingId = createTeacher(
            repository,
            QStringLiteral("Sibling Korean"),
            QStringLiteral("Sibling English"),
            QStringLiteral("Sibling Display")
            );
        if (targetId <= 0 || siblingId <= 0)
        {
            return false;
        }

        pages.initialize(&services, false);
        pages.showPage(PageType::TeacherInfo);
        teacherPage = pages.teacherPage();
        if (!teacherPage)
        {
            return false;
        }
        const auto target = repository->getTeacher(targetId);
        if (!target)
        {
            return false;
        }
        teacherPage->loadTeacher(*target);
        if (teacherPage->hasUnsavedChanges())
        {
            return false;
        }

        controller = std::make_unique<SidebarController>(
            &services,
            &sidebar,
            &pages
            );
        controller->refreshTeacherSidebar();
        sidebar.selectTeacher(targetId);
        if (sidebar.getSelectedTeacherId() != targetId)
        {
            return false;
        }

        DialogServices::setUserPromptServiceForTesting(&prompts);
        return true;
    }

    [[nodiscard]] bool invokeDeleteTeacher() const
    {
        return QMetaObject::invokeMethod(
            controller.get(),
            "deleteTeacher",
            Qt::DirectConnection
            );
    }

    [[nodiscard]] TeacherRepository* repository() const
    {
        return services.databaseSession()->teacherRepository();
    }

    [[nodiscard]] bool targetExists() const
    {
        return repository()->getTeacher(targetId).has_value();
    }

    [[nodiscard]] bool siblingExists() const
    {
        return repository()->getTeacher(siblingId).has_value();
    }

    QTemporaryDir directory;
    ApplicationServices services;
    PageManager pages;
    Sidebar sidebar;
    FakeUserPromptService prompts;
    std::unique_ptr<SidebarController> controller;
    TeacherInfoPage* teacherPage = nullptr;
    int targetId = -1;
    int siblingId = -1;
};

void emitTranscript(const QString& transcript)
{
    qInfo().noquote() << QStringLiteral("F363_TRANSCRIPT=") + transcript;
}

}

class SidebarTeacherDeleteParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void confirmationCancelPreservesTeacherAndSidebar();
    void writeFailureWarnsWithoutRefreshingOrNavigating();
    void successRefreshesSidebarAndKeepsCurrentPage();
};

void SidebarTeacherDeleteParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void SidebarTeacherDeleteParityTests::
confirmationCancelPreservesTeacherAndSidebar()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Rejected);

    QVERIFY(fixture.invokeDeleteTeacher());

    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fixture.prompts.messages.isEmpty());
    QVERIFY(fixture.targetExists());
    QVERIFY(fixture.siblingExists());
    QCOMPARE(fixture.sidebar.getSelectedTeacherId(), fixture.targetId);
    QVERIFY(fixture.pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(!fixture.teacherPage->hasUnsavedChanges());

    emitTranscript(QStringLiteral(
        "{\"case\":\"confirmation_cancel\",\"confirmations\":1,"
        "\"leave_prompts\":0,\"warnings\":0,\"target_present\":true,"
        "\"sidebar_target_selected\":true}"
        ));
}

void SidebarTeacherDeleteParityTests::
writeFailureWarnsWithoutRefreshingOrNavigating()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    QSqlQuery trigger(fixture.services.databaseSession()->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_sidebar_teacher_delete "
        "BEFORE DELETE ON teachers WHEN OLD.id=%1 "
        "BEGIN SELECT RAISE(ABORT, 'F363 sidebar teacher delete failure'); END"
        ).arg(fixture.targetId)));
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Destructive);

    QVERIFY(fixture.invokeDeleteTeacher());

    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const PromptRequest& warning = fixture.prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Delete Teacher"));
    QCOMPARE(warning.message, QStringLiteral("The teacher could not be deleted."));
    QVERIFY(!warning.details.trimmed().isEmpty());
    QVERIFY(fixture.targetExists());
    QVERIFY(fixture.siblingExists());
    QCOMPARE(fixture.sidebar.getSelectedTeacherId(), fixture.targetId);
    QVERIFY(fixture.pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(fixture.teacherPage->teacher().id, fixture.targetId);
    QVERIFY(!fixture.teacherPage->hasUnsavedChanges());

    emitTranscript(QStringLiteral(
        "{\"case\":\"write_failure\",\"confirmations\":1,"
        "\"leave_prompts\":0,\"warnings\":1,"
        "\"warning\":\"The teacher could not be deleted.\","
        "\"warning_details\":%1,\"target_present\":true,"
        "\"sidebar_target_selected\":true}"
        ).arg(jsonBool(!warning.details.trimmed().isEmpty())));
}

void SidebarTeacherDeleteParityTests::
successRefreshesSidebarAndKeepsCurrentPage()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Destructive);

    QVERIFY(fixture.invokeDeleteTeacher());

    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fixture.prompts.messages.isEmpty());
    QVERIFY(!fixture.targetExists());
    QVERIFY(fixture.siblingExists());
    QVERIFY(fixture.pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(fixture.teacherPage->teacher().id, fixture.targetId);
    QVERIFY(!fixture.teacherPage->hasUnsavedChanges());

    fixture.sidebar.selectTeacher(fixture.siblingId);
    QCOMPARE(fixture.sidebar.getSelectedTeacherId(), fixture.siblingId);
    fixture.sidebar.selectTeacher(fixture.targetId);
    const bool targetRemainsAbsent =
        fixture.sidebar.getSelectedTeacherId() == fixture.siblingId;
    QVERIFY(targetRemainsAbsent);

    emitTranscript(QStringLiteral(
        "{\"case\":\"success\",\"confirmations\":1,"
        "\"leave_prompts\":0,\"warnings\":0,\"target_removed\":true,"
        "\"sibling_preserved\":true,\"sidebar_target_removed\":%1,"
        "\"sidebar_sibling_selected\":true,"
        "\"page\":\"teacher_info\",\"page_retained\":true,"
        "\"page_dirty\":false}"
        ).arg(jsonBool(targetRemainsAbsent)));
}

QTEST_MAIN(SidebarTeacherDeleteParityTests)

#include "sidebar_teacher_delete_parity_tests.moc"
