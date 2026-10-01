#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/schedule_import_state_snapshot.h"
#include "next/platform/application_services_schedule_import_state_snapshot_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

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

Domain::TeacherId teacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

bool insertTeacher(
    QSqlDatabase database,
    const QString& koreanName,
    const QString& englishName,
    const QString& room,
    int* teacherId
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO teachers (teacher_kr, teacher_en, room_number) "
        "VALUES (?, ?, ?)"
        ));
    insert.addBindValue(koreanName);
    insert.addBindValue(englishName);
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
    const int classIdValue,
    const std::optional<int> teacherIdValue,
    const QString& grade,
    const QString& level,
    const QString& color
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level, class_color) "
        "VALUES (?, %1, ?, ?, ?)"
        ).arg(teacherIdValue ? QStringLiteral("?") : QStringLiteral("NULL")));
    insert.addBindValue(classIdValue);
    if (teacherIdValue)
    {
        insert.addBindValue(*teacherIdValue);
    }
    insert.addBindValue(grade);
    insert.addBindValue(level);
    insert.addBindValue(color);
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

bool dropTable(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    return query.exec(QStringLiteral("DROP TABLE %1").arg(table));
}

const Application::ScheduleImportStateSnapshotFailure* failure(
    const Application::ScheduleImportStateSnapshotOutcome& outcome
    )
{
    return std::get_if<Application::ScheduleImportStateSnapshotFailure>(
        &outcome
        );
}

}

class NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void mapsActiveSessionRecordsInRepositoryOrder();
    void reportsUnavailableAndClosedSessions();
    void reportsEachRepositoryReadFailureWithoutLegacyFallback();
};

void NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests::
mapsActiveSessionRecordsInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        databasePath(directory, QStringLiteral("active"))
        ));
    const int firstDuplicate = services.classService()->create(
        QStringLiteral("Duplicate")
        ).value_or(-1);
    const int secondDuplicate = services.classService()->create(
        QStringLiteral("Duplicate")
        ).value_or(-1);
    const int unassigned = services.classService()->create(
        QStringLiteral("Unassigned")
        ).value_or(-1);
    const int testingOnly = services.classService()->create(
        QStringLiteral("Testing only")
        ).value_or(-1);
    QVERIFY(firstDuplicate > 0);
    QVERIFY(secondDuplicate > 0);
    QVERIFY(unassigned > 0);
    QVERIFY(testingOnly > 0);

    const QSqlDatabase database = services.databaseSession()->database();
    int teacherIdValue = -1;
    QVERIFY2(
        insertTeacher(
            database,
            QStringLiteral("  \uAE40 \uC120\uC0DD\uB2D8  "),
            QStringLiteral("Zulu teacher"),
            QStringLiteral(" Room 9 "),
            &teacherIdValue
            ),
        "Could not insert the assigned teacher."
        );
    int unusedTeacherId = -1;
    QVERIFY2(
        insertTeacher(
            database,
            QStringLiteral("\uBC15 \uC120\uC0DD\uB2D8"),
            QStringLiteral("Alpha teacher"),
            QStringLiteral("415"),
            &unusedTeacherId
            ),
        "Could not insert the unused teacher."
        );
    QVERIFY(insertClassInfo(
        database,
        firstDuplicate,
        teacherIdValue,
        QStringLiteral(" E4 "),
        QStringLiteral("Hercules"),
        QStringLiteral("#123456")
        ));
    QVERIFY(insertClassInfo(
        database,
        secondDuplicate,
        std::nullopt,
        QStringLiteral("E5"),
        QStringLiteral("Athena"),
        QString()
        ));
    QVERIFY(insertClassInfo(
        database,
        unassigned,
        std::nullopt,
        QStringLiteral("M1"),
        QStringLiteral("Solis"),
        QStringLiteral("#223344")
        ));
    QVERIFY(insertClassInfo(
        database,
        testingOnly,
        teacherIdValue,
        QStringLiteral("M2"),
        QStringLiteral("Ursa"),
        QStringLiteral("#334455")
        ));
    QSqlQuery testingRow(database);
    testingRow.prepare(QStringLiteral(
        "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
        ));
    testingRow.addBindValue(testingOnly);
    testingRow.addBindValue(QStringLiteral("Testing room"));
    QVERIFY(testingRow.exec());

    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        firstDuplicate,
        QStringLiteral(" Raw Monday "),
        QStringLiteral("bad start"),
        QStringLiteral("opaque end")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        firstDuplicate,
        QStringLiteral("Thursday"),
        QStringLiteral("5:00 PM"),
        QStringLiteral("5:50 PM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_intensive_times"),
        firstDuplicate,
        QStringLiteral("Friday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
        ));

    const Result<QList<Classroom>> legacyOrder =
        services.databaseSession()->classRepository()->getClasses();
    const Result<QList<Teacher>> teacherOrder =
        services.databaseSession()->teacherRepository()->getAllTeachers();
    QVERIFY(legacyOrder);
    QVERIFY(teacherOrder);

    Platform::ApplicationServicesScheduleImportStateSnapshotPort port(services);
    const auto result = Application::ScheduleImportStateSnapshotQueryHandler::
        execute({}, port);
    const auto* snapshot =
        std::get_if<Application::ScheduleImportStateSnapshot>(&result);
    QVERIFY2(snapshot, failure(result)
        ? qPrintable(QString::fromStdString(failure(result)->message))
        : "Expected a complete state snapshot.");

    QCOMPARE(snapshot->teachers.size(),
             static_cast<std::size_t>(teacherOrder->size()));
    for (std::size_t index = 0; index < snapshot->teachers.size(); ++index)
    {
        QCOMPARE(
            snapshot->teachers[index].id.value(),
            std::to_string((*teacherOrder)[static_cast<qsizetype>(index)].id)
            );
    }
    QCOMPARE(snapshot->classes.size(),
             static_cast<std::size_t>(legacyOrder->size()));
    for (std::size_t index = 0; index < snapshot->classes.size(); ++index)
    {
        QCOMPARE(
            snapshot->classes[index].id.value(),
            std::to_string((*legacyOrder)[static_cast<qsizetype>(index)].id)
            );
    }

    const auto assigned = std::find_if(
        snapshot->classes.begin(),
        snapshot->classes.end(),
        [firstDuplicate](const auto& value)
        {
            return value.id.value() == std::to_string(firstDuplicate);
        }
        );
    QVERIFY(assigned != snapshot->classes.end());
    QVERIFY(assigned->teacherId == teacherId(teacherIdValue));
    QCOMPARE(assigned->className, std::u16string(u"Duplicate"));
    QCOMPARE(assigned->grade, std::u16string(u" E4 "));
    QCOMPARE(assigned->classColor, std::u16string(u"#123456"));
    QCOMPARE(assigned->normalTimes.size(), std::size_t(2));
    QCOMPARE(assigned->normalTimes[0].day, std::u16string(u" Raw Monday "));
    QCOMPARE(assigned->normalTimes[0].startTime, std::u16string(u"bad start"));
    QCOMPARE(assigned->normalTimes[1].day, std::u16string(u"Thursday"));
    QCOMPARE(assigned->intensiveTimes.size(), std::size_t(1));

    const auto unassignedSnapshot = std::find_if(
        snapshot->classes.begin(),
        snapshot->classes.end(),
        [unassigned](const auto& value)
        {
            return value.id.value() == std::to_string(unassigned);
        }
        );
    QVERIFY(unassignedSnapshot != snapshot->classes.end());
    QVERIFY(unassignedSnapshot->teacherId == teacherId(-1));
    QCOMPARE(unassignedSnapshot->classColor, std::u16string(u"#223344"));

    const auto unusedTeacher = std::find_if(
        snapshot->teachers.begin(),
        snapshot->teachers.end(),
        [unusedTeacherId](const auto& value)
        {
            return value.id.value() == std::to_string(unusedTeacherId);
        }
        );
    QVERIFY(unusedTeacher != snapshot->teachers.end());
    QCOMPARE(unusedTeacher->koreanName, std::u16string(u"\uBC15 \uC120\uC0DD\uB2D8"));
    QCOMPARE(unusedTeacher->roomNumber, std::u16string(u"415"));

    const ScheduleClassInfoReadMetrics& metrics = services
        .databaseSession()
        ->classInfoRepository()
        ->scheduleClassInfoReadMetrics();
    QCOMPARE(metrics.scheduleClassInfosCallCount, 1);
    QCOMPARE(metrics.singleClassInfoReadCount, 0);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 1);
    QCOMPARE(metrics.intensiveScheduleStatementCount, 1);
}

void NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests::
reportsUnavailableAndClosedSessions()
{
    const Application::ScheduleImportStateSnapshotQuery query;
    Platform::ApplicationServicesScheduleImportStateSnapshotPort nullPort(
        nullptr
        );
    auto result = nullPort.readCurrentScheduleImportState(query);
    const auto* error = failure(result);
    QVERIFY(error);
    QCOMPARE(
        error->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            ActiveSessionUnavailable
        );
    QCOMPARE(
        error->source,
        Application::ScheduleImportStateSnapshotFailureSource::Session
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(
        databasePath(directory, QStringLiteral("closed"))
        ));
    closedServices.closeDatabase();
    Platform::ApplicationServicesScheduleImportStateSnapshotPort closedPort(
        closedServices
        );
    result = Application::ScheduleImportStateSnapshotQueryHandler::execute(
        query,
        closedPort
        );
    error = failure(result);
    QVERIFY(error);
    QCOMPARE(
        error->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            ActiveSessionUnavailable
        );
    QCOMPARE(
        error->source,
        Application::ScheduleImportStateSnapshotFailureSource::Session
        );
}

void NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests::
reportsEachRepositoryReadFailureWithoutLegacyFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Application::ScheduleImportStateSnapshotQuery query;

    const auto runFailure = [&](const QString& prefix,
                                const QString& table,
                                const Application::
                                    ScheduleImportStateSnapshotFailureSource
                                    expectedSource)
    {
        ApplicationServices services;
        if (!services.openDatabase(databasePath(directory, prefix)))
        {
            return Application::ScheduleImportStateSnapshotOutcome(
                Application::ScheduleImportStateSnapshotFailure{
                    Application::ScheduleImportStateSnapshotFailureKind::
                        ActiveSessionUnavailable,
                    Application::ScheduleImportStateSnapshotFailureSource::
                        Session,
                    "test database did not open"
                }
                );
        }
        QSqlQuery foreignKeysOff(
            services.databaseSession()->database()
            );
        if (!foreignKeysOff.exec(QStringLiteral("PRAGMA foreign_keys = OFF")))
        {
            return Application::ScheduleImportStateSnapshotOutcome(
                Application::ScheduleImportStateSnapshotFailure{
                    Application::ScheduleImportStateSnapshotFailureKind::
                        RepositoryReadFailed,
                    expectedSource,
                    foreignKeysOff.lastError().text().toStdString()
                }
                );
        }
        if (!dropTable(services.databaseSession()->database(), table))
        {
            return Application::ScheduleImportStateSnapshotOutcome(
                Application::ScheduleImportStateSnapshotFailure{
                    Application::ScheduleImportStateSnapshotFailureKind::
                        RepositoryReadFailed,
                    expectedSource,
                    "could not drop test table"
                }
                );
        }
        Platform::ApplicationServicesScheduleImportStateSnapshotPort port(
            services
            );
        return Application::ScheduleImportStateSnapshotQueryHandler::execute(
            query,
            port
            );
    };

    const auto classesFailure = runFailure(
        QStringLiteral("classes-fail"),
        QStringLiteral("classes"),
        Application::ScheduleImportStateSnapshotFailureSource::Classes
        );
    QVERIFY(failure(classesFailure));
    QCOMPARE(
        failure(classesFailure)->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            RepositoryReadFailed
        );
    QCOMPARE(
        failure(classesFailure)->source,
        Application::ScheduleImportStateSnapshotFailureSource::Classes
        );

    const auto teachersFailure = runFailure(
        QStringLiteral("teachers-fail"),
        QStringLiteral("teachers"),
        Application::ScheduleImportStateSnapshotFailureSource::Teachers
        );
    QVERIFY(failure(teachersFailure));
    QCOMPARE(
        failure(teachersFailure)->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            RepositoryReadFailed
        );
    QCOMPARE(
        failure(teachersFailure)->source,
        Application::ScheduleImportStateSnapshotFailureSource::Teachers
        );

    const auto regularScheduleFailure = runFailure(
        QStringLiteral("regular-fail"),
        QStringLiteral("class_times"),
        Application::ScheduleImportStateSnapshotFailureSource::ClassSchedules
        );
    QVERIFY(failure(regularScheduleFailure));
    QCOMPARE(
        failure(regularScheduleFailure)->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            RepositoryReadFailed
        );
    QCOMPARE(
        failure(regularScheduleFailure)->source,
        Application::ScheduleImportStateSnapshotFailureSource::ClassSchedules
        );

    const auto intensiveScheduleFailure = runFailure(
        QStringLiteral("intensive-fail"),
        QStringLiteral("class_intensive_times"),
        Application::ScheduleImportStateSnapshotFailureSource::ClassSchedules
        );
    QVERIFY(failure(intensiveScheduleFailure));
    QCOMPARE(
        failure(intensiveScheduleFailure)->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            RepositoryReadFailed
        );
    QCOMPARE(
        failure(intensiveScheduleFailure)->source,
        Application::ScheduleImportStateSnapshotFailureSource::ClassSchedules
        );

    const auto classInfoFailure = runFailure(
        QStringLiteral("class-info-fail"),
        QStringLiteral("class_info"),
        Application::ScheduleImportStateSnapshotFailureSource::ClassSchedules
        );
    QVERIFY(failure(classInfoFailure));
    QCOMPARE(
        failure(classInfoFailure)->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            RepositoryReadFailed
        );
    QCOMPARE(
        failure(classInfoFailure)->source,
        Application::ScheduleImportStateSnapshotFailureSource::ClassSchedules
        );
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests
    )

#include "next_platform_application_services_schedule_import_state_snapshot_port_tests.moc"
