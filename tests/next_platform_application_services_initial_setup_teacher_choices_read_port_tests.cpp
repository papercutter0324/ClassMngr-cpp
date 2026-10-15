#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/platform/application_services_initial_setup_teacher_choices_read_port.h"
#include "teacher_choices_cursor_error_fixture.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("initial-setup-teacher-choices-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacher(
    const QString& teacherKr,
    const QString& teacherEn,
    const QString& preferredRomanization,
    const QString& preferredName
    )
{
    Teacher value;
    value.teacherKr = teacherKr;
    value.teacherEn = teacherEn;
    value.preferredRomanization = preferredRomanization;
    value.preferredName = preferredName;
    return value;
}

Domain::TeacherId teacherId(const int value)
{
    const auto parsed = Domain::TeacherId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return *parsed;
}

}

class NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void mapsAllTeacherFieldsInRepositoryOrderFromTheActiveSession();
    void preservesSuccessfulEmptyRepositoryResult();
    void reportsUnavailableSessionAsRecoverableNotFound();
    void reportsTeacherRepositoryReadFailure();
    void preservesWideSqliteIntegerTeacherId();
    void rejectsPartialTeacherChoicesAfterCursorError();
};

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
preservesWideSqliteIntegerTeacherId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery seed(services.databaseSession()->database());
    QVERIFY(seed.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en) "
        "VALUES (2147483648, 'Wide Teacher', 'Wide Teacher')")));
    QVERIFY(seed.exec(QStringLiteral("SELECT id, typeof(id) FROM teachers")));
    QVERIFY(seed.next());
    QCOMPARE(seed.value(0).toLongLong(), qint64(2147483648LL));
    QCOMPARE(seed.value(1).toString(), QStringLiteral("integer"));

    const auto records = services.databaseSession()->teacherRepository()
        ->loadInitialSetupTeacherChoiceRecords();
    QVERIFY(records);
    QCOMPARE(records->size(), 1);
    QCOMPARE(records->front().teacherId, qint64(2147483648LL));
    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(&services);
    const Application::InitialSetupTeacherChoicesReadQuery query(port);
    const auto result = query.execute();
    QVERIFY(result);
    QCOMPARE(result.value().teachers.size(), std::size_t(1));
    QCOMPARE(result.value().teachers.front().teacherId.value(), std::string("2147483648"));
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
rejectsPartialTeacherChoicesAfterCursorError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QSqlDatabase database = session->database();

    const QString setupError =
        TeacherChoicesCursorErrorFixture::install(database);
    QVERIFY2(setupError.isEmpty(), qPrintable(setupError));

    const QString selectSql = QStringLiteral(R"(
        SELECT
            id,
            teacher_kr,
            teacher_en,
            preferred_romanization,
            preferred_name
        FROM teachers
        ORDER BY teacher_en
    )");
    const auto plan = TeacherChoicesCursorErrorFixture::explain(
        database,
        selectSql
        );
    QVERIFY2(plan.executed, qPrintable(plan.error));
    QVERIFY2(
        plan.details.join(QLatin1Char('\n')).contains(
            QStringLiteral("teacher_choices_cursor_teacher_en_idx")
            ),
        qPrintable(plan.details.join(QLatin1Char('\n')))
        );

    const auto probe = TeacherChoicesCursorErrorFixture::probe(
        database,
        selectSql
        );
    QVERIFY2(probe.executed, qPrintable(probe.terminalError.text()));
    QVERIFY(probe.firstRowFound);
    QCOMPARE(probe.firstTeacherId, qint64(1));
    QCOMPARE(probe.firstTeacherKr, QStringLiteral("A Alpha"));
    QVERIFY(!probe.secondRowFound);
    QVERIFY(probe.terminalError.isValid());
    QVERIFY(probe.terminalError.text().contains(
        QStringLiteral("malformed"), Qt::CaseInsensitive
        ));

    TeacherRepository* const repository = session->teacherRepository();
    QVERIFY(repository);
    const auto records = repository->loadInitialSetupTeacherChoiceRecords();
    QVERIFY(!records);
    QVERIFY(records.error().contains(QStringLiteral("malformed"), Qt::CaseInsensitive));

    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );
    const auto result = port.readInitialSetupTeacherChoices();
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(result.error().message.find("malformed") != std::string::npos);
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
mapsAllTeacherFieldsInRepositoryOrderFromTheActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TeacherRepository* const repository =
        services.databaseSession()->teacherRepository();
    QVERIFY(repository);

    const auto zuluId = repository->createTeacher(teacher(
        QStringLiteral("Korean Zulu"),
        QStringLiteral("Zulu"),
        QStringLiteral("Roman Zulu"),
        QStringLiteral("Preferred Zulu")
        ));
    QVERIFY(zuluId);
    const auto alphaId = repository->createTeacher(teacher(
        QStringLiteral("  \uAE40\uC120\uC0DD  "),
        QStringLiteral("  Alpha  "),
        QStringLiteral("  Roman Alpha  "),
        QStringLiteral("  Preferred Alpha  ")
        ));
    QVERIFY(alphaId);

    const auto repositoryRecords =
        repository->loadInitialSetupTeacherChoiceRecords();
    QVERIFY(repositoryRecords);
    QCOMPARE(repositoryRecords->size(), 2);
    QCOMPARE(repositoryRecords->at(0).teacherId, alphaId.value());
    QCOMPARE(repositoryRecords->at(0).teacherKr,
             QStringLiteral("  \uAE40\uC120\uC0DD  "));
    QCOMPARE(repositoryRecords->at(0).teacherEn, QStringLiteral("  Alpha  "));
    QCOMPARE(repositoryRecords->at(0).preferredRomanization,
             QStringLiteral("  Roman Alpha  "));
    QCOMPARE(repositoryRecords->at(0).preferredName,
             QStringLiteral("  Preferred Alpha  "));
    QCOMPARE(repositoryRecords->at(1).teacherId, zuluId.value());

    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );
    const Application::InitialSetupTeacherChoicesReadQuery query(port);
    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(result.value().teachers.size(), std::size_t(2));
    const auto& alpha = result.value().teachers[0];
    QCOMPARE(alpha.teacherId, teacherId(alphaId.value()));
    QCOMPARE(alpha.teacherKr, std::u16string(u"  \uAE40\uC120\uC0DD  "));
    QCOMPARE(alpha.teacherEn, std::u16string(u"  Alpha  "));
    QCOMPARE(alpha.preferredRomanization, std::u16string(u"  Roman Alpha  "));
    QCOMPARE(alpha.preferredName, std::u16string(u"  Preferred Alpha  "));

    const auto& zulu = result.value().teachers[1];
    QCOMPARE(zulu.teacherId, teacherId(zuluId.value()));
    QCOMPARE(zulu.teacherKr, std::u16string(u"Korean Zulu"));
    QCOMPARE(zulu.teacherEn, std::u16string(u"Zulu"));
    QCOMPARE(zulu.preferredRomanization, std::u16string(u"Roman Zulu"));
    QCOMPARE(zulu.preferredName, std::u16string(u"Preferred Zulu"));
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
preservesSuccessfulEmptyRepositoryResult()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );

    const auto result = port.readInitialSetupTeacherChoices();

    QVERIFY(result);
    QVERIFY(result.value().teachers.empty());
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
reportsUnavailableSessionAsRecoverableNotFound()
{
    ApplicationServices services;
    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );

    const auto result = port.readInitialSetupTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests::
reportsTeacherRepositoryReadFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));

    Platform::ApplicationServicesInitialSetupTeacherChoicesReadPort port(
        &services
        );
    const auto result = port.readInitialSetupTeacherChoices();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(result.error().message.find("Loading teachers")
            != std::string::npos);
    QVERIFY(!result.error().recoverable);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests
    )

#include "next_platform_application_services_initial_setup_teacher_choices_read_port_tests.moc"
