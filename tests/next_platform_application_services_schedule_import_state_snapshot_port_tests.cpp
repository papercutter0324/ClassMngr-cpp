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
    const std::optional<qint64> teacherIdValue,
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
    void preservesLargeWorkspaceRecordsAndReadCounts();
    void preservesWideTeacherIdsAndLowIdAssociations();
    void rejectsPartialTeacherCursorRead();
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
    const int missingInfo = services.classService()->create(
        QStringLiteral("Missing metadata")
        ).value_or(-1);
    QVERIFY(firstDuplicate > 0);
    QVERIFY(secondDuplicate > 0);
    QVERIFY(unassigned > 0);
    QVERIFY(testingOnly > 0);
    QVERIFY(missingInfo > 0);

    const QSqlDatabase database = services.workspaceStore()->database();
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
        services.workspaceStore()->classRepository()->getClasses();
    const Result<QList<Teacher>> teacherOrder =
        services.workspaceStore()->teacherRepository()->getAllTeachers();
    QVERIFY(legacyOrder);
    QVERIFY(teacherOrder);

    TeacherRepository* const teacherRepository =
        services.workspaceStore()->teacherRepository();
    const ScheduleImportTeacherReadMetrics teacherMetricsBefore =
        teacherRepository->scheduleImportTeacherReadMetrics();
    ClassInfoRepository* const classInfoRepository =
        services.workspaceStore()->classInfoRepository();
    const ScheduleImportStateSnapshotClassInfoReadMetrics
        snapshotClassInfoMetricsBefore = classInfoRepository
            ->scheduleImportStateSnapshotClassInfoReadMetrics();
    const ScheduleClassInfoReadMetrics genericClassInfoMetricsBefore =
        classInfoRepository->scheduleClassInfoReadMetrics();
    QSqlQuery renameUnusedTeacherField(database);
    QVERIFY2(
        renameUnusedTeacherField.exec(QStringLiteral(
            "ALTER TABLE teachers RENAME COLUMN notes TO unused_profile_notes")),
        qPrintable(renameUnusedTeacherField.lastError().text())
        );
    QSqlQuery renameUnusedClassInfoField(database);
    QVERIFY2(
        renameUnusedClassInfoField.exec(QStringLiteral(
            "ALTER TABLE class_info RENAME COLUMN font_color TO unused_font_color")),
        qPrintable(renameUnusedClassInfoField.lastError().text())
        );
    QSqlQuery renameUnusedTeacherPreferredName(database);
    QVERIFY2(
        renameUnusedTeacherPreferredName.exec(QStringLiteral(
            "ALTER TABLE teachers RENAME COLUMN preferred_name "
            "TO unused_teacher_preferred_name")),
        qPrintable(renameUnusedTeacherPreferredName.lastError().text())
        );

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
        const Teacher& expectedTeacher =
            (*teacherOrder)[static_cast<qsizetype>(index)];
        QCOMPARE(
            snapshot->teachers[index].id.value(),
            std::to_string(expectedTeacher.id)
            );
        QCOMPARE(
            snapshot->teachers[index].koreanName,
            expectedTeacher.teacherKr.toStdU16String()
            );
        QCOMPARE(
            snapshot->teachers[index].roomNumber,
            expectedTeacher.roomNumber.toStdU16String()
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
    QCOMPARE(assigned->level, std::u16string(u"Hercules"));
    QCOMPARE(assigned->classColor, std::u16string(u"#123456"));
    QCOMPARE(assigned->roomNumber, std::u16string(u" Room 9 "));
    QCOMPARE(assigned->normalTimes.size(), std::size_t(2));
    QCOMPARE(assigned->normalTimes[0].day, std::u16string(u" Raw Monday "));
    QCOMPARE(assigned->normalTimes[0].startTime, std::u16string(u"bad start"));
    QCOMPARE(assigned->normalTimes[0].endTime, std::u16string(u"opaque end"));
    QCOMPARE(assigned->normalTimes[1].day, std::u16string(u"Thursday"));
    QCOMPARE(assigned->normalTimes[1].startTime, std::u16string(u"5:00 PM"));
    QCOMPARE(assigned->normalTimes[1].endTime, std::u16string(u"5:50 PM"));
    QCOMPARE(assigned->intensiveTimes.size(), std::size_t(1));
    QCOMPARE(assigned->intensiveTimes[0].day, std::u16string(u"Friday"));
    QCOMPARE(assigned->intensiveTimes[0].startTime, std::u16string(u"9:00 AM"));
    QCOMPARE(assigned->intensiveTimes[0].endTime, std::u16string(u"9:50 AM"));

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

    const auto missingInfoSnapshot = std::find_if(
        snapshot->classes.begin(),
        snapshot->classes.end(),
        [missingInfo](const auto& value)
        {
            return value.id.value() == std::to_string(missingInfo);
        }
        );
    QVERIFY(missingInfoSnapshot != snapshot->classes.end());
    QVERIFY(missingInfoSnapshot->teacherId == teacherId(-1));
    QCOMPARE(missingInfoSnapshot->className, std::u16string(u"Missing metadata"));
    QVERIFY(missingInfoSnapshot->grade.empty());
    QVERIFY(missingInfoSnapshot->level.empty());
    QCOMPARE(missingInfoSnapshot->classColor, std::u16string(u"#FFFFFF"));
    QVERIFY(missingInfoSnapshot->roomNumber.empty());
    QVERIFY(missingInfoSnapshot->normalTimes.empty());
    QVERIFY(missingInfoSnapshot->intensiveTimes.empty());

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

    const auto assignedTeacher = std::find_if(
        snapshot->teachers.begin(),
        snapshot->teachers.end(),
        [teacherIdValue](const auto& value)
        {
            return value.id.value() == std::to_string(teacherIdValue);
        }
        );
    QVERIFY(assignedTeacher != snapshot->teachers.end());
    QCOMPARE(
        assignedTeacher->koreanName,
        std::u16string(u"  \uAE40 \uC120\uC0DD\uB2D8  ")
        );
    QCOMPARE(assignedTeacher->roomNumber, std::u16string(u" Room 9 "));

    const ScheduleImportTeacherReadMetrics teacherMetricsAfter =
        teacherRepository->scheduleImportTeacherReadMetrics();
    QCOMPARE(
        teacherMetricsAfter.callCount - teacherMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        teacherMetricsAfter.statementCount
            - teacherMetricsBefore.statementCount,
        1
        );

    const ScheduleImportStateSnapshotClassInfoReadMetrics
        snapshotClassInfoMetricsAfter = classInfoRepository
            ->scheduleImportStateSnapshotClassInfoReadMetrics();
    QCOMPARE(
        snapshotClassInfoMetricsAfter.callCount
            - snapshotClassInfoMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        snapshotClassInfoMetricsAfter.metadataStatementCount
            - snapshotClassInfoMetricsBefore.metadataStatementCount,
        1
        );
    QCOMPARE(
        snapshotClassInfoMetricsAfter.regularScheduleStatementCount
            - snapshotClassInfoMetricsBefore.regularScheduleStatementCount,
        1
        );
    QCOMPARE(
        snapshotClassInfoMetricsAfter.intensiveScheduleStatementCount
            - snapshotClassInfoMetricsBefore.intensiveScheduleStatementCount,
        1
        );

    const ScheduleClassInfoReadMetrics genericClassInfoMetricsAfter =
        classInfoRepository->scheduleClassInfoReadMetrics();
    QCOMPARE(
        genericClassInfoMetricsAfter.scheduleClassInfosCallCount
            - genericClassInfoMetricsBefore.scheduleClassInfosCallCount,
        0
        );
    QCOMPARE(
        genericClassInfoMetricsAfter.metadataStatementCount
            - genericClassInfoMetricsBefore.metadataStatementCount,
        0
        );
    QCOMPARE(
        genericClassInfoMetricsAfter.regularScheduleStatementCount
            - genericClassInfoMetricsBefore.regularScheduleStatementCount,
        0
        );
    QCOMPARE(
        genericClassInfoMetricsAfter.intensiveScheduleStatementCount
            - genericClassInfoMetricsBefore.intensiveScheduleStatementCount,
        0
        );
}

void NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests::
preservesLargeWorkspaceRecordsAndReadCounts()
{
    constexpr int teacherCount = 24;
    constexpr int classCount = 96;
    constexpr int regularTimesPerClass = 8;
    constexpr int expectedRegularTimeCount = 768;
    constexpr int expectedIntensiveTimeCount = 24;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        databasePath(directory, QStringLiteral("large-workspace"))
        ));
    const QSqlDatabase database = services.workspaceStore()->database();

    std::vector<int> teacherIds(static_cast<std::size_t>(teacherCount), -1);
    for (int index = teacherCount - 1; index >= 0; --index)
    {
        const QString englishName = QStringLiteral("Teacher %1")
            .arg(index, 3, 10, QLatin1Char('0'));
        const QString koreanName = QStringLiteral("Korean teacher %1")
            .arg(index, 3, 10, QLatin1Char('0'));
        const QString room = QStringLiteral("Room %1")
            .arg(index, 3, 10, QLatin1Char('0'));
        QVERIFY2(
            insertTeacher(
                database,
                koreanName,
                englishName,
                room,
                &teacherIds[static_cast<std::size_t>(index)]
                ),
            "Could not insert a large-workspace teacher."
            );
    }

    std::vector<int> classIds(static_cast<std::size_t>(classCount), -1);
    for (int classIndex = classCount - 1; classIndex >= 0; --classIndex)
    {
        const QString className = QStringLiteral("Class %1")
            .arg(classIndex, 3, 10, QLatin1Char('0'));
        const int classIdValue = services.classService()->create(className)
            .value_or(-1);
        QVERIFY(classIdValue > 0);
        classIds[static_cast<std::size_t>(classIndex)] = classIdValue;

        const int assignedTeacherIndex = (classIndex * 7 + 3) % teacherCount;
        const int assignedTeacherId =
            teacherIds[static_cast<std::size_t>(assignedTeacherIndex)];
        const QString grade = QStringLiteral("Grade %1")
            .arg(classIndex % 12, 2, 10, QLatin1Char('0'));
        const QString level = QStringLiteral("Level %1")
            .arg(classIndex % 9, 2, 10, QLatin1Char('0'));
        const QString color = QStringLiteral("Class color %1")
            .arg(classIndex, 3, 10, QLatin1Char('0'));
        QVERIFY(insertClassInfo(
            database,
            classIdValue,
            assignedTeacherId,
            grade,
            level,
            color
            ));

        for (int timeIndex = 0; timeIndex < regularTimesPerClass; ++timeIndex)
        {
            QVERIFY(insertSchedule(
                database,
                QStringLiteral("class_times"),
                classIdValue,
                QStringLiteral("Day %1").arg(timeIndex % 7),
                QStringLiteral("Start %1-%2")
                    .arg(classIndex, 3, 10, QLatin1Char('0'))
                    .arg(timeIndex),
                QStringLiteral("End %1-%2")
                    .arg(classIndex, 3, 10, QLatin1Char('0'))
                    .arg(timeIndex)
                ));
        }

        if (classIndex % 4 == 0)
        {
            QVERIFY(insertSchedule(
                database,
                QStringLiteral("class_intensive_times"),
                classIdValue,
                QStringLiteral("Intensive day %1").arg(classIndex),
                QStringLiteral("Intensive start %1").arg(classIndex),
                QStringLiteral("Intensive end %1").arg(classIndex)
                ));
        }
    }

    ClassRepository* const classRepository =
        services.workspaceStore()->classRepository();
    TeacherRepository* const teacherRepository =
        services.workspaceStore()->teacherRepository();
    ClassInfoRepository* const classInfoRepository =
        services.workspaceStore()->classInfoRepository();
    const Result<QList<Classroom>> expectedClasses =
        classRepository->getClasses();
    const Result<QList<ScheduleImportTeacherReadRecord>> expectedTeachers =
        teacherRepository->loadScheduleImportTeacherRecords();
    const Result<QList<ScheduleImportStateSnapshotClassInfoReadRecord>>
        expectedClassInfos =
            classInfoRepository->loadScheduleImportStateSnapshotClassInfos();
    QVERIFY(expectedClasses);
    QVERIFY(expectedTeachers);
    QVERIFY(expectedClassInfos);
    QCOMPARE(expectedTeachers->size(), teacherCount);
    QCOMPARE(expectedClasses->size(), classCount);
    QCOMPARE(expectedClassInfos->size(), classCount);
    for (int index = 0; index < teacherCount; ++index)
    {
        QCOMPARE(
            expectedTeachers->at(index).teacherId,
            teacherIds[static_cast<std::size_t>(index)]
            );
    }

    int sourceRegularTimeCount = 0;
    int sourceIntensiveTimeCount = 0;
    for (int index = 0; index < classCount; ++index)
    {
        const ScheduleImportStateSnapshotClassInfoReadRecord& info =
            expectedClassInfos->at(index);
        QCOMPARE(
            info.classId,
            classIds[static_cast<std::size_t>(index)]
            );
        sourceRegularTimeCount += info.regularTimes.size();
        sourceIntensiveTimeCount += info.intensiveTimes.size();
    }
    QCOMPARE(sourceRegularTimeCount, expectedRegularTimeCount);
    QCOMPARE(sourceIntensiveTimeCount, expectedIntensiveTimeCount);

    const ScheduleImportTeacherReadMetrics teacherMetricsBefore =
        teacherRepository->scheduleImportTeacherReadMetrics();
    const ScheduleImportStateSnapshotClassInfoReadMetrics classInfoMetricsBefore =
        classInfoRepository->scheduleImportStateSnapshotClassInfoReadMetrics();

    Platform::ApplicationServicesScheduleImportStateSnapshotPort port(services);
    const auto result = Application::ScheduleImportStateSnapshotQueryHandler::
        execute({}, port);
    const auto* snapshot =
        std::get_if<Application::ScheduleImportStateSnapshot>(&result);
    QVERIFY2(snapshot, failure(result)
        ? qPrintable(QString::fromStdString(failure(result)->message))
        : "Expected a complete large-workspace snapshot.");

    QCOMPARE(snapshot->teachers.size(), static_cast<std::size_t>(teacherCount));
    for (int index = 0; index < teacherCount; ++index)
    {
        const ScheduleImportTeacherReadRecord& expected =
            expectedTeachers->at(index);
        const auto& actual = snapshot->teachers[static_cast<std::size_t>(index)];
        QCOMPARE(actual.id.value(), std::to_string(expected.teacherId));
        QCOMPARE(actual.koreanName, expected.teacherKr.toStdU16String());
        QCOMPARE(actual.roomNumber, expected.roomNumber.toStdU16String());
    }

    QCOMPARE(snapshot->classes.size(), static_cast<std::size_t>(classCount));
    int snapshotRegularTimeCount = 0;
    int snapshotIntensiveTimeCount = 0;
    for (int index = 0; index < classCount; ++index)
    {
        const Classroom& expectedClass = expectedClasses->at(index);
        const ScheduleImportStateSnapshotClassInfoReadRecord& expectedInfo =
            expectedClassInfos->at(index);
        const auto& actual = snapshot->classes[static_cast<std::size_t>(index)];
        QCOMPARE(actual.id.value(), std::to_string(expectedClass.id));
        QCOMPARE(actual.teacherId.value(), std::to_string(expectedInfo.teacherId));
        QCOMPARE(actual.className, expectedClass.name.toStdU16String());
        QCOMPARE(actual.grade, expectedInfo.classGrade.toStdU16String());
        QCOMPARE(actual.level, expectedInfo.classLevel.toStdU16String());
        QCOMPARE(actual.classColor, expectedInfo.classColor.toStdU16String());
        QCOMPARE(actual.roomNumber, expectedInfo.roomNumber.toStdU16String());
        QCOMPARE(
            actual.normalTimes.size(),
            static_cast<std::size_t>(expectedInfo.regularTimes.size())
            );
        for (int timeIndex = 0;
             timeIndex < expectedInfo.regularTimes.size();
             ++timeIndex)
        {
            const ClassTime& expectedTime =
                expectedInfo.regularTimes.at(timeIndex);
            const auto& actualTime =
                actual.normalTimes[static_cast<std::size_t>(timeIndex)];
            QCOMPARE(actualTime.day, expectedTime.day.toStdU16String());
            QCOMPARE(
                actualTime.startTime,
                expectedTime.startTime.toStdU16String()
                );
            QCOMPARE(actualTime.endTime, expectedTime.endTime.toStdU16String());
        }
        QCOMPARE(
            actual.intensiveTimes.size(),
            static_cast<std::size_t>(expectedInfo.intensiveTimes.size())
            );
        for (int timeIndex = 0;
             timeIndex < expectedInfo.intensiveTimes.size();
             ++timeIndex)
        {
            const ClassTime& expectedTime =
                expectedInfo.intensiveTimes.at(timeIndex);
            const auto& actualTime =
                actual.intensiveTimes[static_cast<std::size_t>(timeIndex)];
            QCOMPARE(actualTime.day, expectedTime.day.toStdU16String());
            QCOMPARE(
                actualTime.startTime,
                expectedTime.startTime.toStdU16String()
                );
            QCOMPARE(actualTime.endTime, expectedTime.endTime.toStdU16String());
        }
        snapshotRegularTimeCount += static_cast<int>(actual.normalTimes.size());
        snapshotIntensiveTimeCount +=
            static_cast<int>(actual.intensiveTimes.size());
    }
    QCOMPARE(snapshotRegularTimeCount, expectedRegularTimeCount);
    QCOMPARE(snapshotIntensiveTimeCount, expectedIntensiveTimeCount);
    QCOMPARE(
        snapshotRegularTimeCount + snapshotIntensiveTimeCount,
        792
        );

    const ScheduleImportTeacherReadMetrics teacherMetricsAfter =
        teacherRepository->scheduleImportTeacherReadMetrics();
    QCOMPARE(
        teacherMetricsAfter.callCount - teacherMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        teacherMetricsAfter.statementCount - teacherMetricsBefore.statementCount,
        1
        );
    const ScheduleImportStateSnapshotClassInfoReadMetrics classInfoMetricsAfter =
        classInfoRepository->scheduleImportStateSnapshotClassInfoReadMetrics();
    QCOMPARE(
        classInfoMetricsAfter.callCount - classInfoMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        classInfoMetricsAfter.metadataStatementCount
            - classInfoMetricsBefore.metadataStatementCount,
        1
        );
    QCOMPARE(
        classInfoMetricsAfter.regularScheduleStatementCount
            - classInfoMetricsBefore.regularScheduleStatementCount,
        1
        );
    QCOMPARE(
        classInfoMetricsAfter.intensiveScheduleStatementCount
            - classInfoMetricsBefore.intensiveScheduleStatementCount,
        1
        );
}

void NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests::
preservesWideTeacherIdsAndLowIdAssociations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        databasePath(directory, QStringLiteral("wide-teacher"))
        ));
    const int lowClass = services.classService()->create(
        QStringLiteral("Low teacher class")
        ).value_or(-1);
    const int wideClass = services.classService()->create(
        QStringLiteral("Wide teacher class")
        ).value_or(-1);
    QVERIFY(lowClass > 0);
    QVERIFY(wideClass > 0);

    constexpr qint64 wideTeacherIdValue = 2147483648LL;
    const QSqlDatabase database = services.workspaceStore()->database();
    int lowTeacherId = -1;
    QVERIFY(insertTeacher(
        database,
        QStringLiteral("Same teacher"),
        QStringLiteral("Low teacher"),
        QStringLiteral("Low room"),
        &lowTeacherId
        ));
    QCOMPARE(lowTeacherId, 1);

    QSqlQuery wideTeacherInsert(database);
    wideTeacherInsert.prepare(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en, room_number) "
        "VALUES (?, ?, ?, ?)"
        ));
    wideTeacherInsert.addBindValue(wideTeacherIdValue);
    wideTeacherInsert.addBindValue(QStringLiteral("Same teacher"));
    wideTeacherInsert.addBindValue(QStringLiteral("Wide teacher"));
    wideTeacherInsert.addBindValue(QStringLiteral("Wide room"));
    QVERIFY2(
        wideTeacherInsert.exec(),
        qPrintable(wideTeacherInsert.lastError().text())
        );

    QVERIFY(insertClassInfo(
        database,
        lowClass,
        lowTeacherId,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("#123456")
        ));
    QVERIFY(insertClassInfo(
        database,
        wideClass,
        wideTeacherIdValue,
        QStringLiteral("E5"),
        QStringLiteral("Athena"),
        QStringLiteral("#654321")
        ));

    const auto teacherReadRecords = services.workspaceStore()
        ->teacherRepository()->loadScheduleImportTeacherRecords();
    QVERIFY(teacherReadRecords.has_value());
    QCOMPARE(teacherReadRecords->size(), 2);
    QCOMPARE(teacherReadRecords->at(0).teacherId, lowTeacherId);
    QCOMPARE(teacherReadRecords->at(1).teacherId, wideTeacherIdValue);

    Platform::ApplicationServicesScheduleImportStateSnapshotPort port(services);
    const auto result = Application::ScheduleImportStateSnapshotQueryHandler::
        execute({}, port);
    const auto* snapshot =
        std::get_if<Application::ScheduleImportStateSnapshot>(&result);
    QVERIFY2(snapshot, failure(result)
        ? qPrintable(QString::fromStdString(failure(result)->message))
        : "Expected a complete state snapshot.");

    const auto lowTeacher = std::find_if(
        snapshot->teachers.begin(),
        snapshot->teachers.end(),
        [lowTeacherId](const auto& teacher)
        {
            return teacher.id.value() == std::to_string(lowTeacherId);
        }
        );
    const auto wideTeacher = std::find_if(
        snapshot->teachers.begin(),
        snapshot->teachers.end(),
        [](const auto& teacher)
        {
            return teacher.id.value() == "2147483648";
        }
        );
    QVERIFY(lowTeacher != snapshot->teachers.end());
    QVERIFY(wideTeacher != snapshot->teachers.end());
    QCOMPARE(lowTeacher->id.value(), std::string("1"));
    QCOMPARE(wideTeacher->id.value(), std::string("2147483648"));

    const auto lowAssignment = std::find_if(
        snapshot->classes.begin(),
        snapshot->classes.end(),
        [lowClass](const auto& classroom)
        {
            return classroom.id.value() == std::to_string(lowClass);
        }
        );
    const auto wideAssignment = std::find_if(
        snapshot->classes.begin(),
        snapshot->classes.end(),
        [wideClass](const auto& classroom)
        {
            return classroom.id.value() == std::to_string(wideClass);
        }
        );
    QVERIFY(lowAssignment != snapshot->classes.end());
    QVERIFY(wideAssignment != snapshot->classes.end());
    QCOMPARE(lowAssignment->teacherId.value(), std::string("1"));
    QCOMPARE(wideAssignment->teacherId.value(), std::string("2147483648"));
}

void NextPlatformApplicationServicesScheduleImportStateSnapshotPortTests::
rejectsPartialTeacherCursorRead()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        databasePath(directory, QStringLiteral("partial-teacher-read"))
        ));
    const QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery foreignKeysOff(database);
    QVERIFY2(
        foreignKeysOff.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(foreignKeysOff.lastError().text())
        );
    QVERIFY2(
        dropTable(database, QStringLiteral("teachers")),
        "Could not replace the teachers table with the indexed test view."
        );

    QSqlQuery payloadTable(database);
    QVERIFY2(
        payloadTable.exec(QStringLiteral(R"(
            CREATE TABLE schedule_import_teacher_payloads (
                id INTEGER PRIMARY KEY,
                teacher_en TEXT NOT NULL,
                teacher_kr TEXT NOT NULL,
                payload TEXT NOT NULL
            )
        )")),
        qPrintable(payloadTable.lastError().text())
        );
    QSqlQuery payloadIndex(database);
    QVERIFY2(
        payloadIndex.exec(QStringLiteral(R"(
            CREATE INDEX schedule_import_teacher_payloads_teacher_en_idx
            ON schedule_import_teacher_payloads (teacher_en)
        )")),
        qPrintable(payloadIndex.lastError().text())
        );
    QSqlQuery teacherView(database);
    QVERIFY2(
        teacherView.exec(QStringLiteral(R"(
            CREATE VIEW teachers AS
            SELECT id, teacher_en, teacher_kr,
                   json_extract(payload, '$.room') AS room_number
            FROM schedule_import_teacher_payloads
        )")),
        qPrintable(teacherView.lastError().text())
        );

    const auto insertPayload = [&](const qint64 id,
                                   const QString& englishName,
                                   const QString& koreanName,
                                   const QString& payload)
    {
        QSqlQuery insert(database);
        insert.prepare(QStringLiteral(R"(
            INSERT INTO schedule_import_teacher_payloads
                (id, teacher_en, teacher_kr, payload)
            VALUES (?, ?, ?, ?)
        )"));
        insert.addBindValue(id);
        insert.addBindValue(englishName);
        insert.addBindValue(koreanName);
        insert.addBindValue(payload);
        return insert.exec();
    };
    QVERIFY(insertPayload(
        1,
        QStringLiteral("A Valid"),
        QStringLiteral("Valid Korean"),
        QStringLiteral(R"({"room":"Room 1"})")
        ));
    QVERIFY(insertPayload(
        2,
        QStringLiteral("B Broken"),
        QStringLiteral("Broken Korean"),
        QStringLiteral("{broken-json")
        ));

    QSqlQuery plan(database);
    QVERIFY2(
        plan.exec(QStringLiteral(R"(
            EXPLAIN QUERY PLAN
            SELECT id, teacher_kr, room_number
            FROM teachers
            ORDER BY teacher_en
        )")),
        qPrintable(plan.lastError().text())
        );
    bool usesTeacherEnglishIndex = false;
    while (plan.next())
    {
        usesTeacherEnglishIndex = usesTeacherEnglishIndex
            || plan.value(3).toString().contains(
                QStringLiteral(
                    "schedule_import_teacher_payloads_teacher_en_idx"
                    ),
                Qt::CaseInsensitive
                );
    }
    QVERIFY(usesTeacherEnglishIndex);

    QSqlQuery rawQuery(database);
    QVERIFY2(
        rawQuery.exec(QStringLiteral(
            "SELECT id, teacher_kr, room_number FROM teachers "
            "ORDER BY teacher_en"
            )),
        qPrintable(rawQuery.lastError().text())
        );
    QVERIFY(rawQuery.next());
    QCOMPARE(rawQuery.value(QStringLiteral("id")).toLongLong(), qint64(1));
    QCOMPARE(
        rawQuery.value(QStringLiteral("teacher_kr")).toString(),
        QStringLiteral("Valid Korean")
        );
    QCOMPARE(
        rawQuery.value(QStringLiteral("room_number")).toString(),
        QStringLiteral("Room 1")
        );
    QVERIFY(!rawQuery.next());
    QVERIFY(rawQuery.lastError().isValid());
    QVERIFY(rawQuery.lastError().text().contains(
        QStringLiteral("malformed JSON"),
        Qt::CaseInsensitive
        ));

    const auto teacherRecords = services.workspaceStore()
        ->teacherRepository()->loadScheduleImportTeacherRecords();
    QVERIFY(!teacherRecords.has_value());

    Platform::ApplicationServicesScheduleImportStateSnapshotPort port(services);
    const Application::ScheduleImportStateSnapshotQuery query;
    const auto result = Application::ScheduleImportStateSnapshotQueryHandler::
        execute(query, port);
    const auto* readFailure = failure(result);
    QVERIFY(readFailure);
    QCOMPARE(
        readFailure->kind,
        Application::ScheduleImportStateSnapshotFailureKind::
            RepositoryReadFailed
        );
    QCOMPARE(
        readFailure->source,
        Application::ScheduleImportStateSnapshotFailureSource::Teachers
        );
    QVERIFY(!std::get_if<Application::ScheduleImportStateSnapshot>(&result));
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
            services.workspaceStore()->database()
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
        if (!dropTable(services.workspaceStore()->database(), table))
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
