#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/platform/application_services_teacher_display_name_batch_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("teacher-display-name-batch-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacherFixture(const QString& displayName)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김선생");
    teacher.teacherEn = QStringLiteral("English Name");
    teacher.preferredRomanization = QStringLiteral("Seon Kim");
    teacher.preferredName = displayName;
    return teacher;
}

Domain::TeacherId teacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

int createTeacher(ApplicationServices& services, const QString& displayName)
{
    const auto result = services.databaseSession()
        ->teacherRepository()->createTeacher(teacherFixture(displayName));
    return result.value_or(-1);
}

}

class NextPlatformApplicationServicesTeacherDisplayNameBatchReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void reportsUnavailableSession();
    void mapsPartialRowsByIdentityAndUsesOneStatement();
    void reportsRepositoryFailure();
};

void NextPlatformApplicationServicesTeacherDisplayNameBatchReadPortTests::
reportsUnavailableSession()
{
    Platform::ApplicationServicesTeacherDisplayNameBatchReadPort nullPort(
        nullptr
        );
    const auto nullResult = nullPort.readTeacherDisplayNames({teacherId(1)});
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(nullResult.error().recoverable);

    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());
    Platform::ApplicationServicesTeacherDisplayNameBatchReadPort port(
        &services
        );
    const auto result = port.readTeacherDisplayNames({teacherId(1)});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesTeacherDisplayNameBatchReadPortTests::
mapsPartialRowsByIdentityAndUsesOneStatement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int firstId = createTeacher(services, QStringLiteral("First"));
    const int secondId = createTeacher(services, QStringLiteral("Second"));
    QVERIFY(firstId > 0);
    QVERIFY(secondId > firstId);

    TeacherRepository* const repository =
        services.databaseSession()->teacherRepository();
    QVERIFY(repository);
    const TeacherDisplayNameBatchReadMetrics before =
        repository->teacherDisplayNameBatchReadMetrics();

    Platform::ApplicationServicesTeacherDisplayNameBatchReadPort port(
        &services
        );
    const auto result = port.readTeacherDisplayNames({
        teacherId(secondId), teacherId(999999), teacherId(firstId)
    });

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].teacherId.value(), std::to_string(secondId));
    QVERIFY(result.value()[0].fields.teacherKr
        == teacherFixture(QStringLiteral("Second")).teacherKr.toStdU16String());
    QVERIFY(result.value()[0].fields.teacherEn
        == teacherFixture(QStringLiteral("Second")).teacherEn.toStdU16String());
    QVERIFY(result.value()[0].fields.preferredRomanization
        == teacherFixture(QStringLiteral("Second"))
            .preferredRomanization.toStdU16String());
    QVERIFY(result.value()[0].fields.preferredName
        == std::u16string(u"Second"));
    QCOMPARE(result.value()[1].teacherId.value(), std::to_string(firstId));
    QVERIFY(result.value()[1].fields.teacherKr
        == teacherFixture(QStringLiteral("First")).teacherKr.toStdU16String());
    QVERIFY(result.value()[1].fields.teacherEn
        == teacherFixture(QStringLiteral("First")).teacherEn.toStdU16String());
    QVERIFY(result.value()[1].fields.preferredRomanization
        == teacherFixture(QStringLiteral("First"))
            .preferredRomanization.toStdU16String());
    QVERIFY(result.value()[1].fields.preferredName
        == std::u16string(u"First"));

    const TeacherDisplayNameBatchReadMetrics after =
        repository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.requestedTeacherCount - before.requestedTeacherCount, 3);
    QCOMPARE(after.statementCount - before.statementCount, 1);
}

void NextPlatformApplicationServicesTeacherDisplayNameBatchReadPortTests::
reportsRepositoryFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createTeacher(services, QStringLiteral("Teacher"));
    QVERIFY(id > 0);

    Platform::ApplicationServicesTeacherDisplayNameBatchReadPort port(
        &services
        );
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
        qPrintable(dropTeachers.lastError().text()));

    const auto result = port.readTeacherDisplayNames({teacherId(id)});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

QTEST_MAIN(NextPlatformApplicationServicesTeacherDisplayNameBatchReadPortTests)

#include "next_platform_application_services_teacher_display_name_batch_read_port_tests.moc"
