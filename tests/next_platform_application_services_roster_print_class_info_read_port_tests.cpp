#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/roster_print_class_info_read_query.h"
#include "next/platform/application_services_roster_print_class_info_read_port.h"

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
        QStringLiteral("roster-print-class-info-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

ClassInfo classInfo(const int id, const int teacherId)
{
    ClassInfo info;
    info.classId = id;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E5");
    info.classLevel = QStringLiteral("Odyssey");
    info.readingBook = QStringLiteral("excluded book");
    info.notes = QStringLiteral("excluded notes");
    info.classTimes = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("5:00 PM"),
            QStringLiteral("5:50 PM")
        },
        {
            QStringLiteral("Friday"),
            QStringLiteral("6:00 PM"),
            QStringLiteral("6:50 PM")
        }
    };
    info.intensiveTimes = {
        {
            QStringLiteral("Saturday"),
            QStringLiteral("12:00 PM"),
            QStringLiteral("12:50 PM")
        }
    };
    return info;
}

}

class NextPlatformApplicationServicesRosterPrintClassInfoReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsOnlyPrintFieldsAndRegularScheduleFromActiveSession();
    void missingSessionAndInvalidIdsFail();
    void absentClassInfoRowReturnsBlankMetadataAndRegularSchedule();
    void repositoryReadFailurePropagates();
};

void NextPlatformApplicationServicesRosterPrintClassInfoReadPortTests::
mapsOnlyPrintFieldsAndRegularScheduleFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("Teacher Korean");
    teacher.teacherEn = QStringLiteral("Teacher English");
    teacher.preferredName = QStringLiteral("excluded preferred name");
    teacher.roomNumber = QStringLiteral("506");
    teacher.wifiName = QStringLiteral("Campus WiFi");
    teacher.wifiPassword = QStringLiteral("wifi-secret");
    teacher.zoomId = QStringLiteral("zoom-id");
    teacher.zoomPassword = QStringLiteral("zoom-secret");
    const auto teacherId =
        services.databaseSession()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(teacherId);

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster print")
        );
    QVERIFY(createdClass);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, *teacherId)
        ));

    Platform::ApplicationServicesRosterPrintClassInfoReadPort port(&services);
    const Application::RosterPrintClassInfoReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));
    QVERIFY(result);

    const auto& snapshot = result.value();
    QCOMPARE(snapshot.classId, classId(*createdClass));
    QCOMPARE(snapshot.classGrade, std::u16string(u"E5"));
    QCOMPARE(snapshot.classLevel, std::u16string(u"Odyssey"));
    QCOMPARE(snapshot.teacherEn, std::u16string(u"Teacher English"));
    QCOMPARE(snapshot.teacherKr, std::u16string(u"Teacher Korean"));
    QCOMPARE(snapshot.roomNumber, std::u16string(u"506"));
    QCOMPARE(snapshot.wifiName, std::u16string(u"Campus WiFi"));
    QCOMPARE(snapshot.wifiPassword, std::u16string(u"wifi-secret"));
    QCOMPARE(snapshot.zoomId, std::u16string(u"zoom-id"));
    QCOMPARE(snapshot.zoomPassword, std::u16string(u"zoom-secret"));
    QCOMPARE(snapshot.regularSchedule.size(), std::size_t(2));
    QCOMPARE(snapshot.regularSchedule[0].day, std::u16string(u"Tuesday"));
    QCOMPARE(snapshot.regularSchedule[0].startTime, std::u16string(u"5:00 PM"));
    QCOMPARE(snapshot.regularSchedule[0].endTime, std::u16string(u"5:50 PM"));
    QCOMPARE(snapshot.regularSchedule[1].day, std::u16string(u"Friday"));
    QCOMPARE(snapshot.regularSchedule[1].startTime, std::u16string(u"6:00 PM"));
    QCOMPARE(snapshot.regularSchedule[1].endTime, std::u16string(u"6:50 PM"));

    QSqlQuery dropIntensiveTimes(services.databaseSession()->database());
    QVERIFY(dropIntensiveTimes.exec(QStringLiteral(
        "DROP TABLE class_intensive_times"
        )));
    const auto withoutIntensiveTable = query.execute(classId(*createdClass));
    QVERIFY(withoutIntensiveTable);
    QCOMPARE(withoutIntensiveTable.value().regularSchedule.size(), std::size_t(2));
}

void NextPlatformApplicationServicesRosterPrintClassInfoReadPortTests::
missingSessionAndInvalidIdsFail()
{
    ApplicationServices services;
    Platform::ApplicationServicesRosterPrintClassInfoReadPort port(&services);

    const auto noSession = port.readRosterPrintClassInfo(classId(42));
    QVERIFY(!noSession);
    QCOMPARE(noSession.error().code, Domain::ErrorCode::NotFound);

    const auto nonCanonical = Domain::ClassId::fromString("042");
    QVERIFY(nonCanonical);
    const auto invalid = port.readRosterPrintClassInfo(*nonCanonical);
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, Domain::ErrorCode::InvalidInput);

    Platform::ApplicationServicesRosterPrintClassInfoReadPort nullPort(nullptr);
    const auto noServices = nullPort.readRosterPrintClassInfo(classId(42));
    QVERIFY(!noServices);
    QCOMPARE(noServices.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesRosterPrintClassInfoReadPortTests::
absentClassInfoRowReturnsBlankMetadataAndRegularSchedule()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("No class-info row")
        );
    QVERIFY(createdClass);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, -1)
        ));

    QSqlQuery deleteClassInfo(services.databaseSession()->database());
    deleteClassInfo.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id = ?"
        ));
    deleteClassInfo.addBindValue(*createdClass);
    QVERIFY(deleteClassInfo.exec());

    Platform::ApplicationServicesRosterPrintClassInfoReadPort port(&services);
    const Application::RosterPrintClassInfoReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));
    QVERIFY(result);
    QCOMPARE(result.value().classId, classId(*createdClass));
    QVERIFY(result.value().classGrade.empty());
    QVERIFY(result.value().classLevel.empty());
    QVERIFY(result.value().teacherEn.empty());
    QVERIFY(result.value().teacherKr.empty());
    QVERIFY(result.value().roomNumber.empty());
    QVERIFY(result.value().wifiName.empty());
    QVERIFY(result.value().wifiPassword.empty());
    QVERIFY(result.value().zoomId.empty());
    QVERIFY(result.value().zoomPassword.empty());
    QCOMPARE(result.value().regularSchedule.size(), std::size_t(2));
    QCOMPARE(result.value().regularSchedule[0].day, std::u16string(u"Tuesday"));
    QCOMPARE(result.value().regularSchedule[1].day, std::u16string(u"Friday"));
}

void NextPlatformApplicationServicesRosterPrintClassInfoReadPortTests::
repositoryReadFailurePropagates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Repository failure")
        );
    QVERIFY(createdClass);

    QSqlQuery dropTimes(services.databaseSession()->database());
    QVERIFY(dropTimes.exec(QStringLiteral("DROP TABLE class_times")));

    Platform::ApplicationServicesRosterPrintClassInfoReadPort port(&services);
    const Application::RosterPrintClassInfoReadQuery query(port);
    const auto failure = query.execute(classId(*createdClass));
    QVERIFY(!failure);
    QVERIFY(!failure.error().message.empty());
    QCOMPARE(failure.error().code, Domain::ErrorCode::Technical);
}

QTEST_MAIN(NextPlatformApplicationServicesRosterPrintClassInfoReadPortTests)

#include "next_platform_application_services_roster_print_class_info_read_port_tests.moc"
