#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/testing_class_create.h"
#include "next/application/testing_class_delete.h"
#include "next/application/testing_class_details_update.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <optional>
#include <string>

namespace ScheduleWidgetTestStubs
{
void reset();
}

namespace
{
using namespace ClassMngr::Next;

TestingClass storedClass(const QString& name)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M1");
    value.level = QStringLiteral("Major");
    value.room = QStringLiteral("401");
    value.teacherId = -1;
    value.classColor = QStringLiteral("#336699");
    value.fontColor = QStringLiteral("#FFFFFF");
    value.notes = QStringLiteral("Stored notes");
    return value;
}

QString temporaryDatabasePath(QTemporaryDir& directory, const QString& name)
{
    return directory.filePath(name + QStringLiteral(".tps"));
}

QListWidget* classList(TestingClassesPage& page)
{
    return page.findChild<QListWidget*>(QStringLiteral("testingClassesList"));
}

QLineEdit* classNameEdit(TestingClassesPage& page)
{
    return page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
}

QPushButton* deleteButton(TestingClassesPage& page)
{
    return page.findChild<QPushButton*>(
        QStringLiteral("testingClassesDeleteButton")
        );
}

int selectedClassId(TestingClassesPage& page)
{
    QListWidget* const list = classList(page);
    return list && list->currentItem()
        ? list->currentItem()->data(Qt::UserRole).toInt()
        : -1;
}

QString warningSeverity(const PromptRequest& warning)
{
    return warning.severity == PromptSeverity::Warning
        ? QStringLiteral("warning")
        : QStringLiteral("other");
}

void logTranscript(const QJsonObject& transcript)
{
    qInfo().noquote()
        << QStringLiteral("F360_TRANSCRIPT ")
            + QString::fromUtf8(
                QJsonDocument(transcript).toJson(QJsonDocument::Compact)
                );
}

class FailingTestingClassDetailsUpdatePort final
    : public Application::TestingClassDetailsUpdatePort
{
public:
    [[nodiscard]] Application::TestingClassDetailsUpdateResult
    updateTestingClassDetails(
        const Application::TestingClassDetailsUpdateRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return Application::TestingClassDetailsUpdateResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "injected testing class details update failure",
            .recoverable = false
        });
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingClassDetailsUpdateRequest>
        lastRequest;
};

class FailingTestingClassCreatePort final
    : public Application::TestingClassCreatePort
{
public:
    [[nodiscard]] Application::TestingClassCreateResult createTestingClass(
        const Application::TestingClassCreateRequest& request
        ) const override
    {
        ++callCount;
        requests.append(request);
        return Application::TestingClassCreateResult::failure({
            .code = Domain::ErrorCode::Conflict,
            .message = "injected testing class create failure",
            .recoverable = true
        });
    }

    mutable int callCount = 0;
    mutable QList<Application::TestingClassCreateRequest> requests;
};

class FailingTestingClassDeletePort final
    : public Application::TestingClassDeletePort
{
public:
    [[nodiscard]] Application::TestingClassDeleteResult deleteTestingClass(
        const Application::TestingClassDeleteRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return Application::TestingClassDeleteResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "injected testing class delete failure",
            .recoverable = false
        });
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingClassDeleteRequest> lastRequest;
};
}

class TestingClassesPageCancelFailureParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        ScheduleWidgetTestStubs::reset();
    }

    void cleanup()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    void f145UpdateFailureRetainsDraftAndShowsWarning();
    void f146CreateFailureRetainsDraftAndPendingSlot();
    void f147DeleteCancelDoesNotMutate();
    void f147DeleteFailureRetainsDraftAndShowsWarning();
};

void TestingClassesPageCancelFailureParityTests::
f145UpdateFailureRetainsDraftAndShowsWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        temporaryDatabasePath(directory, QStringLiteral("f145-update"))
        ));
    auto* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto classId = repository->createTestingClass(
        storedClass(QStringLiteral("Stored Class"))
        );
    QVERIFY(classId);

    FailingTestingClassDetailsUpdatePort updatePort;
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort
        );
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(*classId);
    page.refresh();

    QLineEdit* const name = classNameEdit(page);
    QVERIFY(name);
    QCOMPARE(selectedClassId(page), *classId);
    name->setText(QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());

    QCOMPARE(updatePort.callCount, 1);
    QVERIFY(updatePort.lastRequest.has_value());
    QCOMPARE(updatePort.lastRequest->classId.value(), std::to_string(*classId));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Save Testing Class"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class details update failure"));
    QCOMPARE(name->text(), QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selectedClassId(page), *classId);

    const auto unchanged = repository->loadTestingClass(*classId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->name, QStringLiteral("Stored Class"));

    logTranscript({
        {QStringLiteral("scenario"), QStringLiteral("F145-update-failure")},
        {QStringLiteral("warning_count"), prompts.messages.size()},
        {QStringLiteral("warning_title"), prompts.messages.constFirst().title},
        {QStringLiteral("warning_severity"),
         warningSeverity(prompts.messages.constFirst())},
        {QStringLiteral("warning_message"),
         prompts.messages.constFirst().message},
        {QStringLiteral("draft_name"), name->text()},
        {QStringLiteral("editor_dirty"), page.hasUnsavedChanges()},
        {QStringLiteral("selected_class_id"), selectedClassId(page)},
        {QStringLiteral("stored_name"), unchanged->name},
        {QStringLiteral("update_calls"), updatePort.callCount}
    });
}

void TestingClassesPageCancelFailureParityTests::
f146CreateFailureRetainsDraftAndPendingSlot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        temporaryDatabasePath(directory, QStringLiteral("f146-create"))
        ));
    auto* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);

    FailingTestingClassCreatePort createPort;
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &createPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(
        -1,
        QStringLiteral("Wednesday"),
        QStringLiteral("18:00")
        );
    page.refresh();

    QLineEdit* const name = classNameEdit(page);
    QVERIFY(name);
    name->setText(QStringLiteral("Draft After Create Failure"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());
    QCOMPARE(createPort.callCount, 1);
    QCOMPARE(createPort.requests.size(), 1);
    QVERIFY(createPort.requests.constFirst().assignmentDay.has_value());
    QVERIFY(createPort.requests.constFirst().assignmentStartTime.has_value());
    QCOMPARE(*createPort.requests.constFirst().assignmentDay,
             std::u16string(u"Wednesday"));
    QCOMPARE(*createPort.requests.constFirst().assignmentStartTime,
             std::u16string(u"18:00"));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Save Testing Class"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class create failure"));
    QCOMPARE(name->text(), QStringLiteral("Draft After Create Failure"));
    QVERIFY(page.hasUnsavedChanges());

    const auto classes = repository->loadTestingClasses();
    QVERIFY(classes);
    QCOMPARE(classes->size(), 0);

    prompts.messages.clear();
    QVERIFY(!page.saveChanges());
    QCOMPARE(createPort.callCount, 2);
    QVERIFY(createPort.requests.at(1).assignmentDay.has_value());
    QVERIFY(createPort.requests.at(1).assignmentStartTime.has_value());
    QCOMPARE(*createPort.requests.at(1).assignmentDay,
             std::u16string(u"Wednesday"));
    QCOMPARE(*createPort.requests.at(1).assignmentStartTime,
             std::u16string(u"18:00"));
    QCOMPARE(name->text(), QStringLiteral("Draft After Create Failure"));
    QVERIFY(page.hasUnsavedChanges());

    logTranscript({
        {QStringLiteral("scenario"), QStringLiteral("F146-create-failure")},
        {QStringLiteral("warning_title"), QStringLiteral("Save Testing Class")},
        {QStringLiteral("warning_severity"), QStringLiteral("warning")},
        {QStringLiteral("warning_message"),
         QStringLiteral("injected testing class create failure")},
        {QStringLiteral("draft_name"), name->text()},
        {QStringLiteral("editor_dirty"), page.hasUnsavedChanges()},
        {QStringLiteral("class_count_after_failure"), classes->size()},
        {QStringLiteral("retry_count"), createPort.callCount - 1},
        {QStringLiteral("pending_day_retained"),
         createPort.requests.at(1).assignmentDay ==
             std::optional<std::u16string>(u"Wednesday")},
        {QStringLiteral("pending_start_retained"),
         createPort.requests.at(1).assignmentStartTime ==
             std::optional<std::u16string>(u"18:00")}
    });
}

void TestingClassesPageCancelFailureParityTests::
f147DeleteCancelDoesNotMutate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        temporaryDatabasePath(directory, QStringLiteral("f147-delete-cancel"))
        ));
    auto* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto classId = repository->createTestingClass(
        storedClass(QStringLiteral("Delete Target"))
        );
    QVERIFY(classId);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    FailingTestingClassDeletePort deletePort;
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &deletePort
        );
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(*classId);
    page.refresh();

    QLineEdit* const name = classNameEdit(page);
    QPushButton* const remove = deleteButton(page);
    QVERIFY(name);
    QVERIFY(remove);
    name->setText(QStringLiteral("Uncommitted Draft"));
    QVERIFY(page.hasUnsavedChanges());
    remove->click();
    QCoreApplication::processEvents();

    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.confirmations.constFirst().title,
             QStringLiteral("Delete Testing Class?"));
    QCOMPARE(deletePort.callCount, 0);
    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(name->text(), QStringLiteral("Uncommitted Draft"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selectedClassId(page), *classId);
    const auto unchanged = repository->loadTestingClass(*classId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->name, QStringLiteral("Delete Target"));

    logTranscript({
        {QStringLiteral("scenario"), QStringLiteral("F147-delete-cancel")},
        {QStringLiteral("confirmation_count"), prompts.confirmations.size()},
        {QStringLiteral("warning_count"), prompts.messages.size()},
        {QStringLiteral("delete_calls"), deletePort.callCount},
        {QStringLiteral("draft_name"), name->text()},
        {QStringLiteral("editor_dirty"), page.hasUnsavedChanges()},
        {QStringLiteral("selected_class_id"), selectedClassId(page)},
        {QStringLiteral("stored_name"), unchanged->name}
    });
}

void TestingClassesPageCancelFailureParityTests::
f147DeleteFailureRetainsDraftAndShowsWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        temporaryDatabasePath(directory, QStringLiteral("f147-delete-failure"))
        ));
    auto* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto classId = repository->createTestingClass(
        storedClass(QStringLiteral("Delete Target"))
        );
    QVERIFY(classId);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    FailingTestingClassDeletePort deletePort;
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &deletePort
        );
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(*classId);
    page.refresh();

    QLineEdit* const name = classNameEdit(page);
    QPushButton* const remove = deleteButton(page);
    QVERIFY(name);
    QVERIFY(remove);
    name->setText(QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    remove->click();
    QCoreApplication::processEvents();

    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(deletePort.callCount, 1);
    QVERIFY(deletePort.lastRequest.has_value());
    QCOMPARE(deletePort.lastRequest->classId.value(), std::to_string(*classId));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Delete Testing Class"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class delete failure"));
    QCOMPARE(name->text(), QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selectedClassId(page), *classId);
    const auto unchanged = repository->loadTestingClass(*classId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->name, QStringLiteral("Delete Target"));

    logTranscript({
        {QStringLiteral("scenario"), QStringLiteral("F147-delete-failure")},
        {QStringLiteral("warning_count"), prompts.messages.size()},
        {QStringLiteral("warning_title"), prompts.messages.constFirst().title},
        {QStringLiteral("warning_severity"),
         warningSeverity(prompts.messages.constFirst())},
        {QStringLiteral("warning_message"),
         prompts.messages.constFirst().message},
        {QStringLiteral("delete_calls"), deletePort.callCount},
        {QStringLiteral("draft_name"), name->text()},
        {QStringLiteral("editor_dirty"), page.hasUnsavedChanges()},
        {QStringLiteral("selected_class_id"), selectedClassId(page)},
        {QStringLiteral("stored_name"), unchanged->name}
    });
}

QTEST_MAIN(TestingClassesPageCancelFailureParityTests)

#include "testing_classes_page_cancel_failure_parity_tests.moc"
