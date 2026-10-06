#include "core/application_services.h"
#include "data/database/database_session.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QApplication>
#include <QAbstractButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <memory>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("initial-setup-teacher-create-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int teacherCount(DatabaseSession& session)
{
    QSqlQuery query(session.database());
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM teachers"))
        && query.next()
        ? query.value(0).toInt()
        : -1;
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }
    QSqlQuery query(session->database());
    return query.exec(statement);
}

class WizardFixture final
{
public:
    WizardFixture()
    {
        DialogServices::setUserPromptServiceForTesting(&prompts);
    }

    ~WizardFixture()
    {
        wizard.reset();
        services.closeDatabase();
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    bool open()
    {
        if (!directory.isValid()
            || !services.openDatabase(databasePath(directory)))
        {
            return false;
        }

        wizard = std::make_unique<InitialSetupWizard>(&services);
        wizard->setStartId(InitialSetupWizard::TeacherEntryPage);
        wizard->show();
        QApplication::processEvents();
        return currentId() == InitialSetupWizard::TeacherEntryPage;
    }

    QLineEdit* edit(const QString& objectName) const
    {
        return wizard
            ? wizard->findChild<QLineEdit*>(objectName)
            : nullptr;
    }

    bool fillValidTeacher()
    {
        QLineEdit* const english = edit(QStringLiteral("setupTeacherEnglishName"));
        QLineEdit* const spelling = edit(
            QStringLiteral("setupTeacherPreferredSpelling"));
        if (!english || !spelling)
        {
            return false;
        }
        english->setText(QStringLiteral("Alex Smith"));
        spelling->setText(QStringLiteral("Alex Smith"));
        return true;
    }

    bool advance()
    {
        if (!wizard)
        {
            return false;
        }
        QAbstractButton* const next = wizard->button(QWizard::NextButton);
        if (!next)
        {
            return false;
        }
        next->click();
        QApplication::processEvents();
        return true;
    }

    bool addAnother()
    {
        QPushButton* const button = wizard
            ? wizard->findChild<QPushButton*>(
                  QStringLiteral("setupAddAnotherTeacherButton"))
            : nullptr;
        if (!button)
        {
            return false;
        }
        button->click();
        QApplication::processEvents();
        return true;
    }

    int rows() const
    {
        DatabaseSession* const session = services.databaseSession();
        return session && session->isOpen() ? teacherCount(*session) : -1;
    }

    int currentId() const
    {
        return wizard ? wizard->currentId() : -1;
    }

    ApplicationServices services;
    QTemporaryDir directory;
    FakeUserPromptService prompts;
    std::unique_ptr<InitialSetupWizard> wizard;
};

void emitTranscript(const QJsonObject& transcript)
{
    qInfo().noquote() << "F366_TRANSCRIPT"
                      << QJsonDocument(transcript).toJson(
                             QJsonDocument::Compact
                             );
}

}

class InitialSetupWizardTeacherCreateParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void identityValidationKeepsFieldsAndPromptOrder();
    void writeFailureRetainsFieldsAndRetryAdvancesWithoutSuccessPrompt();
    void addAnotherClearsFieldsAndDuplicateSubmissionsAreDistinct();
    void successfulNextAdvancesWithoutSuccessPrompt();
};

void InitialSetupWizardTeacherCreateParityTests::
identityValidationKeepsFieldsAndPromptOrder()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());
    QLineEdit* const english =
        fixture.edit(QStringLiteral("setupTeacherEnglishName"));
    QVERIFY(english);
    english->setText(QStringLiteral("Alex Smith"));

    QVERIFY(fixture.advance());

    QCOMPARE(fixture.currentId(), InitialSetupWizard::TeacherEntryPage);
    QCOMPARE(fixture.rows(), 0);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const PromptRequest& prompt = fixture.prompts.messages.constFirst();
    QCOMPARE(prompt.severity, PromptSeverity::Information);
    QCOMPARE(prompt.title, QStringLiteral("Teacher Information"));
    QCOMPARE(
        prompt.message,
        QStringLiteral(
            "Complete at least 1 of the following fields before continuing:\n\n"
            "- Korean Name\n- Preferred Spelling")
        );
    QCOMPARE(english->text(), QStringLiteral("Alex Smith"));

    emitTranscript({
        {QStringLiteral("rows_after_incomplete_identity"), 0},
        {QStringLiteral("information_prompt_order_and_text_match"), true},
        {QStringLiteral("entered_field_retained"), true},
        {QStringLiteral("stayed_on_teacher_page"), true}
    });
}

void InitialSetupWizardTeacherCreateParityTests::
writeFailureRetainsFieldsAndRetryAdvancesWithoutSuccessPrompt()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());
    QVERIFY(fixture.fillValidTeacher());
    QVERIFY(executeSql(
        fixture.services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_setup_teacher_create
            BEFORE INSERT ON teachers
            BEGIN
                SELECT RAISE(ABORT, 'forced teacher create failure');
            END
        )")
        ));

    QVERIFY(fixture.advance());
    QCOMPARE(fixture.currentId(), InitialSetupWizard::TeacherEntryPage);
    QCOMPARE(fixture.rows(), 0);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const PromptRequest failurePrompt = fixture.prompts.messages.constFirst();
    QCOMPARE(failurePrompt.severity, PromptSeverity::Warning);
    QCOMPARE(failurePrompt.title, QStringLiteral("Teacher Information"));
    QCOMPARE(failurePrompt.message, QStringLiteral("The teacher could not be saved."));
    QVERIFY(!failurePrompt.details.trimmed().isEmpty());
    QCOMPARE(
        fixture.edit(QStringLiteral("setupTeacherEnglishName"))->text(),
        QStringLiteral("Alex Smith")
        );
    QCOMPARE(
        fixture.edit(QStringLiteral("setupTeacherPreferredSpelling"))->text(),
        QStringLiteral("Alex Smith")
        );

    QVERIFY(executeSql(
        fixture.services,
        QStringLiteral("DROP TRIGGER fail_setup_teacher_create")
        ));
    QVERIFY(fixture.advance());
    QCOMPARE(fixture.currentId(), InitialSetupWizard::ClassDetailsPage);
    QCOMPARE(fixture.rows(), 1);
    QCOMPARE(fixture.prompts.messages.size(), 1);

    emitTranscript({
        {QStringLiteral("rows_after_failed_write"), 0},
        {QStringLiteral("write_failure_warning_matches"), true},
        {QStringLiteral("fields_retained_for_retry"), true},
        {QStringLiteral("rows_after_retry"), 1},
        {QStringLiteral("retry_navigated_to_class_details"), true},
        {QStringLiteral("next_success_prompt_count"), 0}
    });
}

void InitialSetupWizardTeacherCreateParityTests::
addAnotherClearsFieldsAndDuplicateSubmissionsAreDistinct()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());
    QVERIFY(fixture.fillValidTeacher());
    QVERIFY(fixture.addAnother());

    QCOMPARE(fixture.rows(), 1);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    QCOMPARE(fixture.prompts.messages.constFirst().severity,
        PromptSeverity::Information);
    QCOMPARE(fixture.prompts.messages.constFirst().title,
        QStringLiteral("Teacher Information"));
    QCOMPARE(fixture.prompts.messages.constFirst().message,
        QStringLiteral(
            "Teacher saved. You can enter another teacher or continue to the next step."));
    QCOMPARE(
        fixture.edit(QStringLiteral("setupTeacherEnglishName"))->text(),
        QString()
        );
    QCOMPARE(
        fixture.edit(QStringLiteral("setupTeacherPreferredSpelling"))->text(),
        QString()
        );

    QVERIFY(fixture.fillValidTeacher());
    QVERIFY(fixture.addAnother());
    QCOMPARE(fixture.rows(), 2);
    QCOMPARE(fixture.prompts.messages.size(), 2);

    QVERIFY(fixture.advance());
    QCOMPARE(fixture.currentId(), InitialSetupWizard::ClassDetailsPage);
    QCOMPARE(fixture.rows(), 2);
    QCOMPARE(fixture.prompts.messages.size(), 2);

    emitTranscript({
        {QStringLiteral("rows_after_add_another"), 1},
        {QStringLiteral("add_another_information_prompt_matches"), true},
        {QStringLiteral("fields_cleared_after_add_another"), true},
        {QStringLiteral("identical_submissions_create_distinct_rows"), true},
        {QStringLiteral("blank_next_with_existing_teacher_navigates"), true},
        {QStringLiteral("blank_next_prompt_count"), 0}
    });
}

void InitialSetupWizardTeacherCreateParityTests::
successfulNextAdvancesWithoutSuccessPrompt()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());
    QVERIFY(fixture.fillValidTeacher());

    QVERIFY(fixture.advance());

    QCOMPARE(fixture.currentId(), InitialSetupWizard::ClassDetailsPage);
    QCOMPARE(fixture.rows(), 1);
    QCOMPARE(fixture.prompts.messages.size(), 0);

    emitTranscript({
        {QStringLiteral("teacher_rows"), 1},
        {QStringLiteral("navigated_to_class_details"), true},
        {QStringLiteral("success_prompt_count"), 0}
    });
}

QTEST_MAIN(InitialSetupWizardTeacherCreateParityTests)

#include "initial_setup_wizard_teacher_create_parity_tests.moc"
