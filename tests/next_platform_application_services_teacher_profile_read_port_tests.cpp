#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/platform/application_services_teacher_profile_read_port.h"

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
        QStringLiteral("teacher-profile-read-port-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacherFixture()
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김선생");
    teacher.teacherEn = QStringLiteral("Alex Teacher");
    teacher.preferredRomanization = QStringLiteral("Seon Kim");
    teacher.preferredName = QStringLiteral("Alex");
    teacher.roomNumber = QStringLiteral("Room 5");
    teacher.birthday = QStringLiteral("03-14");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Class Wi-Fi");
    teacher.wifiPassword = QStringLiteral("wifi secret");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("alex.zoom");
    teacher.zoomPassword = QStringLiteral("zoom secret");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.notes = QStringLiteral("Teacher profile notes");
    return teacher;
}

Domain::TeacherId teacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

int createTeacher(ApplicationServices& services)
{
    const auto result =
        services.databaseSession()->teacherRepository()->createTeacher(
            teacherFixture()
            );
    return result.value_or(-1);
}

}

class NextPlatformApplicationServicesTeacherProfileReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsCompleteTeacherProfileFromActiveSession();
    void reportsUnavailableSessionAndRepositoryErrorsWithoutFallback();
};

void NextPlatformApplicationServicesTeacherProfileReadPortTests::
readsCompleteTeacherProfileFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createTeacher(services);
    QVERIFY(id > 0);

    Platform::ApplicationServicesTeacherProfileReadPort port(&services);
    const Application::TeacherProfileReadResult result =
        port.readTeacherProfile(teacherId(id));

    QVERIFY(result);
    const Teacher expected = teacherFixture();
    QCOMPARE(result.value().teacherId.value(), std::to_string(id));
    const Domain::TeacherProfileFields& fields = result.value().fields;
    QVERIFY(fields.teacherKr == expected.teacherKr.toStdU16String());
    QVERIFY(fields.teacherEn == expected.teacherEn.toStdU16String());
    QVERIFY(fields.preferredRomanization
        == expected.preferredRomanization.toStdU16String());
    QVERIFY(fields.preferredName == expected.preferredName.toStdU16String());
    QVERIFY(fields.roomNumber == expected.roomNumber.toStdU16String());
    QVERIFY(fields.birthday == expected.birthday.toStdU16String());
    QVERIFY(fields.phoneNumber == expected.phoneNumber.toStdU16String());
    QVERIFY(fields.wifiName == expected.wifiName.toStdU16String());
    QVERIFY(fields.wifiPassword == expected.wifiPassword.toStdU16String());
    QVERIFY(fields.internetType == expected.internetType.toStdU16String());
    QVERIFY(fields.zoomId == expected.zoomId.toStdU16String());
    QVERIFY(fields.zoomPassword == expected.zoomPassword.toStdU16String());
    QVERIFY(fields.projectionType == expected.projectionType.toStdU16String());
    QVERIFY(fields.notes == expected.notes.toStdU16String());
}

void NextPlatformApplicationServicesTeacherProfileReadPortTests::
reportsUnavailableSessionAndRepositoryErrorsWithoutFallback()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    Platform::ApplicationServicesTeacherProfileReadPort port(&services);
    auto unavailable = port.readTeacherProfile(teacherId(1));
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createTeacher(services);
    QVERIFY(id > 0);

    const auto missing = port.readTeacherProfile(teacherId(id + 1000));
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!missing.error().message.empty());
    QVERIFY(!missing.error().recoverable);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE teachers")),
        qPrintable(query.lastError().text()));
    const auto repositoryFailure = port.readTeacherProfile(teacherId(id));
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());

    services.closeDatabase();
    unavailable = port.readTeacherProfile(teacherId(id));
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(NextPlatformApplicationServicesTeacherProfileReadPortTests)

#include "next_platform_application_services_teacher_profile_read_port_tests.moc"
