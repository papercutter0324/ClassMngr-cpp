#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/data_service.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_builder_source_snapshot.h"
#include "next/platform/application_services_schedule_builder_source_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <limits>
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

bool insertPersistedClass(
    QSqlDatabase database,
    const qint64 classIdValue,
    const QString& name
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO classes (id, name) VALUES (?, ?)"
        ));
    insert.addBindValue(classIdValue);
    insert.addBindValue(name);
    return insert.exec();
}

bool insertSchedule(
    QSqlDatabase database,
    const QString& table,
    const qint64 classIdValue,
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
    void readsActiveWorkspaceCompactSourceInRepositoryOrder();
    void readsEveryClassAndScheduleRowBeyondTwentyFive();
    void leavesResultMetricsUnchangedForEmptyAndFailedReads();
    void validatesStoredScheduleClassIdsBeforeNarrowing();
    void distinguishesClosedUnavailableAndFailedReadsWithoutLegacyFallback();
};

void NextPlatformApplicationServicesScheduleBuilderSourcePortTests::
readsActiveWorkspaceCompactSourceInRepositoryOrder()
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

    const QSqlDatabase database = services.workspaceStore()->database();
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
        QString::fromUcs4(U" Raw \U0001F9ED Monday "),
        QStringLiteral("invalid start"),
        QStringLiteral("")
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
        .workspaceStore()
        ->classInfoRepository()
        ->scheduleClassInfoReadMetrics();
    QCOMPARE(readMetrics.scheduleClassInfosCallCount, 1);
    QCOMPARE(readMetrics.singleClassInfoReadCount, 0);
    QCOMPARE(readMetrics.metadataStatementCount, 1);
    QCOMPARE(readMetrics.regularScheduleStatementCount, 1);
    QCOMPARE(readMetrics.intensiveScheduleStatementCount, 1);
    QCOMPARE(readMetrics.completedClassCount, qint64(4));
    QCOMPARE(readMetrics.regularScheduleRowCount, qint64(5));
    QCOMPARE(readMetrics.intensiveScheduleRowCount, qint64(1));

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
    QCOMPARE(assigned.regularSchedule[0].day.value(),
             std::u16string(u" Raw \U0001F9ED Monday "));
    QCOMPARE(assigned.regularSchedule[0].startTime.value(),
             std::u16string(u"invalid start"));
    QVERIFY(assigned.regularSchedule[0].endTime.value().empty());
    QCOMPARE(assigned.regularSchedule[1].day.value(), std::u16string(u"Monday"));
    QCOMPARE(assigned.intensiveSchedule.size(), std::size_t(1));
    QCOMPARE(assigned.intensiveSchedule[0].day.value(),
             std::u16string(u"Not a weekday"));
    QCOMPARE(assigned.intensiveSchedule[0].startTime.value(),
             std::u16string(u"09:05"));
    QCOMPARE(assigned.intensiveSchedule[0].endTime.value(),
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
    QCOMPARE(noProfile.regularSchedule[0].day.value(),
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
readsEveryClassAndScheduleRowBeyondTwentyFive()
{
    constexpr int classCount = 31;
    constexpr int regularScheduleRowCount = classCount + 1;
    constexpr int intensiveScheduleRowCount = 12;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("more-than-twenty-five-classes")
        )));

    const QSqlDatabase database = services.workspaceStore()->database();
    std::vector<qint64> classIdsByName(static_cast<std::size_t>(classCount));
    for (int index = classCount - 1; index >= 0; --index)
    {
        const qint64 id = 1000 + index;
        const QString name = QStringLiteral("Class %1").arg(
            index,
            2,
            10,
            QChar('0')
            );
        QVERIFY2(
            insertPersistedClass(database, id, name),
            qPrintable(QStringLiteral("Could not insert %1.").arg(name))
            );
        classIdsByName[static_cast<std::size_t>(index)] = id;
    }

    for (int index = 0; index < classCount; ++index)
    {
        const qint64 id = classIdsByName[static_cast<std::size_t>(index)];
        QVERIFY(insertSchedule(
            database,
            QStringLiteral("class_times"),
            id,
            QStringLiteral("Regular %1 first").arg(index),
            QStringLiteral("start %1").arg(index),
            QStringLiteral("end %1").arg(index)
            ));
        if (index == 0)
        {
            QVERIFY(insertSchedule(
                database,
                QStringLiteral("class_times"),
                id,
                QStringLiteral("Regular 0 second"),
                QStringLiteral("second start"),
                QStringLiteral("second end")
                ));
        }
        if (index < intensiveScheduleRowCount)
        {
            QVERIFY(insertSchedule(
                database,
                QStringLiteral("class_intensive_times"),
                id,
                QStringLiteral("Intensive %1").arg(index),
                QStringLiteral("intensive start %1").arg(index),
                QStringLiteral("intensive end %1").arg(index)
                ));
        }
    }

    Platform::ApplicationServicesScheduleBuilderSourcePort port(services);
    const auto result = port.readScheduleClasses({});
    QVERIFY2(result,
             result ? "" : qPrintable(QString::fromStdString(
                 result.error().message)));
    QCOMPARE(result.value().classes.size(), std::size_t(classCount));

    std::size_t observedRegularRows = 0;
    std::size_t observedIntensiveRows = 0;
    for (int index = 0; index < classCount; ++index)
    {
        const auto& entry = result.value().classes[static_cast<std::size_t>(index)];
        QCOMPARE(entry.classId.value(),
                 std::to_string(classIdsByName[static_cast<std::size_t>(index)]));
        QCOMPARE(entry.regularSchedule.size(), std::size_t(index == 0 ? 2 : 1));
        QCOMPARE(entry.intensiveSchedule.size(),
                 std::size_t(index < intensiveScheduleRowCount ? 1 : 0));
        QCOMPARE(entry.regularSchedule[0].day.value(),
                 QStringLiteral("Regular %1 first")
                     .arg(index)
                     .toStdU16String());
        if (index == 0)
        {
            QCOMPARE(entry.regularSchedule[1].day.value(),
                     std::u16string(u"Regular 0 second"));
        }
        if (index < intensiveScheduleRowCount)
        {
            QCOMPARE(entry.intensiveSchedule[0].day.value(),
                     QStringLiteral("Intensive %1")
                         .arg(index)
                         .toStdU16String());
        }
        observedRegularRows += entry.regularSchedule.size();
        observedIntensiveRows += entry.intensiveSchedule.size();
    }
    QCOMPARE(observedRegularRows, std::size_t(regularScheduleRowCount));
    QCOMPARE(observedIntensiveRows, std::size_t(intensiveScheduleRowCount));

    const ScheduleClassInfoReadMetrics& metrics = services
        .workspaceStore()
        ->classInfoRepository()
        ->scheduleClassInfoReadMetrics();
    QCOMPARE(metrics.completedClassCount, qint64(classCount));
    QCOMPARE(metrics.regularScheduleRowCount, qint64(regularScheduleRowCount));
    QCOMPARE(metrics.intensiveScheduleRowCount, qint64(intensiveScheduleRowCount));
}

void NextPlatformApplicationServicesScheduleBuilderSourcePortTests::
leavesResultMetricsUnchangedForEmptyAndFailedReads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("empty-schedule-source")
        )));
    Platform::ApplicationServicesScheduleBuilderSourcePort port(services);

    const auto emptyResult = port.readScheduleClasses({});
    QVERIFY2(emptyResult,
             emptyResult ? "" : qPrintable(QString::fromStdString(
                 emptyResult.error().message)));
    QVERIFY(emptyResult.value().classes.empty());

    const auto metricsAfterEmpty = services
        .workspaceStore()
        ->classInfoRepository()
        ->scheduleClassInfoReadMetrics();
    QCOMPARE(metricsAfterEmpty.completedClassCount, qint64(0));
    QCOMPARE(metricsAfterEmpty.regularScheduleRowCount, qint64(0));
    QCOMPARE(metricsAfterEmpty.intensiveScheduleRowCount, qint64(0));

    ApplicationServices failedServices;
    QVERIFY(failedServices.openDatabase(databasePath(
        directory,
        QStringLiteral("failed-intensive-schedule-source")
        )));
    const QSqlDatabase failedDatabase =
        failedServices.workspaceStore()->database();
    constexpr qint64 seededClassId = 8765;
    QVERIFY(insertPersistedClass(
        failedDatabase,
        seededClassId,
        QStringLiteral("Seeded class")
        ));
    QVERIFY(insertSchedule(
        failedDatabase,
        QStringLiteral("class_times"),
        seededClassId,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
        ));

    QSqlQuery dropIntensiveScheduleTable(failedDatabase);
    QVERIFY(dropIntensiveScheduleTable.exec(
        QStringLiteral("DROP TABLE class_intensive_times")
        ));

    Platform::ApplicationServicesScheduleBuilderSourcePort failedPort(
        failedServices
        );
    const auto failedResult = failedPort.readScheduleClasses({});
    QVERIFY(!failedResult);
    const auto metricsAfterFailure = failedServices
        .workspaceStore()
        ->classInfoRepository()
        ->scheduleClassInfoReadMetrics();
    QCOMPARE(metricsAfterFailure.metadataStatementCount, 1);
    QCOMPARE(metricsAfterFailure.regularScheduleStatementCount, 1);
    QCOMPARE(metricsAfterFailure.intensiveScheduleStatementCount, 1);
    QCOMPARE(metricsAfterFailure.completedClassCount, qint64(0));
    QCOMPARE(metricsAfterFailure.regularScheduleRowCount, qint64(0));
    QCOMPARE(metricsAfterFailure.intensiveScheduleRowCount, qint64(0));
}

void NextPlatformApplicationServicesScheduleBuilderSourcePortTests::
validatesStoredScheduleClassIdsBeforeNarrowing()
{
    constexpr qint64 ordinaryClassId = 1;
    constexpr qint64 maxIntClassId = std::numeric_limits<int>::max();
    constexpr qint64 wideClassId = 4294967297LL;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("wide-class-id")
        )));

    const QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(insertPersistedClass(
        database,
        ordinaryClassId,
        QStringLiteral("Alpha ordinary ID")
        ));
    QVERIFY(insertPersistedClass(
        database,
        maxIntClassId,
        QStringLiteral("Zulu maximum ID")
        ));

    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        ordinaryClassId,
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        maxIntClassId,
        QStringLiteral("Tuesday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
        ));

    const Application::ScheduleBuilderSourceQuery query;
    Platform::ApplicationServicesScheduleBuilderSourcePort port(services);
    const auto validPortResult = port.readScheduleClasses(query);
    QVERIFY2(validPortResult,
             validPortResult ? "" : qPrintable(QString::fromStdString(
                 validPortResult.error().message)));
    QCOMPARE(validPortResult.value().classes.size(), std::size_t(2));
    QCOMPARE(validPortResult.value().classes[0].classId.value(),
             std::string("1"));
    QCOMPARE(validPortResult.value().classes[1].classId.value(),
             std::to_string(maxIntClassId));
    QCOMPARE(validPortResult.value().classes[0].regularSchedule.size(),
             std::size_t(1));
    QCOMPARE(validPortResult.value().classes[1].regularSchedule.size(),
             std::size_t(1));
    QCOMPARE(validPortResult.value().classes[0].regularSchedule[0].day.value(),
             std::u16string(u"Monday"));
    QCOMPARE(validPortResult.value().classes[1].regularSchedule[0].day.value(),
             std::u16string(u"Tuesday"));

    const auto validHandlerResult =
        Application::ScheduleBuilderSourceQueryHandler::execute(query, port);
    QVERIFY2(validHandlerResult,
             validHandlerResult ? "" : qPrintable(QString::fromStdString(
                 validHandlerResult.error().message)));
    QCOMPARE(validHandlerResult.value().classes.size(), std::size_t(2));

    QVERIFY(insertPersistedClass(
        database,
        wideClassId,
        QStringLiteral("Middle wide ID")
        ));
    QVERIFY(insertSchedule(
        database,
        QStringLiteral("class_times"),
        wideClassId,
        QStringLiteral("Wednesday"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("11:50 AM")
        ));

    auto* const repository = services.workspaceStore()->classInfoRepository();
    const auto metricsBeforeInvalidId =
        repository->scheduleClassInfoReadMetrics();
    const auto invalidRepositoryResult = repository->loadScheduleClassInfos();
    QVERIFY(!invalidRepositoryResult);
    QVERIFY(invalidRepositoryResult.error().contains(
        QString::number(wideClassId)
        ));
    QCOMPARE(repository->scheduleClassInfoReadMetrics()
                 .regularScheduleStatementCount,
             metricsBeforeInvalidId.regularScheduleStatementCount);
    QCOMPARE(repository->scheduleClassInfoReadMetrics()
                 .intensiveScheduleStatementCount,
             metricsBeforeInvalidId.intensiveScheduleStatementCount);
    QCOMPARE(repository->scheduleClassInfoReadMetrics().completedClassCount,
             metricsBeforeInvalidId.completedClassCount);
    QCOMPARE(repository->scheduleClassInfoReadMetrics().regularScheduleRowCount,
             metricsBeforeInvalidId.regularScheduleRowCount);
    QCOMPARE(repository->scheduleClassInfoReadMetrics().intensiveScheduleRowCount,
             metricsBeforeInvalidId.intensiveScheduleRowCount);

    const auto invalidPortResult = port.readScheduleClasses(query);
    QVERIFY(!invalidPortResult);
    QCOMPARE(invalidPortResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!invalidPortResult.error().message.empty());
    const auto metricsAfterInvalidPort =
        repository->scheduleClassInfoReadMetrics();
    QCOMPARE(metricsAfterInvalidPort.regularScheduleStatementCount,
             metricsBeforeInvalidId.regularScheduleStatementCount);
    QCOMPARE(metricsAfterInvalidPort.intensiveScheduleStatementCount,
             metricsBeforeInvalidId.intensiveScheduleStatementCount);
    QCOMPARE(metricsAfterInvalidPort.completedClassCount,
             metricsBeforeInvalidId.completedClassCount);
    QCOMPARE(metricsAfterInvalidPort.regularScheduleRowCount,
             metricsBeforeInvalidId.regularScheduleRowCount);
    QCOMPARE(metricsAfterInvalidPort.intensiveScheduleRowCount,
             metricsBeforeInvalidId.intensiveScheduleRowCount);

    const auto invalidHandlerResult =
        Application::ScheduleBuilderSourceQueryHandler::execute(query, port);
    QVERIFY(!invalidHandlerResult);
    QCOMPARE(invalidHandlerResult.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(invalidHandlerResult.error().message,
             invalidPortResult.error().message);

    QSqlQuery verifyClasses(database);
    QVERIFY(verifyClasses.exec(QStringLiteral(
        "SELECT id, typeof(id) AS id_type, name "
        "FROM classes ORDER BY id"
        )));
    QVERIFY(verifyClasses.next());
    QCOMPARE(verifyClasses.value(QStringLiteral("id")).toLongLong(),
             ordinaryClassId);
    QCOMPARE(verifyClasses.value(QStringLiteral("id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifyClasses.value(QStringLiteral("name")).toString(),
             QStringLiteral("Alpha ordinary ID"));
    QVERIFY(verifyClasses.next());
    QCOMPARE(verifyClasses.value(QStringLiteral("id")).toLongLong(),
             maxIntClassId);
    QCOMPARE(verifyClasses.value(QStringLiteral("id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifyClasses.value(QStringLiteral("name")).toString(),
             QStringLiteral("Zulu maximum ID"));
    QVERIFY(verifyClasses.next());
    QCOMPARE(verifyClasses.value(QStringLiteral("id")).toLongLong(),
             wideClassId);
    QCOMPARE(verifyClasses.value(QStringLiteral("id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifyClasses.value(QStringLiteral("name")).toString(),
             QStringLiteral("Middle wide ID"));
    QVERIFY(!verifyClasses.next());

    QSqlQuery verifySchedules(database);
    QVERIFY(verifySchedules.exec(QStringLiteral(
        "SELECT class_id, typeof(class_id) AS class_id_type, day "
        "FROM class_times ORDER BY id"
        )));
    QVERIFY(verifySchedules.next());
    QCOMPARE(verifySchedules.value(QStringLiteral("class_id")).toLongLong(),
             ordinaryClassId);
    QCOMPARE(verifySchedules.value(QStringLiteral("class_id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifySchedules.value(QStringLiteral("day")).toString(),
             QStringLiteral("Monday"));
    QVERIFY(verifySchedules.next());
    QCOMPARE(verifySchedules.value(QStringLiteral("class_id")).toLongLong(),
             maxIntClassId);
    QCOMPARE(verifySchedules.value(QStringLiteral("class_id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifySchedules.value(QStringLiteral("day")).toString(),
             QStringLiteral("Tuesday"));
    QVERIFY(verifySchedules.next());
    QCOMPARE(verifySchedules.value(QStringLiteral("class_id")).toLongLong(),
             wideClassId);
    QCOMPARE(verifySchedules.value(QStringLiteral("class_id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifySchedules.value(QStringLiteral("day")).toString(),
             QStringLiteral("Wednesday"));
    QVERIFY(!verifySchedules.next());

    ApplicationServices zeroIdServices;
    QVERIFY(zeroIdServices.openDatabase(databasePath(
        directory,
        QStringLiteral("zero-class-id")
        )));
    const QSqlDatabase zeroIdDatabase =
        zeroIdServices.workspaceStore()->database();
    QVERIFY(insertPersistedClass(
        zeroIdDatabase,
        0,
        QStringLiteral("Zero class ID")
        ));
    QVERIFY(insertSchedule(
        zeroIdDatabase,
        QStringLiteral("class_times"),
        0,
        QStringLiteral("Thursday"),
        QStringLiteral("12:00 PM"),
        QStringLiteral("12:50 PM")
        ));

    auto* const zeroIdRepository =
        zeroIdServices.workspaceStore()->classInfoRepository();
    const auto zeroIdMetricsBeforeRead =
        zeroIdRepository->scheduleClassInfoReadMetrics();
    const auto zeroIdRepositoryResult =
        zeroIdRepository->loadScheduleClassInfos();
    QVERIFY(!zeroIdRepositoryResult);
    QVERIFY(zeroIdRepositoryResult.error().contains(QStringLiteral("0")));

    Platform::ApplicationServicesScheduleBuilderSourcePort zeroIdPort(
        zeroIdServices
        );
    const auto zeroIdPortResult = zeroIdPort.readScheduleClasses(query);
    QVERIFY(!zeroIdPortResult);
    QCOMPARE(zeroIdPortResult.error().code, Domain::ErrorCode::Technical);
    const auto zeroIdHandlerResult =
        Application::ScheduleBuilderSourceQueryHandler::execute(
            query,
            zeroIdPort
            );
    QVERIFY(!zeroIdHandlerResult);
    QCOMPARE(zeroIdHandlerResult.error().code, Domain::ErrorCode::Technical);
    const auto zeroIdMetricsAfterPort =
        zeroIdRepository->scheduleClassInfoReadMetrics();
    QCOMPARE(zeroIdMetricsAfterPort.regularScheduleStatementCount,
             zeroIdMetricsBeforeRead.regularScheduleStatementCount);
    QCOMPARE(zeroIdMetricsAfterPort.intensiveScheduleStatementCount,
             zeroIdMetricsBeforeRead.intensiveScheduleStatementCount);
    QCOMPARE(zeroIdMetricsAfterPort.completedClassCount, qint64(0));
    QCOMPARE(zeroIdMetricsAfterPort.regularScheduleRowCount, qint64(0));
    QCOMPARE(zeroIdMetricsAfterPort.intensiveScheduleRowCount, qint64(0));

    QSqlQuery verifyZeroIdClass(zeroIdDatabase);
    QVERIFY(verifyZeroIdClass.exec(QStringLiteral(
        "SELECT id, typeof(id) AS id_type, name FROM classes"
        )));
    QVERIFY(verifyZeroIdClass.next());
    QCOMPARE(verifyZeroIdClass.value(QStringLiteral("id")).toLongLong(), 0LL);
    QCOMPARE(verifyZeroIdClass.value(QStringLiteral("id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifyZeroIdClass.value(QStringLiteral("name")).toString(),
             QStringLiteral("Zero class ID"));
    QVERIFY(!verifyZeroIdClass.next());

    QSqlQuery verifyZeroIdSchedule(zeroIdDatabase);
    QVERIFY(verifyZeroIdSchedule.exec(QStringLiteral(
        "SELECT class_id, typeof(class_id) AS class_id_type, day "
        "FROM class_times"
        )));
    QVERIFY(verifyZeroIdSchedule.next());
    QCOMPARE(verifyZeroIdSchedule.value(QStringLiteral("class_id")).toLongLong(),
             0LL);
    QCOMPARE(verifyZeroIdSchedule.value(QStringLiteral("class_id_type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(verifyZeroIdSchedule.value(QStringLiteral("day")).toString(),
             QStringLiteral("Thursday"));
    QVERIFY(!verifyZeroIdSchedule.next());
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
        failedReadServices.workspaceStore()->database()
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
