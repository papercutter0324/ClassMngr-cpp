#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/application/my_classes_teacher_profile_batch_read_query.h"
#include "next/platform/application_services_my_classes_teacher_profile_batch_read_port.h"

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
        QStringLiteral("my-classes-teacher-profile-batch-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::TeacherId teacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

Teacher teacherFixture()
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uD55C\uAD6D\uC5B4 \uAD50\uC0AC");
    teacher.teacherEn = QStringLiteral("English Teacher");
    teacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    teacher.preferredName = QStringLiteral("Preferred \U0001F393");
    teacher.roomNumber = QStringLiteral("Room 12 \uAC15\uB0A8");
    teacher.birthday = QStringLiteral("03-14");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Class WiFi \U0001F4F6");
    teacher.wifiPassword = QStringLiteral("WiFi password \U0001F511");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("Zoom ID");
    teacher.zoomPassword = QStringLiteral("Zoom password \U0001F510");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.notes = QStringLiteral("Profile notes\nUTF-16 \U0001F9ED");
    return teacher;
}

int createTeacher(ApplicationServices& services, const Teacher& teacher)
{
    const auto result = services.databaseSession()
        ->teacherRepository()->createTeacher(teacher);
    return result.value_or(-1);
}

}

class NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsConsumedFieldsInRequestedOrderAndKeepsMissingTeacherFailure();
    void batchSqlFailurePreservesIndependentPerTeacherFailures();
    void emptyInputAvoidsRepositoryWork();
    void reportsUnavailableServicesPerTeacher();
};

void NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests::
readsConsumedFieldsInRequestedOrderAndKeepsMissingTeacherFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const Teacher expected = teacherFixture();
    const int existingId = createTeacher(services, expected);
    QVERIFY(existingId > 0);
    const int missingId = existingId + 1000;

    TeacherRepository* const repository = services.databaseSession()
        ->teacherRepository();
    QVERIFY(repository);
    const MyClassesTeacherProfileBatchReadMetrics before =
        repository->myClassesTeacherProfileBatchReadMetrics();
    const TeacherProfileBatchReadMetrics broadBefore =
        repository->teacherProfileBatchReadMetrics();

    Platform::ApplicationServicesMyClassesTeacherProfileBatchReadPort port(
        services
        );
    const Application::MyClassesTeacherProfileBatchReadQuery query(port);
    const std::vector<Domain::TeacherId> requested{
        teacherId(missingId), teacherId(existingId)
    };
    const auto loaded = query.execute(requested);

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), requested.size());
    QVERIFY(loaded.value()[0].teacherId == requested[0]);
    QVERIFY(!loaded.value()[0].profile);
    QCOMPARE(loaded.value()[0].profile.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(loaded.value()[0].profile.error().message.find(
                "no matching record") != std::string::npos);

    QVERIFY(loaded.value()[1].teacherId == requested[1]);
    QVERIFY(loaded.value()[1].profile);
    const Application::MyClassesTeacherProfileFields& fields =
        loaded.value()[1].profile.value();
    QVERIFY(fields.teacherKr == expected.teacherKr.toStdU16String());
    QVERIFY(fields.teacherEn == expected.teacherEn.toStdU16String());
    QVERIFY(fields.preferredRomanization
        == expected.preferredRomanization.toStdU16String());
    QVERIFY(fields.preferredName == expected.preferredName.toStdU16String());
    QVERIFY(fields.roomNumber == expected.roomNumber.toStdU16String());
    QVERIFY(fields.wifiName == expected.wifiName.toStdU16String());
    QVERIFY(fields.wifiPassword == expected.wifiPassword.toStdU16String());
    QVERIFY(fields.internetType == expected.internetType.toStdU16String());
    QVERIFY(fields.zoomId == expected.zoomId.toStdU16String());
    QVERIFY(fields.zoomPassword == expected.zoomPassword.toStdU16String());
    QVERIFY(fields.projectionType == expected.projectionType.toStdU16String());
    QVERIFY(fields.notes == expected.notes.toStdU16String());

    const MyClassesTeacherProfileBatchReadMetrics after =
        repository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedTeacherCount, before.requestedTeacherCount + 2);
    QCOMPARE(after.statementCount, before.statementCount + 1);
    const TeacherProfileBatchReadMetrics broadAfter =
        repository->teacherProfileBatchReadMetrics();
    QCOMPARE(broadAfter.callCount, broadBefore.callCount);
    QCOMPARE(broadAfter.statementCount, broadBefore.statementCount);
}

void NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests::
batchSqlFailurePreservesIndependentPerTeacherFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TeacherRepository* const repository = services.databaseSession()
        ->teacherRepository();
    QVERIFY(repository);
    const MyClassesTeacherProfileBatchReadMetrics before =
        repository->myClassesTeacherProfileBatchReadMetrics();

    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));

    Platform::ApplicationServicesMyClassesTeacherProfileBatchReadPort port(
        services
        );
    const Application::MyClassesTeacherProfileBatchReadQuery query(port);
    const std::vector<Domain::TeacherId> requested{
        teacherId(13), teacherId(41)
    };
    const auto loaded = query.execute(requested);

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), requested.size());
    for (std::size_t index = 0; index < requested.size(); ++index)
    {
        QVERIFY(loaded.value()[index].teacherId == requested[index]);
        QVERIFY(!loaded.value()[index].profile);
        QCOMPARE(loaded.value()[index].profile.error().code,
                 Domain::ErrorCode::Technical);
        QVERIFY(!loaded.value()[index].profile.error().message.empty());
    }

    const MyClassesTeacherProfileBatchReadMetrics after =
        repository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedTeacherCount, before.requestedTeacherCount + 2);
    QCOMPARE(after.statementCount, before.statementCount + 1);
}

void NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests::
emptyInputAvoidsRepositoryWork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TeacherRepository* const repository = services.databaseSession()
        ->teacherRepository();
    QVERIFY(repository);
    const MyClassesTeacherProfileBatchReadMetrics before =
        repository->myClassesTeacherProfileBatchReadMetrics();

    Platform::ApplicationServicesMyClassesTeacherProfileBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesTeacherProfiles({});

    QVERIFY(loaded);
    QVERIFY(loaded.value().empty());
    const MyClassesTeacherProfileBatchReadMetrics after =
        repository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount);
    QCOMPARE(after.requestedTeacherCount, before.requestedTeacherCount);
    QCOMPARE(after.statementCount, before.statementCount);
}

void NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests::
reportsUnavailableServicesPerTeacher()
{
    Platform::ApplicationServicesMyClassesTeacherProfileBatchReadPort port(
        nullptr
        );
    const auto loaded = port.readMyClassesTeacherProfiles({teacherId(3)});

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(1));
    QVERIFY(loaded.value()[0].teacherId == teacherId(3));
    QVERIFY(!loaded.value()[0].profile);
    QCOMPARE(loaded.value()[0].profile.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[0].profile.error().recoverable);
}

QTEST_MAIN(
    NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests
    )

#include "next_platform_application_services_my_classes_teacher_profile_batch_read_port_tests.moc"
