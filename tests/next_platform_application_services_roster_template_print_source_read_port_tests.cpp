#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/roster_template_print_source_read_query.h"
#include "next/platform/application_services_roster_template_print_source_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("roster-template-print-source-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created ? *created : -1;
}

ClassInfo classInfo(
    const int classIdValue,
    const int teacherId,
    const QString& grade,
    const QString& level,
    const QList<ClassTime>& times
    )
{
    ClassInfo info;
    info.classId = classIdValue;
    info.teacherId = teacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.classTimes = times;
    return info;
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    QSqlQuery query(services.workspaceStore()->database());
    return query.exec(statement);
}

} // namespace

class NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void loadsFullClassInfoAndSparseRosterSnapshotsInRequestedOrder();
    void missingParentWithOrphanPrintDataReturnsNotFoundWithoutSnapshot();
    void emptyInputAndAllEmptyRostersSkipUnneededStatements();
    void repositoryBatchFailureIsTechnical();
    void rejectsOutOfRangeIntegerRosterWidthWithoutPartialSnapshot();
    void fractionalColumnIndexIsRejectedByTemplatePrintRead();
};

void NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests::
missingParentWithOrphanPrintDataReturnsNotFoundWithoutSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int deletedClass = createClass(
        services,
        QStringLiteral("Deleted class with orphan print data")
        );
    const int retainedClass = createClass(
        services,
        QStringLiteral("Retained class")
        );
    QVERIFY(deletedClass > 0);
    QVERIFY(retainedClass > 0);

    Roster retainedRoster;
    retainedRoster.columns = {QStringLiteral("Student")};
    retainedRoster.rows = {{QStringLiteral("Retained student")}};
    QVERIFY(services.workspaceStore()->rosterRepository()->saveRoster(
        retainedClass,
        retainedRoster
        ));

    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(
            deletedClass,
            -1,
            QStringLiteral("Orphan grade"),
            QStringLiteral("Orphan level"),
            {{QStringLiteral("Thursday"), QStringLiteral("6:00 PM"), QStringLiteral("6:50 PM")}}
            )
        ));
    Roster orphanRoster;
    orphanRoster.columns = {QStringLiteral("Student")};
    orphanRoster.rows = {{QStringLiteral("Orphan student")}};
    QVERIFY(services.workspaceStore()->rosterRepository()->saveRoster(
        deletedClass,
        orphanRoster
        ));

    QVERIFY(executeSql(
        services,
        QStringLiteral("PRAGMA foreign_keys=OFF")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DELETE FROM classes WHERE id=%1").arg(deletedClass)
        ));

    QSqlQuery orphanCounts(services.workspaceStore()->database());
    QVERIFY(orphanCounts.prepare(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM class_info WHERE class_id=?) AS class_info_count, "
        "(SELECT COUNT(*) FROM class_times WHERE class_id=?) AS class_times_count, "
        "(SELECT COUNT(*) FROM roster_columns WHERE class_id=?) AS columns_count, "
        "(SELECT COUNT(*) FROM roster_data WHERE class_id=?) AS data_count"
        )));
    orphanCounts.addBindValue(deletedClass);
    orphanCounts.addBindValue(deletedClass);
    orphanCounts.addBindValue(deletedClass);
    orphanCounts.addBindValue(deletedClass);
    QVERIFY2(orphanCounts.exec(), qPrintable(orphanCounts.lastError().text()));
    QVERIFY(orphanCounts.next());
    QCOMPARE(orphanCounts.value("class_info_count").toInt(), 1);
    QCOMPARE(orphanCounts.value("class_times_count").toInt(), 1);
    QCOMPARE(orphanCounts.value("columns_count").toInt(), 1);
    QCOMPARE(orphanCounts.value("data_count").toInt(), 1);

    const QList<int> requestedClassIds{retainedClass, deletedClass};
    const auto classInfoRecords = services.workspaceStore()
        ->classInfoRepository()->loadRosterPrintClassInfoRecords(
            requestedClassIds
            );
    QVERIFY(classInfoRecords);
    QCOMPARE(classInfoRecords->size(), 2);
    QCOMPARE(classInfoRecords->at(0).classId, retainedClass);
    QCOMPARE(classInfoRecords->at(1).classId, deletedClass);
    QVERIFY(!classInfoRecords->at(0).parentMissing);
    QVERIFY(classInfoRecords->at(1).parentMissing);
    QVERIFY(classInfoRecords->at(1).classGrade.isEmpty());
    QVERIFY(classInfoRecords->at(1).classLevel.isEmpty());
    QVERIFY(classInfoRecords->at(1).teacherEnglishName.isEmpty());
    QVERIFY(classInfoRecords->at(1).teacherKoreanName.isEmpty());
    QVERIFY(classInfoRecords->at(1).roomNumber.isEmpty());
    QVERIFY(classInfoRecords->at(1).wifiName.isEmpty());
    QVERIFY(classInfoRecords->at(1).wifiPassword.isEmpty());
    QVERIFY(classInfoRecords->at(1).zoomId.isEmpty());
    QVERIFY(classInfoRecords->at(1).zoomPassword.isEmpty());
    QVERIFY(classInfoRecords->at(1).regularTimes.isEmpty());

    const auto rosterRecords = services.workspaceStore()
        ->rosterRepository()->loadRostersForTemplatePrint(
            requestedClassIds
            );
    QVERIFY(rosterRecords);
    QCOMPARE(rosterRecords->size(), 2);
    QCOMPARE(rosterRecords->at(0).classId, retainedClass);
    QVERIFY(!rosterRecords->at(0).parentMissing);
    QCOMPARE(rosterRecords->at(0).roster.columns,
        QStringList{QStringLiteral("Student")});
    QCOMPARE(rosterRecords->at(0).roster.rows.size(), 1);
    QCOMPARE(rosterRecords->at(0).roster.rows[0],
        QStringList{QStringLiteral("Retained student")});
    QCOMPARE(rosterRecords->at(1).classId, deletedClass);
    QVERIFY(rosterRecords->at(1).parentMissing);
    QVERIFY(rosterRecords->at(1).roster.columns.isEmpty());
    QVERIFY(rosterRecords->at(1).roster.rows.isEmpty());

    Platform::ApplicationServicesRosterTemplatePrintSourceReadPort port(
        &services
        );
    const Application::RosterTemplatePrintSourceReadQuery query(port);
    const auto result = query.execute({
        classId(retainedClass),
        classId(deletedClass)
    });

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests::
loadsFullClassInfoAndSparseRosterSnapshotsInRequestedOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("Teacher Korean");
    teacher.teacherEn = QStringLiteral("Teacher English");
    teacher.roomNumber = QStringLiteral("Room 506");
    teacher.wifiName = QStringLiteral("Campus WiFi");
    teacher.wifiPassword = QStringLiteral("WiFi secret");
    teacher.zoomId = QStringLiteral("Zoom ID");
    teacher.zoomPassword = QStringLiteral("Zoom secret");
    const auto savedTeacher =
        services.workspaceStore()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(savedTeacher);

    const int firstClass = createClass(services, QStringLiteral("First"));
    const int missingInfoClass = createClass(services, QStringLiteral("Missing info"));
    const int emptyRosterClass = createClass(services, QStringLiteral("Empty roster"));
    QVERIFY(firstClass > 0);
    QVERIFY(missingInfoClass > 0);
    QVERIFY(emptyRosterClass > 0);

    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(
            firstClass,
            *savedTeacher,
            QStringLiteral("E4"),
            QStringLiteral("Odyssey"),
            {
                {QStringLiteral("Thursday"), QStringLiteral("3:00 PM"), QStringLiteral("3:50 PM")},
                {QStringLiteral("Tuesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}
            }
            )
        ));
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(
            missingInfoClass,
            *savedTeacher,
            QStringLiteral("E5"),
            QStringLiteral("Should be absent"),
            {{QStringLiteral("Friday"), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}}
            )
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DELETE FROM class_info WHERE class_id=%1")
            .arg(missingInfoClass)
        ));

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Notes")
    };
    roster.columnWidths = {121, 143, 177};
    roster.rows = {
        {QStringLiteral("Alex"), QStringLiteral("알렉스"), QStringLiteral("First")},
        {QString(), QString(), QString()},
        {QStringLiteral("Casey"), QString(), QStringLiteral("Third row")}
    };
    QVERIFY(services.workspaceStore()->rosterRepository()->saveRoster(
        firstClass,
        roster
        ));

    const auto classInfoMetricsBefore = services.workspaceStore()
        ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics();
    const auto rosterMetricsBefore = services.workspaceStore()
        ->rosterRepository()->templatePrintBatchReadMetrics();

    Platform::ApplicationServicesRosterTemplatePrintSourceReadPort port(
        &services
        );
    const Application::RosterTemplatePrintSourceReadQuery query(port);
    const auto result = query.execute({
        classId(missingInfoClass),
        classId(firstClass),
        classId(emptyRosterClass)
    });
    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(3));
    QCOMPARE(result.value()[0].classId, classId(missingInfoClass));
    QCOMPARE(result.value()[1].classId, classId(firstClass));
    QCOMPARE(result.value()[2].classId, classId(emptyRosterClass));

    const auto& missingInfo = result.value()[0].classInfo;
    QVERIFY(missingInfo.classGrade.empty());
    QVERIFY(missingInfo.classLevel.empty());
    QVERIFY(missingInfo.teacherEn.empty());
    QVERIFY(missingInfo.roomNumber.empty());
    QCOMPARE(missingInfo.regularSchedule.size(), std::size_t(1));
    QCOMPARE(missingInfo.regularSchedule[0].day, std::u16string(u"Friday"));

    const auto& firstInfo = result.value()[1].classInfo;
    QCOMPARE(firstInfo.classGrade, std::u16string(u"E4"));
    QCOMPARE(firstInfo.classLevel, std::u16string(u"Odyssey"));
    QCOMPARE(firstInfo.teacherEn, std::u16string(u"Teacher English"));
    QCOMPARE(firstInfo.teacherKr, std::u16string(u"Teacher Korean"));
    QCOMPARE(firstInfo.roomNumber, std::u16string(u"Room 506"));
    QCOMPARE(firstInfo.wifiName, std::u16string(u"Campus WiFi"));
    QCOMPARE(firstInfo.wifiPassword, std::u16string(u"WiFi secret"));
    QCOMPARE(firstInfo.zoomId, std::u16string(u"Zoom ID"));
    QCOMPARE(firstInfo.zoomPassword, std::u16string(u"Zoom secret"));
    QCOMPARE(firstInfo.regularSchedule.size(), std::size_t(2));
    QCOMPARE(firstInfo.regularSchedule[0].day, std::u16string(u"Thursday"));
    QCOMPARE(firstInfo.regularSchedule[1].day, std::u16string(u"Tuesday"));

    const auto& firstRoster = result.value()[1].roster;
    QCOMPARE(firstRoster.columns, (std::vector<std::u16string>{
        u"English", u"Korean", u"Notes"
    }));
    QCOMPARE(firstRoster.columnWidths, (std::vector<Application::RosterSnapshotColumnWidth>{
        Application::RosterSnapshotColumnWidth{121},
        Application::RosterSnapshotColumnWidth{143},
        Application::RosterSnapshotColumnWidth{177}
    }));
    QCOMPARE(firstRoster.rows.size(), std::size_t(3));
    QCOMPARE(firstRoster.rows[0], (std::vector<std::u16string>{
        u"Alex", u"알렉스", u"First"
    }));
    QCOMPARE(firstRoster.rows[1], (std::vector<std::u16string>{u"", u"", u""}));
    QCOMPARE(firstRoster.rows[2], (std::vector<std::u16string>{
        u"Casey", u"", u"Third row"
    }));
    QVERIFY(result.value()[2].roster.columns.empty());
    QVERIFY(result.value()[2].roster.columnWidths.empty());
    QVERIFY(result.value()[2].roster.rows.empty());

    const auto classInfoMetricsAfter = services.workspaceStore()
        ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics();
    QCOMPARE(classInfoMetricsAfter.callCount - classInfoMetricsBefore.callCount, 1);
    QCOMPARE(classInfoMetricsAfter.requestedClassCount - classInfoMetricsBefore.requestedClassCount, 3);
    QCOMPARE(classInfoMetricsAfter.metadataStatementCount - classInfoMetricsBefore.metadataStatementCount, 1);
    QCOMPARE(classInfoMetricsAfter.regularScheduleStatementCount - classInfoMetricsBefore.regularScheduleStatementCount, 1);
    QCOMPARE(
        services.workspaceStore()->classInfoRepository()
            ->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        0
        );

    const auto rosterMetricsAfter = services.workspaceStore()
        ->rosterRepository()->templatePrintBatchReadMetrics();
    QCOMPARE(rosterMetricsAfter.callCount - rosterMetricsBefore.callCount, 1);
    QCOMPARE(rosterMetricsAfter.requestedClassCount - rosterMetricsBefore.requestedClassCount, 3);
    QCOMPARE(rosterMetricsAfter.columnStatementCount - rosterMetricsBefore.columnStatementCount, 1);
    QCOMPARE(rosterMetricsAfter.cellStatementCount - rosterMetricsBefore.cellStatementCount, 1);
}

void NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests::
emptyInputAndAllEmptyRostersSkipUnneededStatements()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classIdValue = createClass(services, QStringLiteral("No roster"));
    QVERIFY(classIdValue > 0);

    Platform::ApplicationServicesRosterTemplatePrintSourceReadPort port(
        &services
        );
    const Application::RosterTemplatePrintSourceReadQuery query(port);
    const auto beforeClassInfo = services.workspaceStore()
        ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics();
    const auto beforeRoster = services.workspaceStore()
        ->rosterRepository()->templatePrintBatchReadMetrics();

    const auto empty = query.execute({});
    QVERIFY(empty);
    QVERIFY(empty.value().empty());
    QCOMPARE(services.workspaceStore()
                 ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics()
                 .callCount,
        beforeClassInfo.callCount);
    QCOMPARE(services.workspaceStore()
                 ->rosterRepository()->templatePrintBatchReadMetrics()
                 .callCount,
        beforeRoster.callCount);

    const auto noColumns = query.execute({classId(classIdValue)});
    QVERIFY(noColumns);
    QCOMPARE(noColumns.value().size(), std::size_t(1));
    QVERIFY(noColumns.value()[0].classInfo.classGrade.empty());
    QVERIFY(noColumns.value()[0].classInfo.classLevel.empty());
    QVERIFY(noColumns.value()[0].classInfo.regularSchedule.empty());
    QVERIFY(noColumns.value()[0].roster.columns.empty());
    const auto afterRoster = services.workspaceStore()
        ->rosterRepository()->templatePrintBatchReadMetrics();
    QCOMPARE(afterRoster.columnStatementCount - beforeRoster.columnStatementCount, 1);
    QCOMPARE(afterRoster.cellStatementCount - beforeRoster.cellStatementCount, 0);
}

void NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests::
repositoryBatchFailureIsTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classIdValue = createClass(services, QStringLiteral("Query failure"));
    QVERIFY(classIdValue > 0);

    Roster roster;
    roster.columns = {QStringLiteral("English")};
    QVERIFY(services.workspaceStore()->rosterRepository()->saveRoster(
        classIdValue,
        roster
        ));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE roster_data")));

    Platform::ApplicationServicesRosterTemplatePrintSourceReadPort port(
        &services
        );
    const Application::RosterTemplatePrintSourceReadQuery query(port);
    const auto result = query.execute({classId(classIdValue)});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests::
rejectsOutOfRangeIntegerRosterWidthWithoutPartialSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classIdValue = createClass(
        services,
        QStringLiteral("Out of range template print roster width")
        );
    QVERIFY(classIdValue > 0);

    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, 'Student', 0, ?)"
        )));
    insertColumn.addBindValue(classIdValue);
    insertColumn.addBindValue(QVariant::fromValue<qlonglong>(4294967297LL));
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery storedWidth(database);
    QVERIFY(storedWidth.prepare(QStringLiteral(
        "SELECT width, typeof(width) AS storage_type FROM roster_columns "
        "WHERE class_id=?"
        )));
    storedWidth.addBindValue(classIdValue);
    QVERIFY2(storedWidth.exec(), qPrintable(storedWidth.lastError().text()));
    QVERIFY(storedWidth.next());
    QCOMPARE(storedWidth.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedWidth.value("width").toLongLong(), qlonglong(4294967297LL));
    QVERIFY(!storedWidth.next());

    Platform::ApplicationServicesRosterTemplatePrintSourceReadPort port(
        &services
        );
    const Application::RosterTemplatePrintSourceReadQuery query(port);
    const auto result = query.execute({classId(classIdValue)});

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("class id %1").arg(classIdValue))
            && diagnostic.contains(QStringLiteral("width"), Qt::CaseInsensitive)
            && diagnostic.contains(QStringLiteral("4294967297")),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests::
fractionalColumnIndexIsRejectedByTemplatePrintRead()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classIdValue = createClass(services, QStringLiteral("Fractional index"));
    QVERIFY(classIdValue > 0);

    Roster roster;
    roster.columns = {QStringLiteral("English")};
    roster.rows = {{QStringLiteral("Integer column zero")}};
    QVERIFY(services.workspaceStore()->rosterRepository()->saveRoster(
        classIdValue,
        roster
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (%1, 0, 0.25, 'Fractional column zero')"
            ).arg(classIdValue)
        ));

    Platform::ApplicationServicesRosterTemplatePrintSourceReadPort port(
        &services
        );
    const Application::RosterTemplatePrintSourceReadQuery query(port);
    const auto result = query.execute({classId(classIdValue)});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    QSqlQuery storedCells(services.workspaceStore()->database());
    QVERIFY(storedCells.prepare(QStringLiteral(
        "SELECT col_index, typeof(col_index) AS storage_type, value "
        "FROM roster_data WHERE class_id=? AND row_index=0 "
        "ORDER BY col_index"
        )));
    storedCells.addBindValue(classIdValue);
    QVERIFY(storedCells.exec());
    QVERIFY(storedCells.next());
    QCOMPARE(storedCells.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedCells.value("col_index").toLongLong(), qlonglong(0));
    QCOMPARE(storedCells.value("value").toString(), QStringLiteral("Integer column zero"));
    QVERIFY(storedCells.next());
    QCOMPARE(storedCells.value("storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(storedCells.value("col_index").toDouble(), 0.25);
    QCOMPARE(storedCells.value("value").toString(), QStringLiteral("Fractional column zero"));
    QVERIFY(!storedCells.next());
}

QTEST_MAIN(NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests)

#include "next_platform_application_services_roster_template_print_source_read_port_tests.moc"
