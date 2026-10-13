#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/platform/application_services_teacher_profile_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
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

Domain::TeacherId teacherId(const qint64 value)
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

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

}

class NextPlatformApplicationServicesTeacherProfileReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsCompleteTeacherProfileFromActiveSession();
    void readsCompleteTeacherProfileForWideDatabaseId();
    void rejectsNonCanonicalTeacherIds();
    void reportsUnavailableMissingAndRepositoryFailureWithoutFallback();
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
readsCompleteTeacherProfileForWideDatabaseId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    constexpr qint64 wideTeacherId = 2147483648LL;
    const Teacher expected = teacherFixture();
    QSqlQuery insert(services.databaseSession()->database());
    QVERIFY2(insert.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id,
            teacher_kr,
            teacher_en,
            preferred_romanization,
            preferred_name,
            room_number,
            birthday,
            phone_number,
            wifi_name,
            wifi_password,
            internet_type,
            zoom_id,
            zoom_password,
            projection_type,
            notes
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )")), qPrintable(insert.lastError().text()));
    insert.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    insert.addBindValue(expected.teacherKr);
    insert.addBindValue(expected.teacherEn);
    insert.addBindValue(expected.preferredRomanization);
    insert.addBindValue(expected.preferredName);
    insert.addBindValue(expected.roomNumber);
    insert.addBindValue(expected.birthday);
    insert.addBindValue(expected.phoneNumber);
    insert.addBindValue(expected.wifiName);
    insert.addBindValue(expected.wifiPassword);
    insert.addBindValue(expected.internetType);
    insert.addBindValue(expected.zoomId);
    insert.addBindValue(expected.zoomPassword);
    insert.addBindValue(expected.projectionType);
    insert.addBindValue(expected.notes);
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    QSqlQuery storedId(services.databaseSession()->database());
    QVERIFY(storedId.prepare(QStringLiteral(
        "SELECT typeof(id), id FROM teachers WHERE id=?")));
    storedId.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    QVERIFY2(storedId.exec(), qPrintable(storedId.lastError().text()));
    QVERIFY(storedId.next());
    QCOMPARE(storedId.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(storedId.value(1).toLongLong(), wideTeacherId);

    Platform::ApplicationServicesTeacherProfileReadPort port(&services);
    const Domain::TeacherId requestedId = teacherId(wideTeacherId);
    const Application::TeacherProfileReadResult result =
        port.readTeacherProfile(requestedId);

    QVERIFY(result);
    QVERIFY(result.value().teacherId == requestedId);
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
rejectsNonCanonicalTeacherIds()
{
    ApplicationServices services;
    Platform::ApplicationServicesTeacherProfileReadPort port(&services);

    for (const std::string& value : {
             std::string("0"),
             std::string("-1"),
             std::string("+7"),
             std::string("07"),
             std::string("2147483648x"),
             std::string("9223372036854775808")
         })
    {
        const auto invalidId = Domain::TeacherId::fromString(value);
        QVERIFY(invalidId);
        const auto result = port.readTeacherProfile(*invalidId);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("Teacher ID must be a canonical positive integer.")
            );
        QVERIFY(result.error().recoverable);
    }

    const auto maximumId = Domain::TeacherId::fromString(
        "9223372036854775807"
        );
    QVERIFY(maximumId);
    const auto maximumResult = port.readTeacherProfile(*maximumId);
    QVERIFY(!maximumResult);
    QCOMPARE(maximumResult.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(
        maximumResult.error().message,
        std::string(
            "No active Teacher Profile database session is available."
            )
        );
}

void NextPlatformApplicationServicesTeacherProfileReadPortTests::
reportsUnavailableMissingAndRepositoryFailureWithoutFallback()
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

    const int missingId = id + 1000;
    TeacherRepository* const repository =
        services.databaseSession()->teacherRepository();
    const TeacherProfileLookupResult typedProfileMissing =
        repository->lookupTeacherProfileById(missingId);
    QCOMPARE(typedProfileMissing.code, TeacherProfileLookupCode::Missing);
    const TeacherByIdLookupResult typedMissing =
        repository->lookupTeacherById(missingId);
    QCOMPARE(typedMissing.code, TeacherByIdLookupCode::Missing);
    const QString expectedMissingDiagnostic = QObject::tr(
        "Loading teacher failed for %1: no matching record exists."
        ).arg(QObject::tr("teacher id %1").arg(missingId));
    QCOMPARE(typedMissing.diagnostic, expectedMissingDiagnostic);

    const Result<Teacher> legacyMissing = repository->getTeacher(missingId);
    QVERIFY(!legacyMissing);
    QCOMPARE(legacyMissing.error(), typedMissing.diagnostic);

    const auto missing = port.readTeacherProfile(teacherId(missingId));
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(missing.error().message, utf8(typedProfileMissing.diagnostic));
    QVERIFY(missing.error().recoverable);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE teachers")),
        qPrintable(query.lastError().text()));
    const TeacherProfileLookupResult typedProfileFailure =
        repository->lookupTeacherProfileById(id);
    QCOMPARE(typedProfileFailure.code, TeacherProfileLookupCode::Failure);
    QVERIFY(!typedProfileFailure.diagnostic.isEmpty());

    const auto repositoryFailure = port.readTeacherProfile(teacherId(id));
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        repositoryFailure.error().message,
        utf8(typedProfileFailure.diagnostic)
        );
    QVERIFY(!repositoryFailure.error().recoverable);

    services.closeDatabase();
    unavailable = port.readTeacherProfile(teacherId(id));
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(NextPlatformApplicationServicesTeacherProfileReadPortTests)

#include "next_platform_application_services_teacher_profile_read_port_tests.moc"
