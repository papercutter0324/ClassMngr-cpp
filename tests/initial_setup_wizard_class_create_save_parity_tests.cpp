#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"

#include <QApplication>
#include <QAbstractButton>
#include <QJsonDocument>
#include <QJsonObject>
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
        QStringLiteral("initial-setup-create-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int scalarCount(DatabaseSession& session, const QString& table)
{
    QSqlQuery query(session.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

int scalarValue(
    DatabaseSession& session,
    const QString& statement,
    const int classId
    )
{
    QSqlQuery query(session.database());
    query.prepare(statement);
    query.addBindValue(classId);
    return query.exec() && query.next() ? query.value(0).toInt() : -1;
}

QString scalarText(
    DatabaseSession& session,
    const QString& statement,
    const int classId
    )
{
    QSqlQuery query(session.database());
    query.prepare(statement);
    query.addBindValue(classId);
    return query.exec() && query.next() ? query.value(0).toString() : QString{};
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

        Teacher teacher;
        teacher.teacherEn = QStringLiteral("F365 Setup Teacher");
        teacher.teacherKr = QStringLiteral("Setup Teacher");
        teacher.preferredName = teacher.teacherEn;
        TeacherRepository* const teachers =
            services.databaseSession()->teacherRepository();
        if (!teachers)
        {
            return false;
        }
        const auto savedTeacher = teachers->createTeacher(teacher);
        if (!savedTeacher)
        {
            return false;
        }

        wizard = std::make_unique<InitialSetupWizard>(&services);
        ClassInfo draft;
        draft.teacherId = *savedTeacher;
        draft.classGrade = QStringLiteral("E4");
        draft.classLevel = QStringLiteral("Theseus");
        draft.readingBook = QStringLiteral("Reading Explorer 1");
        draft.essayBook = QStringLiteral("4A");
        draft.classColor = QStringLiteral("#123456");
        draft.fontColor = QStringLiteral("#FFFFFF");
        wizard->setClassDraft(draft);
        wizard->setStartId(InitialSetupWizard::ClassTimesPage);
        wizard->show();
        QApplication::processEvents();

        ClassScheduleSection* const schedules = wizard->findChild<
            ClassScheduleSection*>(QStringLiteral("setupClassSchedules"));
        if (!schedules || schedules->regularRows().isEmpty())
        {
            return false;
        }
        ClassTimeRow* const row = schedules->regularRows().constFirst();
        row->setDay(QStringLiteral("Monday"));
        row->setStartTime(QStringLiteral("9:00 AM"));
        row->setEndTime(QStringLiteral("9:55 AM"));
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

    int count(const QString& table) const
    {
        DatabaseSession* const session = services.databaseSession();
        return session ? scalarCount(*session, table) : -1;
    }

    int value(const QString& statement, const int classId) const
    {
        DatabaseSession* const session = services.databaseSession();
        return session ? scalarValue(*session, statement, classId) : -1;
    }

    QString text(const QString& statement, const int classId) const
    {
        DatabaseSession* const session = services.databaseSession();
        return session ? scalarText(*session, statement, classId) : QString{};
    }

    ApplicationServices services;
    QTemporaryDir directory;
    FakeUserPromptService prompts;
    std::unique_ptr<InitialSetupWizard> wizard;
};

void emitTranscript(const QJsonObject& transcript)
{
    qInfo().noquote() << "F365_TRANSCRIPT"
                      << QJsonDocument(transcript).toJson(
                             QJsonDocument::Compact
                             );
}

}

class InitialSetupWizardClassCreateSaveParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void createFailureLeavesIdUnsetAndCanRetry();
    void detailsFailureRetainsBlankClassAndRetryReusesId();
    void successfulSaveAdvancesWithDetailsAndTeacher();
};

void InitialSetupWizardClassCreateSaveParityTests::
createFailureLeavesIdUnsetAndCanRetry()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());
    QVERIFY(executeSql(
        fixture.services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_setup_class_create
            BEFORE INSERT ON classes
            BEGIN
                SELECT RAISE(ABORT, 'forced class create failure');
            END
        )")
        ));

    QVERIFY(fixture.advance());
    QCOMPARE(fixture.wizard->currentId(), InitialSetupWizard::ClassTimesPage);
    QCOMPARE(fixture.wizard->createdClassId(), -1);
    QCOMPARE(fixture.count(QStringLiteral("classes")), 0);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const bool idUnsetAfterFailure =
        fixture.wizard->createdClassId() <= 0;
    const bool warningAfterFailure = fixture.prompts.messages.size() == 1;
    QCOMPARE(
        fixture.prompts.messages.constFirst().title,
        QStringLiteral("Create Class")
        );
    QCOMPARE(
        fixture.prompts.messages.constFirst().message,
        QStringLiteral("The class could not be created.")
        );
    const bool warningDetailsPresent =
        !fixture.prompts.messages.constFirst().details.trimmed().isEmpty();

    QVERIFY(executeSql(
        fixture.services,
        QStringLiteral("DROP TRIGGER fail_setup_class_create")
        ));
    QVERIFY(fixture.advance());
    QCOMPARE(fixture.wizard->currentId(), InitialSetupWizard::CompletionPage);
    QVERIFY(fixture.wizard->createdClassId() > 0);
    QCOMPARE(fixture.count(QStringLiteral("classes")), 1);

    emitTranscript({
        {QStringLiteral("id_unset_after_failure"),
         idUnsetAfterFailure},
        {QStringLiteral("class_rows_after_failure"), 0},
        {QStringLiteral("warning_after_failure"), warningAfterFailure},
        {QStringLiteral("warning_message_matches"), true},
        {QStringLiteral("warning_details_present"), warningDetailsPresent},
        {QStringLiteral("retry_created_one_class"),
         fixture.count(QStringLiteral("classes")) == 1},
        {QStringLiteral("retry_advanced"),
         fixture.wizard->currentId() == InitialSetupWizard::CompletionPage}
    });
}

void InitialSetupWizardClassCreateSaveParityTests::
detailsFailureRetainsBlankClassAndRetryReusesId()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());
    QVERIFY(executeSql(
        fixture.services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_setup_class_info_save
            BEFORE INSERT ON class_info
            BEGIN
                SELECT RAISE(ABORT, 'forced class details failure');
            END
        )")
        ));

    QVERIFY(fixture.advance());
    const int createdId = fixture.wizard->createdClassId();
    QVERIFY(createdId > 0);
    QCOMPARE(fixture.wizard->currentId(), InitialSetupWizard::ClassTimesPage);
    QCOMPARE(fixture.count(QStringLiteral("classes")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_info")), 0);
    QCOMPARE(fixture.count(QStringLiteral("class_times")), 0);
    QCOMPARE(fixture.count(QStringLiteral("class_intensive_times")), 0);
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT name FROM classes WHERE id = ?"),
            createdId
            ),
        QString()
        );
    QCOMPARE(fixture.prompts.messages.size(), 1);
    QCOMPARE(
        fixture.prompts.messages.constFirst().title,
        QStringLiteral("Create Class")
        );
    QCOMPARE(
        fixture.prompts.messages.constFirst().message,
        QStringLiteral("The class information could not be saved.")
        );
    QVERIFY(executeSql(
        fixture.services,
        QStringLiteral("DROP TRIGGER fail_setup_class_info_save")
        ));

    QVERIFY(fixture.advance());
    QCOMPARE(fixture.wizard->currentId(), InitialSetupWizard::CompletionPage);
    QCOMPARE(fixture.wizard->createdClassId(), createdId);
    QCOMPARE(fixture.count(QStringLiteral("classes")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_info")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_times")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_intensive_times")), 0);
    QCOMPARE(
        fixture.value(
            QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id = ?"),
            createdId
            ),
        fixture.wizard->classDraft().teacherId
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT name FROM classes WHERE id = ?"),
            createdId
            ),
        QString()
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT class_grade FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("E4")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT class_level FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("Theseus")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT reading_book FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("Reading Explorer 1")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT essay_book FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("4A")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT class_color FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("#123456")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT font_color FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("#FFFFFF")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT day FROM class_times WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("Monday")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT start_time FROM class_times WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("9:00 AM")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT end_time FROM class_times WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("9:55 AM")
        );

    emitTranscript({
        {QStringLiteral("blank_class_after_failure"),
         fixture.count(QStringLiteral("classes")) == 1},
        {QStringLiteral("class_info_after_failure"), 0},
        {QStringLiteral("regular_rows_after_failure"), 0},
        {QStringLiteral("intensive_rows_after_failure"), 0},
        {QStringLiteral("id_retained_on_failure"), createdId > 0},
        {QStringLiteral("warning_message_matches"), true},
        {QStringLiteral("retry_reused_id"),
         fixture.wizard->createdClassId() == createdId},
        {QStringLiteral("class_rows_after_retry"), 1},
        {QStringLiteral("class_info_after_retry"), 1},
        {QStringLiteral("regular_rows_after_retry"), 1},
        {QStringLiteral("teacher_saved"), true},
        {QStringLiteral("details_and_time_saved"), true},
        {QStringLiteral("retry_advanced"),
         fixture.wizard->currentId() == InitialSetupWizard::CompletionPage}
    });
}

void InitialSetupWizardClassCreateSaveParityTests::
successfulSaveAdvancesWithDetailsAndTeacher()
{
    WizardFixture fixture;
    QVERIFY(fixture.open());

    QVERIFY(fixture.advance());
    QCOMPARE(fixture.wizard->currentId(), InitialSetupWizard::CompletionPage);
    const int createdId = fixture.wizard->createdClassId();
    QVERIFY(createdId > 0);
    QCOMPARE(fixture.count(QStringLiteral("classes")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_info")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_times")), 1);
    QCOMPARE(fixture.count(QStringLiteral("class_intensive_times")), 0);
    QCOMPARE(
        fixture.value(
            QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id = ?"),
            createdId
            ),
        fixture.wizard->classDraft().teacherId
        );
    QCOMPARE(fixture.wizard->classDraft().classGrade, QStringLiteral("E4"));
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT class_level FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("Theseus")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT reading_book FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("Reading Explorer 1")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT essay_book FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("4A")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT class_color FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("#123456")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT font_color FROM class_info WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("#FFFFFF")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT start_time FROM class_times WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("9:00 AM")
        );
    QCOMPARE(
        fixture.text(
            QStringLiteral("SELECT end_time FROM class_times WHERE class_id = ?"),
            createdId
            ),
        QStringLiteral("9:55 AM")
        );

    emitTranscript({
        {QStringLiteral("class_rows"), 1},
        {QStringLiteral("class_info_rows"), 1},
        {QStringLiteral("regular_rows"), 1},
        {QStringLiteral("intensive_rows"), 0},
        {QStringLiteral("teacher_saved"), true},
        {QStringLiteral("details_and_time_saved"), true},
        {QStringLiteral("advanced_to_completion"), true}
    });
}

QTEST_MAIN(InitialSetupWizardClassCreateSaveParityTests)

#include "initial_setup_wizard_class_create_save_parity_tests.moc"
