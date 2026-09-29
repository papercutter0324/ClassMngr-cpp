#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_block_repository.h"
#include "next/application/schedule_testing_assignment_read_query.h"
#include "next/platform/application_services_schedule_testing_assignment_read_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

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

bool execute(
    QSqlDatabase database,
    const QString& sql,
    const QVariantList& values = {}
    )
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant& value : values)
    {
        query.addBindValue(value);
    }
    return query.exec();
}

int insertClass(
    QSqlDatabase database,
    const QString& name,
    bool testingClass,
    bool classInfo,
    int teacherId = -1
    )
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("INSERT INTO classes (name) VALUES (?)"));
    query.addBindValue(name);
    if (!query.exec())
    {
        return -1;
    }
    const int classId = query.lastInsertId().toInt();

    if (classInfo)
    {
        if (!execute(
                database,
                QStringLiteral(R"(
                    INSERT INTO class_info (
                        class_id, teacher_id, class_grade, class_level,
                        class_color, font_color
                    ) VALUES (?, ?, ?, ?, ?, ?)
                )"),
                {
                    classId,
                    teacherId > 0 ? QVariant(teacherId) : QVariant(),
                    QStringLiteral("M2"),
                    QStringLiteral("Mixed (High)"),
                    QStringLiteral("#123456"),
                    QStringLiteral("#FFFFFF")
                }
                ))
        {
            return -1;
        }
    }

    if (testingClass && !execute(
            database,
            QStringLiteral(
                "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
                ),
            {classId, QStringLiteral("Library")}
            ))
    {
        return -1;
    }

    return classId;
}

bool insertAssignment(
    QSqlDatabase database,
    const QString& day,
    const QString& startTime,
    const QString& room,
    const QVariant& classId
    )
{
    return execute(
        database,
        QStringLiteral(R"(
            INSERT INTO schedule_testing_blocks (day, start_time, room, class_id)
            VALUES (?, ?, ?, ?)
        )"),
        {day, startTime, room, classId}
        );
}

}

class NextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsOrderedRawAssignmentsAndJoinedSpecialMetadata();
    void usesOneStatementAsAssignmentCountGrows();
    void distinguishesUnavailableSessionFromRepositoryFailure();
};

void NextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests::
readsOrderedRawAssignmentsAndJoinedSpecialMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("active")
        )));
    const QSqlDatabase database = services.databaseSession()->database();

    QSqlQuery teacher(database);
    QVERIFY(teacher.exec(QStringLiteral(R"(
        INSERT INTO teachers (teacher_kr, teacher_en, preferred_name)
        VALUES ('김선생', 'Teacher Kim', 'Kim')
    )")));
    const int teacherId = teacher.lastInsertId().toInt();

    const int fullMetadataClass = insertClass(
        database,
        QStringLiteral("Oral Testing"),
        true,
        true,
        teacherId
        );
    QVERIFY(fullMetadataClass > 0);
    const int missingInfoClass = insertClass(
        database,
        QStringLiteral("Missing Details"),
        true,
        false
        );
    QVERIFY(missingInfoClass > 0);
    const int missingTestingClass = insertClass(
        database,
        QStringLiteral("No Testing Class"),
        false,
        false
        );
    QVERIFY(missingTestingClass > 0);

    QVERIFY(insertAssignment(
        database,
        QStringLiteral("Tuesday "),
        QStringLiteral("09:05 "),
        QStringLiteral(""),
        missingInfoClass
        ));
    QVERIFY(insertAssignment(
        database,
        QStringLiteral("Monday"),
        QStringLiteral("10:00"),
        QStringLiteral(""),
        fullMetadataClass
        ));
    QVERIFY(insertAssignment(
        database,
        QStringLiteral("Monday"),
        QStringLiteral("09:00"),
        QStringLiteral("Room 4"),
        QVariant()
        ));
    QVERIFY(insertAssignment(
        database,
        QStringLiteral("Wednesday"),
        QStringLiteral("11:00"),
        QStringLiteral(""),
        missingTestingClass
        ));

    Platform::ApplicationServicesScheduleTestingAssignmentReadPort port(
        services
        );
    const auto result = Application::
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

    QVERIFY(result);
    QCOMPARE(result.value().rows.size(), std::size_t(4));
    QCOMPARE(result.value().rows[0].day, std::u16string(u"Monday"));
    QCOMPARE(result.value().rows[0].startTime, std::u16string(u"09:00"));
    QCOMPARE(result.value().rows[0].room, std::u16string(u"Room 4"));
    QCOMPARE(result.value().rows[0].classId, -1);
    QVERIFY(!result.value().rows[0].specialClass);

    QCOMPARE(result.value().rows[1].startTime, std::u16string(u"10:00"));
    QCOMPARE(result.value().rows[1].classId, fullMetadataClass);
    QVERIFY(result.value().rows[1].specialClass.has_value());
    QCOMPARE(
        result.value().rows[1].specialClass->name,
        std::u16string(u"Oral Testing")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->teacherKoreanName,
        std::u16string(u"김선생")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->teacherEnglishName,
        std::u16string(u"Teacher Kim")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->teacherPreferredName,
        std::u16string(u"Kim")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->room,
        std::u16string(u"Library")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->grade,
        std::u16string(u"M2")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->level,
        std::u16string(u"Mixed (High)")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->classColor,
        std::u16string(u"#123456")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->fontColor,
        std::u16string(u"#FFFFFF")
        );

    QCOMPARE(result.value().rows[2].day, std::u16string(u"Tuesday "));
    QCOMPARE(result.value().rows[2].startTime, std::u16string(u"09:05 "));
    QCOMPARE(result.value().rows[2].classId, missingInfoClass);
    QVERIFY(result.value().rows[2].specialClass.has_value());
    QCOMPARE(result.value().rows[2].specialClass->name,
             std::u16string(u"Missing Details"));
    QCOMPARE(result.value().rows[2].specialClass->teacherEnglishName,
             std::u16string());
    QCOMPARE(result.value().rows[2].specialClass->grade,
             std::u16string());
    QCOMPARE(result.value().rows[2].specialClass->level,
             std::u16string());
    QCOMPARE(result.value().rows[2].specialClass->classColor,
             std::u16string(u"#FFFFFF"));
    QCOMPARE(result.value().rows[2].specialClass->fontColor,
             std::u16string(u"#000000"));
    QCOMPARE(result.value().rows[3].day, std::u16string(u"Wednesday"));
    QCOMPARE(result.value().rows[3].classId, missingTestingClass);
    QVERIFY(!result.value().rows[3].specialClass);

    const TestingAssignmentDisplayReadMetrics& metrics = services
        .databaseSession()
        ->testingBlockRepository()
        ->testingAssignmentDisplayReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.statementCount, 1);
}

void NextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests::
usesOneStatementAsAssignmentCountGrows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("query-count")
        )));
    const QSqlDatabase database = services.databaseSession()->database();
    QVERIFY(insertAssignment(
        database,
        QStringLiteral("Monday"),
        QStringLiteral("09:00"),
        QStringLiteral("Room"),
        QVariant()
        ));

    Platform::ApplicationServicesScheduleTestingAssignmentReadPort port(
        services
        );
    auto result = Application::
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);
    QVERIFY(result);
    QCOMPARE(result.value().rows.size(), std::size_t(1));
    QCOMPARE(
        services.databaseSession()->testingBlockRepository()
            ->testingAssignmentDisplayReadMetrics().statementCount,
        1
        );

    for (int index = 0; index < 40; ++index)
    {
        QVERIFY(insertAssignment(
            database,
            QStringLiteral("Day %1").arg(index),
            QStringLiteral("%1:00").arg(index),
            QStringLiteral("Room %1").arg(index),
            QVariant()
            ));
    }

    result = Application::
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);
    QVERIFY(result);
    QCOMPARE(result.value().rows.size(), std::size_t(41));
    const TestingAssignmentDisplayReadMetrics& metrics = services
        .databaseSession()
        ->testingBlockRepository()
        ->testingAssignmentDisplayReadMetrics();
    QCOMPARE(metrics.callCount, 2);
    QCOMPARE(metrics.statementCount, 2);
}

void NextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests::
distinguishesUnavailableSessionFromRepositoryFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices unavailableServices;
    Platform::ApplicationServicesScheduleTestingAssignmentReadPort
        unavailablePort(unavailableServices);
    auto result = Application::
        ScheduleTestingAssignmentReadQueryHandler::execute(
            {},
            unavailablePort
            );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(
        directory,
        QStringLiteral("closed")
        )));
    closedServices.closeDatabase();
    Platform::ApplicationServicesScheduleTestingAssignmentReadPort closedPort(
        closedServices
        );
    result = Application::ScheduleTestingAssignmentReadQueryHandler::execute(
        {},
        closedPort
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices failedReadServices;
    QVERIFY(failedReadServices.openDatabase(databasePath(
        directory,
        QStringLiteral("failed-read")
        )));
    QSqlQuery dropAssignments(
        failedReadServices.databaseSession()->database()
        );
    QVERIFY(dropAssignments.exec(QStringLiteral(
        "DROP TABLE schedule_testing_blocks"
        )));
    Platform::ApplicationServicesScheduleTestingAssignmentReadPort failedReadPort(
        failedReadServices
        );
    result = Application::
        ScheduleTestingAssignmentReadQueryHandler::execute(
            {},
            failedReadPort
            );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("Loading testing blocks")
            != std::string::npos);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests
    )

#include "next_platform_application_services_schedule_testing_assignment_read_port_tests.moc"
