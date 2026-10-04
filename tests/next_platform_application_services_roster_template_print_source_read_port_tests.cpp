#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/roster_template_print_source_read_query.h"
#include "next/platform/application_services_roster_template_print_source_read_port.h"

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
    QSqlQuery query(services.databaseSession()->database());
    return query.exec(statement);
}

} // namespace

class NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void loadsFullClassInfoAndSparseRosterSnapshotsInRequestedOrder();
    void emptyInputAndAllEmptyRostersSkipUnneededStatements();
    void repositoryBatchFailureIsTechnical();
};

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
        services.databaseSession()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(savedTeacher);

    const int firstClass = createClass(services, QStringLiteral("First"));
    const int missingInfoClass = createClass(services, QStringLiteral("Missing info"));
    const int emptyRosterClass = createClass(services, QStringLiteral("Empty roster"));
    QVERIFY(firstClass > 0);
    QVERIFY(missingInfoClass > 0);
    QVERIFY(emptyRosterClass > 0);

    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
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
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
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
    QVERIFY(services.databaseSession()->rosterRepository()->saveRoster(
        firstClass,
        roster
        ));

    const auto classInfoMetricsBefore = services.databaseSession()
        ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics();
    const auto rosterMetricsBefore = services.databaseSession()
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
    QCOMPARE(firstRoster.columnWidths, (std::vector<int>{121, 143, 177}));
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

    const auto classInfoMetricsAfter = services.databaseSession()
        ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics();
    QCOMPARE(classInfoMetricsAfter.callCount - classInfoMetricsBefore.callCount, 1);
    QCOMPARE(classInfoMetricsAfter.requestedClassCount - classInfoMetricsBefore.requestedClassCount, 3);
    QCOMPARE(classInfoMetricsAfter.metadataStatementCount - classInfoMetricsBefore.metadataStatementCount, 1);
    QCOMPARE(classInfoMetricsAfter.regularScheduleStatementCount - classInfoMetricsBefore.regularScheduleStatementCount, 1);
    QCOMPARE(
        services.databaseSession()->classInfoRepository()
            ->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        0
        );

    const auto rosterMetricsAfter = services.databaseSession()
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
    const auto beforeClassInfo = services.databaseSession()
        ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics();
    const auto beforeRoster = services.databaseSession()
        ->rosterRepository()->templatePrintBatchReadMetrics();

    const auto empty = query.execute({});
    QVERIFY(empty);
    QVERIFY(empty.value().empty());
    QCOMPARE(services.databaseSession()
                 ->classInfoRepository()->rosterPrintClassInfoBatchReadMetrics()
                 .callCount,
        beforeClassInfo.callCount);
    QCOMPARE(services.databaseSession()
                 ->rosterRepository()->templatePrintBatchReadMetrics()
                 .callCount,
        beforeRoster.callCount);

    const auto noColumns = query.execute({classId(classIdValue)});
    QVERIFY(noColumns);
    QVERIFY(noColumns.value()[0].roster.columns.empty());
    const auto afterRoster = services.databaseSession()
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
    QVERIFY(services.databaseSession()->rosterRepository()->saveRoster(
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

QTEST_MAIN(NextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests)

#include "next_platform_application_services_roster_template_print_source_read_port_tests.moc"
