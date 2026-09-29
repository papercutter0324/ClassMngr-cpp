#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/data_service.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_builder_source_snapshot.h"
#include "next/platform/application_services_schedule_builder_source_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory, const QString& prefix)
{
    return directory.filePath(
        QStringLiteral("%1-%2.tps").arg(
            prefix,
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

bool insertTeacher(
    QSqlDatabase database,
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredName,
    const QString& room,
    int* teacherId
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO teachers "
        "(teacher_kr, teacher_en, preferred_name, room_number) "
        "VALUES (?, ?, ?, ?)"
        ));
    insert.addBindValue(koreanName);
    insert.addBindValue(englishName);
    insert.addBindValue(preferredName);
    insert.addBindValue(room);
    if (!insert.exec())
    {
        return false;
    }
    *teacherId = insert.lastInsertId().toInt();
    return *teacherId > 0;
}

bool insertClassInfo(
    QSqlDatabase database,
    const int id,
    const std::optional<int> teacherId,
    const QString& grade,
    const QString& level,
    const QString& classColor,
    const QString& fontColor
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level, class_color, font_color) "
        "VALUES (?, %1, ?, ?, ?, ?)"
        ).arg(teacherId ? QStringLiteral("?") : QStringLiteral("NULL")));
    insert.addBindValue(id);
    if (teacherId)
    {
        insert.addBindValue(*teacherId);
    }
    insert.addBindValue(grade);
    insert.addBindValue(level);
    insert.addBindValue(classColor);
    insert.addBindValue(fontColor);
    return insert.exec();
}

bool insertSchedule(
    QSqlDatabase database,
    const QString& table,
    const int classIdValue,
    const QString& day,
    const QString& start,
    const QString& end
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO %1 (class_id, day, start_time, end_time) "
        "VALUES (?, ?, ?, ?)"
        ).arg(table));
    insert.addBindValue(classIdValue);
    insert.addBindValue(day);
    insert.addBindValue(start);
    insert.addBindValue(end);
    return insert.exec();
}

}

class NextPlatformApplicationServicesScheduleBuilderSourcePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsActiveSessionCompactSourceInRepositoryOrder();
    void distinguishesClosedUnavailableAndFailedReadsWithoutLegacyFallback();
};

void NextPlatformApplicationServicesScheduleBuilderSourcePortTests::
readsActiveSessionCompactSourceInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory, QStringLiteral("active"))));

    const auto unassignedTeacherClass = services.classService()->create(
        QStringLiteral("Omega no teacher")
        );
    const auto assignedClass = services.classService()->create(
        QStringLiteral("Beta assigned")
        );
    const auto invalidTeacherClass = services.classService()->create(
        QStringLiteral("Delta stale teacher")
        );
    const auto unassignedClassInfo = services.classService()->create(
        QStringLiteral("Gamma no profile")
        );
    const auto testingClass = services.classService()->create(
        QStringLiteral("Zulu testing only")
        );
    QVERIFY(unassignedTeacherClass);
    QVERIFY(assignedClass);
    QVERIFY(invalidTeacherClass);
    QVERIFY(unassignedClassInfo);
    QVERIFY(testingClass);

    const QSqlDatabase database = services.databaseSession()->database();
    int teacherId = -1;
    QVERIFY2(
        insertTeacher(
            database,
            QString::fromUcs4(U" 김\U0001F9ED "),
            QString::fromUcs4(U"  Teacher \U0001F9ED  "),
            QStringLiteral("  Preferred Name  "),
            QStringLiteral(" Room 9 "),
            &teacherId
            ),
        "Could not insert the schedule teacher."
        );

    QVERIFY2(
        insertClassInfo(
            database,
            *assignedClass,
            teacherId,
            QStringLiteral("E4"),
            QStringLiteral("Hercules"),
            QStringLiteral("#123456"),
            QStringLiteral("#ABCDEF")
            ),
        "Could not insert the assigned class profile."
        );
    QVERIFY2(
        insertClassInfo(
            database,
            *unassignedTeacherClass,
            std::nullopt,
            QStringLiteral("M1"),
            QStringLiteral("Solis"),
            QString(),
            QString()
            ),
        "Could not insert the class profile without a teacher."
        );
    QSqlQuery foreignKeysOff(database);
    QVERIFY(foreignKeysOff.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
    QVERIFY2(
        insertClassInfo(
            database,
            *invalidTeacherClass,
            999999,
            QStringLiteral("M2"),
            QStringLiteral("Ursa"),
            QStringLiteral("#334455"),
            QStringLiteral("#667788")
            ),
        "Could not insert the class profile with a stale teacher ID."
        );
    QSqlQuery foreignKeysOn(database);
    QVERIFY(foreignKeysOn.exec(QStringLiteral("PRAGMA foreign_keys = ON")));
    QVERIFY2(
        insertClassInfo(
            database,
            *testingClass,
            teacherId,
            QStringLiteral("M2"),
            QStringLiteral("Ursa"),
            QStringLiteral("#223344"),
            QStringLiteral("#556677")
            ),
        "Could not insert the testing class profile."
        );

    QSqlQuery testingRow(database);
    testingRow.prepare(QStringLiteral(
        "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
        ));
    testingRow.addBindValue(*testingClass);
    testingRow.addBindValue(QStringLiteral("Testing Room"));
    QVERIFY2(testingRow.exec(), qPrintable(testingRow.lastError().text()));

    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *assignedClass,
        QStringLiteral(" Raw Monday "),
        QStringLiteral("invalid start"),
        QStringLiteral(" opaque end ")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *assignedClass,
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_intensive_times"),
        *assignedClass,
        QStringLiteral("Not a weekday"),
        QStringLiteral("09:05"),
        QStringLiteral("end? ")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *unassignedTeacherClass,
        QStringLiteral("Tuesday"),
        QStringLiteral("5:00 PM"),
        QStringLiteral("5:50 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *invalidTeacherClass,
        QStringLiteral("Wednesday"),
        QStringLiteral("6:00 PM"),
        QStringLiteral("6:50 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *unassignedClassInfo,
        QStringLiteral("No class info"),
        QStringLiteral("Raw time"),
        QStringLiteral("Raw end")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        *testingClass,
        QStringLiteral("Saturday"),
        QStringLiteral("7:00 PM"),
        QStringLiteral("7:50 PM")
        ));

    Platform::ApplicationServicesScheduleBuilderSourcePort port(services);
    const auto result = Application::ScheduleBuilderSourceQueryHandler::execute(
        {},
        port
        );
    QVERIFY2(
        result,
        qPrintable(result
            ? QString()
            : QString::fromStdString(result.error().message))
        );

    QCOMPARE(result.value().classes.size(), std::size_t(4));
    const ScheduleClassInfoReadMetrics& readMetrics = services
        .databaseSession()
        ->classInfoRepository()
        ->scheduleClassInfoReadMetrics();
    QCOMPARE(readMetrics.scheduleClassInfosCallCount, 1);
    QCOMPARE(readMetrics.singleClassInfoReadCount, 0);
    QCOMPARE(readMetrics.metadataStatementCount, 1);
    QCOMPARE(readMetrics.regularScheduleStatementCount, 1);
    QCOMPARE(readMetrics.intensiveScheduleStatementCount, 1);

    // Creation IDs are Omega, Beta, Delta, Gamma; the repository returns
    // classes by name, so the snapshot order must be Beta, Delta, Gamma, Omega.
    const auto& assigned = result.value().classes[0];
    QVERIFY(assigned.classId == classId(*assignedClass));
    QCOMPARE(assigned.teacherKoreanName, std::u16string(u" 김\U0001F9ED "));
    QCOMPARE(assigned.teacherEnglishName,
             std::u16string(u"  Teacher \U0001F9ED  "));
    QCOMPARE(assigned.teacherPreferredName,
             std::u16string(u"  Preferred Name  "));
    QCOMPARE(assigned.roomNumber, std::u16string(u" Room 9 "));
    QCOMPARE(assigned.grade, std::u16string(u"E4"));
    QCOMPARE(assigned.level, std::u16string(u"Hercules"));
    QCOMPARE(assigned.classColor, std::u16string(u"#123456"));
    QCOMPARE(assigned.fontColor, std::u16string(u"#ABCDEF"));
    QCOMPARE(assigned.regularSchedule.size(), std::size_t(2));
    QCOMPARE(assigned.regularSchedule[0].day,
             std::u16string(u" Raw Monday "));
    QCOMPARE(assigned.regularSchedule[0].startTime,
             std::u16string(u"invalid start"));
    QCOMPARE(assigned.regularSchedule[0].endTime,
             std::u16string(u" opaque end "));
    QCOMPARE(assigned.regularSchedule[1].day, std::u16string(u"Monday"));
    QCOMPARE(assigned.intensiveSchedule.size(), std::size_t(1));
    QCOMPARE(assigned.intensiveSchedule[0].day,
             std::u16string(u"Not a weekday"));
    QCOMPARE(assigned.intensiveSchedule[0].startTime,
             std::u16string(u"09:05"));
    QCOMPARE(assigned.intensiveSchedule[0].endTime,
             std::u16string(u"end? "));

    const auto& invalidTeacher = result.value().classes[1];
    QVERIFY(invalidTeacher.classId == classId(*invalidTeacherClass));
    QVERIFY(invalidTeacher.teacherKoreanName.empty());
    QVERIFY(invalidTeacher.teacherEnglishName.empty());
    QCOMPARE(invalidTeacher.grade, std::u16string(u"M2"));
    QCOMPARE(invalidTeacher.regularSchedule.size(), std::size_t(1));

    const auto& noProfile = result.value().classes[2];
    QVERIFY(noProfile.classId == classId(*unassignedClassInfo));
    QVERIFY(noProfile.grade.empty());
    QVERIFY(noProfile.teacherEnglishName.empty());
    QCOMPARE(noProfile.regularSchedule.size(), std::size_t(1));
    QCOMPARE(noProfile.regularSchedule[0].day,
             std::u16string(u"No class info"));

    const auto& noTeacher = result.value().classes[3];
    QVERIFY(noTeacher.classId == classId(*unassignedTeacherClass));
    QVERIFY(noTeacher.teacherKoreanName.empty());
    QVERIFY(noTeacher.teacherEnglishName.empty());
    QCOMPARE(noTeacher.grade, std::u16string(u"M1"));
    QCOMPARE(noTeacher.level, std::u16string(u"Solis"));
    QCOMPARE(noTeacher.classColor, std::u16string(u"#FFFFFF"));
    QCOMPARE(noTeacher.fontColor, std::u16string(u"#000000"));
    QCOMPARE(noTeacher.regularSchedule.size(), std::size_t(1));
}

void NextPlatformApplicationServicesScheduleBuilderSourcePortTests::
distinguishesClosedUnavailableAndFailedReadsWithoutLegacyFallback()
{
    const Application::ScheduleBuilderSourceQuery query;

    Platform::ApplicationServicesScheduleBuilderSourcePort nullPort(nullptr);
    auto result = nullPort.readScheduleClasses(query);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory, QStringLiteral("closed"))));
    services.closeDatabase();

    const QString legacyPath = databasePath(directory, QStringLiteral("legacy"));
    DataService legacyData(legacyPath);
    QVERIFY(legacyData.open());
    ClassService legacyService(&legacyData);
    QVERIFY(legacyService.isAvailable());
    QVERIFY(!legacyService.scheduleClassInfos());

    Platform::ApplicationServicesScheduleBuilderSourcePort closedPort(services);
    result = closedPort.readScheduleClasses(query);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices failedReadServices;
    QVERIFY(failedReadServices.openDatabase(
        databasePath(directory, QStringLiteral("read-failure"))
        ));
    QSqlQuery dropScheduleTable(
        failedReadServices.databaseSession()->database()
        );
    QVERIFY(dropScheduleTable.exec(QStringLiteral("DROP TABLE class_times")));

    Platform::ApplicationServicesScheduleBuilderSourcePort failedReadPort(
        failedReadServices
        );
    result = Application::ScheduleBuilderSourceQueryHandler::execute(
        query,
        failedReadPort
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesScheduleBuilderSourcePortTests
    )

#include "next_platform_application_services_schedule_builder_source_port_tests.moc"
