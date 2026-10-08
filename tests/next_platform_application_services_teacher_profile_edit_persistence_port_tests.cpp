#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/domain/teacher_profile.h"
#include "next/platform/application_services_teacher_profile_edit_persistence_port.h"

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
        QStringLiteral("teacher-profile-edit-port-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::TeacherProfileFields fields()
{
    return {
        .teacherKr = u"김수업",
        .teacherEn = u"Alexandra Kim",
        .preferredRomanization = u"Soo Kim",
        .preferredName = u"Alexandra Kim",
        .roomNumber = u"Room 204",
        .birthday = u"03-14",
        .phoneNumber = u"010-1234-5678",
        .wifiName = u"Class Network",
        .wifiPassword = u"wifi secret",
        .internetType = u"Both",
        .zoomId = u"alex.zoom",
        .zoomPassword = u"zoom secret",
        .projectionType = u"Zoom",
        .notes = u"  Notes stay exact at this boundary  "
    };
}

Domain::TeacherProfile profile(const int intId)
{
    return {
        .id = *Domain::TeacherId::fromString(std::to_string(intId)),
        .fields = fields()
    };
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김초기값");
    teacher.teacherEn = QStringLiteral("Before Save");
    teacher.preferredName = teacher.teacherEn;
    const auto created = services.teacherService()->create(teacher);
    return created ? *created : -1;
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

}

class NextPlatformApplicationServicesTeacherProfileEditPersistencePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void updatesAndReloadsEveryProfileFieldThroughActiveRepository();
    void mapsUnavailableSessionAndRepositoryFailures();
};

void NextPlatformApplicationServicesTeacherProfileEditPersistencePortTests::
updatesAndReloadsEveryProfileFieldThroughActiveRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int teacherId = createTeacher(services);
    QVERIFY(teacherId > 0);

    const Domain::TeacherProfile expected = profile(teacherId);
    Platform::ApplicationServicesTeacherProfileEditPersistencePort port(
        services);
    const Domain::Result<void> updated = port.update(expected);
    QVERIFY(updated);

    TeacherRepository* const repository =
        services.databaseSession()->teacherRepository();
    QVERIFY(repository);
    const Result<Teacher> stored = repository->getTeacher(teacherId);
    QVERIFY(stored);
    QCOMPARE(stored->id, teacherId);
    QCOMPARE(stored->teacherKr, QString::fromStdU16String(
        expected.fields.teacherKr));
    QCOMPARE(stored->teacherEn, QString::fromStdU16String(
        expected.fields.teacherEn));
    QCOMPARE(stored->preferredRomanization, QString::fromStdU16String(
        expected.fields.preferredRomanization));
    QCOMPARE(stored->preferredName, QString::fromStdU16String(
        expected.fields.preferredName));
    QCOMPARE(stored->roomNumber, QString::fromStdU16String(
        expected.fields.roomNumber));
    QCOMPARE(stored->birthday, QString::fromStdU16String(
        expected.fields.birthday));
    QCOMPARE(stored->phoneNumber, QString::fromStdU16String(
        expected.fields.phoneNumber));
    QCOMPARE(stored->wifiName, QString::fromStdU16String(
        expected.fields.wifiName));
    QCOMPARE(stored->wifiPassword, QString::fromStdU16String(
        expected.fields.wifiPassword));
    QCOMPARE(stored->internetType, QString::fromStdU16String(
        expected.fields.internetType));
    QCOMPARE(stored->zoomId, QString::fromStdU16String(
        expected.fields.zoomId));
    QCOMPARE(stored->zoomPassword, QString::fromStdU16String(
        expected.fields.zoomPassword));
    QCOMPARE(stored->projectionType, QString::fromStdU16String(
        expected.fields.projectionType));
    QCOMPARE(stored->notes, QString::fromStdU16String(
        expected.fields.notes));

    const Domain::Result<Domain::TeacherProfile> reloaded =
        port.reload(expected.id);
    QVERIFY(reloaded);
    QVERIFY(reloaded.value() == expected);
}

void NextPlatformApplicationServicesTeacherProfileEditPersistencePortTests::
mapsUnavailableSessionAndRepositoryFailures()
{
    ApplicationServices services;
    Platform::ApplicationServicesTeacherProfileEditPersistencePort port(
        services);

    const Domain::TeacherProfile unavailable = profile(1);
    const Domain::Result<void> noSession = port.update(unavailable);
    QVERIFY(!noSession);
    QCOMPARE(noSession.error().code, Domain::ErrorCode::NotFound);
    const auto noSessionReload = port.reload(unavailable.id);
    QVERIFY(!noSessionReload);
    QCOMPARE(noSessionReload.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int teacherId = createTeacher(services);
    QVERIFY(teacherId > 0);

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_teacher_profile_update "
        "BEFORE UPDATE ON teachers "
        "WHEN NEW.teacher_en = 'Reject Profile Update' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected profile update failure'); "
        "END"
        )));

    Domain::TeacherProfile rejected = profile(teacherId);
    rejected.fields.teacherEn = u"Reject Profile Update";
    const Domain::Result<void> updateFailure = port.update(rejected);
    QVERIFY(!updateFailure);
    QCOMPARE(updateFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(fromUtf8(updateFailure.error().message).contains(
        QStringLiteral("injected profile update failure")));

    const auto missingId = Domain::TeacherId::fromString(
        std::to_string(teacherId + 1000));
    QVERIFY(missingId);
    const Domain::Result<Domain::TeacherProfile> reloadFailure =
        port.reload(*missingId);
    QVERIFY(!reloadFailure);
    QCOMPARE(reloadFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(fromUtf8(reloadFailure.error().message).contains(
        QStringLiteral("no matching record"), Qt::CaseInsensitive));

    services.closeDatabase();
    const auto closedSession = port.reload(
        *Domain::TeacherId::fromString(std::to_string(teacherId)));
    QVERIFY(!closedSession);
    QCOMPARE(closedSession.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(
    NextPlatformApplicationServicesTeacherProfileEditPersistencePortTests
    )

#include "next_platform_application_services_teacher_profile_edit_persistence_port_tests.moc"
