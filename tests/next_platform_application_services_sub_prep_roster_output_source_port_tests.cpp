#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/roster.h"
#include "next/application/sub_prep_roster_output_source_query.h"
#include "next/platform/application_services_sub_prep_roster_output_source_port.h"

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>
#include <QVariant>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepRosterOutputSourcePort;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-roster-output-%1.db").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(ApplicationServices& services, QTemporaryDir& directory)
{
    return services.openDatabase(databasePath(directory)).has_value();
}

ClassId classId(const int id)
{
    return *ClassId::fromString(std::to_string(id));
}

TeacherId teacherId(const int id)
{
    return *TeacherId::fromString(std::to_string(id));
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store)
    {
        return false;
    }
    QSqlQuery query(store->database());
    return query.exec(statement);
}

bool installMalformedRosterColumnsCursorFixture(
    ApplicationServices& services,
    const int classIdValue
    )
{
    if (!executeSql(services, QStringLiteral(R"(
        CREATE TEMP TABLE roster_columns_cursor_fixture (
            id INTEGER NOT NULL,
            class_id INTEGER NOT NULL,
            position INTEGER NOT NULL,
            payload TEXT NOT NULL
        )
    )"))
        || !executeSql(services, QStringLiteral(
            "CREATE INDEX roster_columns_cursor_fixture_order "
            "ON roster_columns_cursor_fixture (class_id, position, id)"
            ))
        || !executeSql(services, QStringLiteral(R"(
            CREATE TEMP VIEW roster_columns AS
            SELECT id, class_id, position,
                   json_extract(payload, '$.name') AS name,
                   0 AS width
            FROM roster_columns_cursor_fixture
        )")))
    {
        return false;
    }

    WorkspaceStore* const store = services.workspaceStore();
    if (!store)
    {
        return false;
    }

    QSqlQuery insert(store->database());
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_columns_cursor_fixture "
            "(id, class_id, position, payload) VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }

    insert.addBindValue(1);
    insert.addBindValue(classIdValue);
    insert.addBindValue(0);
    insert.addBindValue(QStringLiteral(R"({"name":"English"})"));
    if (!insert.exec())
    {
        return false;
    }
    insert.finish();
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_columns_cursor_fixture "
            "(id, class_id, position, payload) VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }

    insert.addBindValue(2);
    insert.addBindValue(classIdValue);
    insert.addBindValue(1);
    insert.addBindValue(QStringLiteral("not-json"));
    return insert.exec();
}

bool installMalformedRosterDataCursorFixture(
    ApplicationServices& services,
    const int classIdValue
    )
{
    if (!executeSql(services, QStringLiteral(R"(
        CREATE TEMP TABLE roster_data_cursor_fixture (
            class_id INTEGER NOT NULL,
            row_index INTEGER NOT NULL,
            col_index INTEGER NOT NULL,
            payload TEXT NOT NULL
        )
    )"))
        || !executeSql(services, QStringLiteral(
            "CREATE INDEX roster_data_cursor_fixture_order "
            "ON roster_data_cursor_fixture (class_id, row_index, col_index)"
            ))
        || !executeSql(services, QStringLiteral(R"(
            CREATE TEMP VIEW roster_data AS
            SELECT class_id, row_index, col_index,
                   json_extract(payload, '$.value') AS value
            FROM roster_data_cursor_fixture
        )")))
    {
        return false;
    }

    WorkspaceStore* const store = services.workspaceStore();
    if (!store)
    {
        return false;
    }

    QSqlQuery insert(store->database());
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_data_cursor_fixture "
            "(class_id, row_index, col_index, payload) VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }

    insert.addBindValue(classIdValue);
    insert.addBindValue(0);
    insert.addBindValue(0);
    insert.addBindValue(QStringLiteral(R"({"value":"Alex"})"));
    if (!insert.exec())
    {
        return false;
    }
    insert.finish();
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_data_cursor_fixture "
            "(class_id, row_index, col_index, payload) VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }

    insert.addBindValue(classIdValue);
    insert.addBindValue(0);
    insert.addBindValue(1);
    insert.addBindValue(QStringLiteral("not-json"));
    return insert.exec();
}

bool insertRosterOutputColumns(
    ApplicationServices& services,
    const int classIdValue,
    const QStringList& columns
    )
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store)
    {
        return false;
    }

    QSqlQuery query(store->database());
    for (qsizetype index = 0; index < columns.size(); ++index)
    {
        if (!query.prepare(QStringLiteral(
                "INSERT INTO roster_columns (class_id, name, position, width) "
                "VALUES (?, ?, ?, 0)"
                )))
        {
            return false;
        }
        query.addBindValue(classIdValue);
        query.addBindValue(columns.at(index));
        query.addBindValue(index);
        if (!query.exec())
        {
            return false;
        }
        query.finish();
    }
    return true;
}

bool insertRosterOutputCell(
    ApplicationServices& services,
    const int classIdValue,
    const int rowIndex,
    const int columnIndex,
    const QString& value
    )
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store)
    {
        return false;
    }

    QSqlQuery query(store->database());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }
    query.addBindValue(classIdValue);
    query.addBindValue(rowIndex);
    query.addBindValue(columnIndex);
    query.addBindValue(value);
    return query.exec();
}

int createTeacher(
    ApplicationServices& services,
    const QString& englishName = QStringLiteral("Teacher English")
    )
{
    Teacher teacher;
    teacher.teacherKr = QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98");
    teacher.teacherEn = englishName;
    teacher.preferredName = englishName;
    teacher.preferredRomanization = QStringLiteral("Romanized Name");
    teacher.roomNumber = QStringLiteral("Teacher Room");
    teacher.wifiName = QStringLiteral("Teacher Network");
    teacher.wifiPassword = QStringLiteral("Teacher WiFi Password");
    teacher.zoomId = QStringLiteral("teacher-zoom-id");
    teacher.zoomPassword = QStringLiteral("teacher-zoom-password");
    teacher.notes = QStringLiteral("not copied into roster output");
    const auto created = services.teacherService()->create(teacher);
    return created ? *created : -1;
}

int createClass(
    ApplicationServices& services,
    const QString& name,
    const int assignedTeacherId,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& startTime,
    const QString& endTime,
    const QList<ClassTime>& intensiveTimes = {}
    )
{
    const auto created = services.classService()->create(name);
    if (!created)
    {
        return -1;
    }

    ClassInfo info;
    info.classId = *created;
    info.teacherId = assignedTeacherId;
    info.teacherEn = QStringLiteral("Class English Name");
    info.teacherKr = QStringLiteral("Class Korean Name");
    info.classGrade = grade;
    info.classLevel = level;
    info.roomNumber = QStringLiteral("Class Room");
    info.wifiName = QStringLiteral("Class Network");
    info.wifiPassword = QStringLiteral("Class WiFi Password");
    info.zoomId = QStringLiteral("class-zoom-id");
    info.zoomPassword = QStringLiteral("class-zoom-password");
    info.classTimes = {{day, startTime, endTime}};
    info.intensiveTimes = intensiveTimes;
    const auto saved = services.classService()->saveClassInfo(info);
    if (!saved)
    {
        return -1;
    }
    return *created;
}

Roster rosterWithColumns()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columns.append(QStringLiteral("Notes"));
    roster.columns.append(QStringLiteral("Unused"));
    roster.rows = {
        {
            QStringLiteral("Alex"),
            QString::fromUtf8("\xEA\xB9\x80\xEA\xB0\x80"),
            {},
            {},
            {},
            {},
            QStringLiteral("First note"),
            QStringLiteral("not projected")
        },
        {
            QStringLiteral("Casey"),
            QString::fromUtf8("\xEC\x9D\xB4\xEB\xAF\xBC"),
            {},
            {},
            {},
            {},
            QStringLiteral("Second note"),
            QStringLiteral("also not projected")
        }
    };
    return roster;
}

SubPrepRosterOutputSourceRequest requestFor(
    const std::vector<ClassId>& ids,
    const std::vector<SubPrepWeekday>& days,
    const ScheduleViewMode mode = ScheduleViewMode::Regular
    )
{
    return {
        .selectedClassIds = ids,
        .selectedDays = days,
        .mode = mode,
        .selectedExtraColumns = {"Notes"}
    };
}

} // namespace

class NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsSelectedClassesModeAndRequestedRosterColumns();
    void rejectsFractionalRosterRowIndexAsTechnical();
    void rejectsLateRosterColumnFetchFailureAsTechnical();
    void rejectsLateRosterCellFetchFailureAsTechnical();
    void returnsEmptyRosterOutputWhenHeadersHaveNoStoredCells();
    void projectsSparseIntegerRosterRowsWithBlankGaps();
    void preservesWideTeacherIdFromSqliteThroughRosterOutputSourcePort();
    void batchesDistinctAssignedTeachersInFirstSeenOrder();
    void includesUnassignedTeacherAndUsesSelectedMode();
    void emptyScopeAndNoncanonicalIdsAvoidDatabaseReads();
    void unavailableSessionsDoNotFallBackToDataService();
    void staleTeacherFailureDoesNotReturnPartialInput();
    void activeSessionScheduleReadFailureIsTechnical();
    void classNameBatchFailureAbortsSource();
    void classNameBatchMissingRowPropagatesNotFound();
    void classInfoBatchFailureAbortsSource();
    void classInfoBatchRejectsMissingMetadataRecord();
    void rejectsOutOfBoundRosterAndClassText();
    void rejectsScheduleMeetingOverflow();
    void rejectsAggregateMeetingOverflow();
    void rejectsAggregateRosterRowOverflowAcrossClasses();
    void rejectsAggregateRosterCellOverflowAcrossClasses();
    void rejectsAggregateRosterTextOverflowBeforeReturningInput();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
readsSelectedClassesModeAndRequestedRosterColumns()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    const int outsideTeacher = createTeacher(
        services,
        QStringLiteral("Outside teacher")
        );
    QVERIFY(teacher > 0);
    QVERIFY(outsideTeacher > 0);
    QVERIFY(outsideTeacher != teacher);

    const int firstClass = createClass(
        services,
        QStringLiteral("First class"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    const int secondClass = createClass(
        services,
        QStringLiteral("Second class"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Artemis"),
        QStringLiteral("Monday"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("12:00 PM")
        );
    const int outsideSelectedDays = createClass(
        services,
        QStringLiteral("Tuesday class"),
        outsideTeacher,
        QStringLiteral("E6"),
        QStringLiteral("Helios"),
        QStringLiteral("Tuesday"),
        QStringLiteral("1:00 PM"),
        QStringLiteral("2:00 PM")
        );
    QVERIFY(firstClass > 0);
    QVERIFY(secondClass > 0);
    QVERIFY(outsideSelectedDays > 0);
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Monday', '12:00 PM', '1:00 PM'), "
            "(%1, 'Tuesday', '2:00 PM', '3:00 PM')"
            ).arg(secondClass)
        ));
    QVERIFY(services.rosterService()->saveRoster(firstClass, rosterWithColumns()));
    QVERIFY(services.rosterService()->saveRoster(secondClass, rosterWithColumns()));
    QVERIFY(services.rosterService()->saveRoster(
        outsideSelectedDays,
        rosterWithColumns()
        ));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassRepository* const classRepository = store->classRepository();
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(classInfoRepository);
    const ClassRepository::ReadMetrics classReadMetricsBefore =
        classRepository->readMetrics();
    const auto classInfoBatchMetricsBefore =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();
    const auto scheduleBatchMetricsBefore =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    const ScheduleClassInfoReadMetrics scheduleReadMetricsBefore =
        classInfoRepository->scheduleClassInfoReadMetrics();
    TeacherRepository* const teacherRepository = store->teacherRepository();
    QVERIFY(teacherRepository);
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics
        teacherProfileMetricsBefore =
            teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();
    const TeacherProfileBatchReadMetrics genericProfileMetricsBefore =
        teacherRepository->teacherProfileBatchReadMetrics();

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "UPDATE teachers SET preferred_name = 'Preferred Person', "
            "preferred_romanization = 'Preferred Romanization' WHERE id = %1"
            ).arg(teacher)
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const SubPrepRosterOutputSourceQuery query(port);
    const auto requested = requestFor(
        {
            classId(secondClass),
            classId(firstClass),
            classId(outsideSelectedDays)
        },
        {SubPrepWeekday::Monday}
        );
    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(result.value().classes().size(), std::size_t(2));
    QCOMPARE(result.value().classes().at(0).id, classId(secondClass));
    QCOMPARE(result.value().classes().at(1).id, classId(firstClass));
    QCOMPARE(result.value().teachers().size(), std::size_t(1));
    QCOMPARE(
        result.value().teachers().front(),
        (SubPrepRosterOutputTeacher{
            teacherId(teacher),
            "Teacher English",
            utf8(QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98")),
            "Preferred Person",
            "Preferred Romanization"
        })
        );

    const auto& first = result.value().classes().front();
    QCOMPARE(first.classroomName, std::string("Second class"));
    QCOMPARE(first.grade, std::string("E5"));
    QCOMPARE(first.level, std::string("Artemis"));
    QCOMPARE(first.classTeacherEnglishName, std::string("Teacher English"));
    QCOMPARE(
        first.classTeacherKoreanName,
        utf8(QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98"))
        );
    QCOMPARE(first.room, std::string("Teacher Room"));
    QCOMPARE(first.wifiName, std::string("Teacher Network"));
    QCOMPARE(first.wifiPassword, std::string("Teacher WiFi Password"));
    QCOMPARE(first.zoomId, std::string("teacher-zoom-id"));
    QCOMPARE(first.zoomPassword, std::string("teacher-zoom-password"));
    QCOMPARE(first.meetings.size(), std::size_t(2));
    QCOMPARE(first.meetings.front().weekday, SubPrepWeekday::Monday);
    QCOMPARE(first.meetings.front().startTime, std::string("11:00 AM"));
    QCOMPARE(first.meetings.back().weekday, SubPrepWeekday::Monday);
    QCOMPARE(first.meetings.back().startTime, std::string("12:00 PM"));
    QCOMPARE(
        first.rosterColumns,
        (std::vector<std::string>{"English", "Korean", "Notes"})
        );
    QCOMPARE(first.rosterRows.size(), std::size_t(2));
    QCOMPARE(
        first.rosterRows.front(),
        (std::vector<std::string>{
            "Alex",
            utf8(QString::fromUtf8("\xEA\xB9\x80\xEA\xB0\x80")),
            "First note"
        })
        );

    const ClassRepository::ReadMetrics classReadMetricsAfter =
        classRepository->readMetrics();
    QCOMPARE(
        classReadMetricsAfter.getClassByIdCallCount
            - classReadMetricsBefore.getClassByIdCallCount,
        0
        );
    QCOMPARE(
        classReadMetricsAfter.getClassesByIdsCallCount
            - classReadMetricsBefore.getClassesByIdsCallCount,
        1
        );
    QCOMPARE(
        classReadMetricsAfter.requestedClassCount
            - classReadMetricsBefore.requestedClassCount,
        2
        );
    QCOMPARE(
        classReadMetricsAfter.batchStatementCount
            - classReadMetricsBefore.batchStatementCount,
        1
        );
    const auto classInfoBatchMetricsAfter =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();
    QCOMPARE(
        classInfoBatchMetricsAfter.callCount
            - classInfoBatchMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        classInfoBatchMetricsAfter.requestedClassCount
            - classInfoBatchMetricsBefore.requestedClassCount,
        2
        );
    QCOMPARE(
        classInfoBatchMetricsAfter.statementCount
            - classInfoBatchMetricsBefore.statementCount,
        1
        );
    const auto scheduleBatchMetricsAfter =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    QCOMPARE(
        scheduleBatchMetricsAfter.callCount - scheduleBatchMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        scheduleBatchMetricsAfter.requestedClassCount
            - scheduleBatchMetricsBefore.requestedClassCount,
        3
        );
    QCOMPARE(
        scheduleBatchMetricsAfter.statementCount
            - scheduleBatchMetricsBefore.statementCount,
        1
        );
    QCOMPARE(
        classInfoRepository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        scheduleReadMetricsBefore.singleClassInfoReadCount
        );
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics
        teacherProfileMetricsAfter =
            teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();
    QCOMPARE(
        teacherProfileMetricsAfter.callCount
            - teacherProfileMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        teacherProfileMetricsAfter.requestedTeacherCount
            - teacherProfileMetricsBefore.requestedTeacherCount,
        1
        );
    QCOMPARE(
        teacherProfileMetricsAfter.statementCount
            - teacherProfileMetricsBefore.statementCount,
        1
        );
    const TeacherProfileBatchReadMetrics genericProfileMetricsAfter =
        teacherRepository->teacherProfileBatchReadMetrics();
    QCOMPARE(
        genericProfileMetricsAfter.callCount - genericProfileMetricsBefore.callCount,
        0
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsFractionalRosterRowIndexAsTechnical()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Fractional roster row"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    const QSqlDatabase database = store->database();
    QSqlQuery insertColumn(database);
    for (qsizetype index = 0; index < Roster::BaseColumns.size(); ++index)
    {
        QVERIFY(insertColumn.prepare(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, 0)"
            )));
        insertColumn.addBindValue(classIdValue);
        insertColumn.addBindValue(Roster::BaseColumns.at(index));
        insertColumn.addBindValue(index);
        QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));
        insertColumn.finish();
    }

    QSqlQuery emptyRoster(database);
    QVERIFY(emptyRoster.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM roster_data WHERE class_id=?"
        )));
    emptyRoster.addBindValue(classIdValue);
    QVERIFY2(emptyRoster.exec(), qPrintable(emptyRoster.lastError().text()));
    QVERIFY(emptyRoster.next());
    QCOMPARE(emptyRoster.value(0).toInt(), 0);
    emptyRoster.finish();

    QSqlQuery insertCell(database);
    QVERIFY(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )));
    insertCell.addBindValue(classIdValue);
    insertCell.addBindValue(QVariant::fromValue(0.25));
    insertCell.addBindValue(0);
    insertCell.addBindValue(QStringLiteral("Fractional phantom student"));
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    insertCell.finish();

    QSqlQuery storedCell(database);
    QVERIFY(storedCell.prepare(QStringLiteral(
        "SELECT row_index, typeof(row_index) AS row_storage_type, "
        "col_index, typeof(col_index) AS col_storage_type, value "
        "FROM roster_data WHERE class_id=?"
        )));
    storedCell.addBindValue(classIdValue);
    QVERIFY2(storedCell.exec(), qPrintable(storedCell.lastError().text()));
    QVERIFY(storedCell.next());
    QCOMPARE(storedCell.value("row_index").toDouble(), 0.25);
    QCOMPARE(storedCell.value("row_storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(storedCell.value("col_index").toInt(), 0);
    QCOMPARE(storedCell.value("col_storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(
        storedCell.value("value").toString(),
        QStringLiteral("Fractional phantom student")
        );
    QVERIFY(!storedCell.next());
    storedCell.finish();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QVERIFY(
        result.error().message.find("row_index has non-INTEGER SQLite storage")
        != std::string::npos
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsLateRosterColumnFetchFailureAsTechnical()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Late roster column fetch failure"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    QVERIFY(installMalformedRosterColumnsCursorFixture(services, classIdValue));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlQuery preflight(store->database());
    preflight.setForwardOnly(true);
    QVERIFY(preflight.prepare(QStringLiteral(R"(
        SELECT substr(name, 1, 257) AS bounded_name,
               length(CAST(name AS BLOB)) AS name_bytes
        FROM roster_columns
        WHERE class_id=?
        ORDER BY position, id
        LIMIT ?
    )")));
    preflight.addBindValue(classIdValue);
    preflight.addBindValue(4'097);
    QVERIFY2(preflight.exec(), qPrintable(preflight.lastError().text()));
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value("bounded_name").toString(), QStringLiteral("English"));
    QCOMPARE(preflight.value("name_bytes").toInt(), 7);
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    QVERIFY(preflight.lastError().text().contains(
        QStringLiteral("malformed JSON"),
        Qt::CaseInsensitive
        ));
    preflight.finish();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QVERIFY(result.error().message.find(
        "Loading bounded roster output columns failed"
        ) != std::string::npos);
    QVERIFY(result.error().message.find(
        "class id " + std::to_string(classIdValue)
        ) != std::string::npos);
    QVERIFY(result.error().message.find("malformed JSON") != std::string::npos);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsLateRosterCellFetchFailureAsTechnical()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Late roster cell fetch failure"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);

    QStringList columns = Roster::BaseColumns;
    columns.append(QStringLiteral("Notes"));
    QVERIFY(insertRosterOutputColumns(services, classIdValue, columns));
    QVERIFY(installMalformedRosterDataCursorFixture(services, classIdValue));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlQuery preflight(store->database());
    preflight.setForwardOnly(true);
    QVERIFY(preflight.prepare(QStringLiteral(R"(
        SELECT row_index, col_index,
               substr(value, 1, %1) AS bounded_value,
               length(CAST(value AS BLOB)) AS value_bytes
        FROM roster_data
        WHERE class_id=? AND row_index>=0 AND col_index IN (%2)
        ORDER BY row_index, col_index
        LIMIT ?
    )")
        .arg(static_cast<qulonglong>(
            kRosterRepositoryOutputMaxCellBytes + 1
            ))
        .arg(QStringLiteral("?,?,?"))));
    preflight.addBindValue(classIdValue);
    preflight.addBindValue(0);
    preflight.addBindValue(1);
    preflight.addBindValue(6);
    preflight.addBindValue(4);
    QVERIFY2(preflight.exec(), qPrintable(preflight.lastError().text()));
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value("row_index").toInt(), 0);
    QCOMPARE(preflight.value("col_index").toInt(), 0);
    QCOMPARE(preflight.value("bounded_value").toString(), QStringLiteral("Alex"));
    QCOMPARE(preflight.value("value_bytes").toInt(), 4);
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    QVERIFY(preflight.lastError().text().contains(
        QStringLiteral("malformed JSON"),
        Qt::CaseInsensitive
        ));
    preflight.finish();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QVERIFY(result.error().message.find(
        "Loading bounded roster output values failed"
        ) != std::string::npos);
    QVERIFY(result.error().message.find(
        "class id " + std::to_string(classIdValue)
        ) != std::string::npos);
    QVERIFY(result.error().message.find("malformed JSON") != std::string::npos);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
returnsEmptyRosterOutputWhenHeadersHaveNoStoredCells()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Empty roster output"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);

    QStringList columns = Roster::BaseColumns;
    columns.append(QStringLiteral("Notes"));
    columns.append(QStringLiteral("Unused"));
    QVERIFY(insertRosterOutputColumns(services, classIdValue, columns));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlQuery emptyRoster(store->database());
    QVERIFY(emptyRoster.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM roster_data WHERE class_id=?"
        )));
    emptyRoster.addBindValue(classIdValue);
    QVERIFY2(emptyRoster.exec(), qPrintable(emptyRoster.lastError().text()));
    QVERIFY(emptyRoster.next());
    QCOMPARE(emptyRoster.value(0).toInt(), 0);
    emptyRoster.finish();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    const auto& outputClass = result.value().classes.front();
    QCOMPARE(
        outputClass.rosterColumns,
        (std::vector<std::string>{"English", "Korean", "Notes"})
        );
    QVERIFY(outputClass.rosterRows.empty());
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
projectsSparseIntegerRosterRowsWithBlankGaps()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Sparse roster output"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);

    QStringList columns = Roster::BaseColumns;
    columns.append(QStringLiteral("Notes"));
    columns.append(QStringLiteral("Unused"));
    QVERIFY(insertRosterOutputColumns(services, classIdValue, columns));
    QVERIFY(insertRosterOutputCell(
        services,
        classIdValue,
        2,
        0,
        QStringLiteral("Sparse English")
        ));
    QVERIFY(insertRosterOutputCell(
        services,
        classIdValue,
        2,
        1,
        QStringLiteral("Sparse Korean")
        ));
    QVERIFY(insertRosterOutputCell(
        services,
        classIdValue,
        2,
        2,
        QStringLiteral("Unrequested base value")
        ));
    QVERIFY(insertRosterOutputCell(
        services,
        classIdValue,
        2,
        6,
        QStringLiteral("Sparse note")
        ));
    QVERIFY(insertRosterOutputCell(
        services,
        classIdValue,
        2,
        7,
        QStringLiteral("Unrequested extra value")
        ));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlQuery storedRow(store->database());
    QVERIFY(storedRow.prepare(QStringLiteral(
        "SELECT DISTINCT row_index, typeof(row_index) AS storage_type "
        "FROM roster_data WHERE class_id=? ORDER BY row_index"
        )));
    storedRow.addBindValue(classIdValue);
    QVERIFY2(storedRow.exec(), qPrintable(storedRow.lastError().text()));
    QVERIFY(storedRow.next());
    QCOMPARE(storedRow.value("row_index").toInt(), 2);
    QCOMPARE(storedRow.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(!storedRow.next());
    storedRow.finish();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    const auto& outputClass = result.value().classes.front();
    QCOMPARE(
        outputClass.rosterColumns,
        (std::vector<std::string>{"English", "Korean", "Notes"})
        );
    QCOMPARE(outputClass.rosterRows.size(), std::size_t(3));
    const std::vector<std::string> blankProjectedRow{"", "", ""};
    QCOMPARE(outputClass.rosterRows[0], blankProjectedRow);
    QCOMPARE(outputClass.rosterRows[1], blankProjectedRow);
    QCOMPARE(
        outputClass.rosterRows[2],
        (std::vector<std::string>{
            "Sparse English",
            "Sparse Korean",
            "Sparse note"
        })
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
preservesWideTeacherIdFromSqliteThroughRosterOutputSourcePort()
{
    constexpr qlonglong wideTeacherId = 2147483648LL;

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int classIdValue = createClass(
        services,
        QStringLiteral("Wide teacher ID class"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    QVERIFY(classIdValue < 1000);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlDatabase database = store->database();

    QSqlQuery insertTeacher(database);
    QVERIFY(insertTeacher.prepare(QStringLiteral(
        "INSERT INTO teachers ("
        "id, teacher_kr, teacher_en, preferred_name, "
        "preferred_romanization, room_number, wifi_name, wifi_password, "
        "zoom_id, zoom_password) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        )));
    insertTeacher.bindValue(
        0,
        QVariant::fromValue<qlonglong>(wideTeacherId)
        );
    const QString koreanTeacherName = QString::fromUtf8(
        "\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 "
        "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4"
        );
    insertTeacher.bindValue(1, koreanTeacherName);
    insertTeacher.bindValue(2, QStringLiteral("Wide Teacher English"));
    insertTeacher.bindValue(3, QStringLiteral("Wide Preferred Name"));
    insertTeacher.bindValue(4, QStringLiteral("Wide Preferred Romanization"));
    insertTeacher.bindValue(5, QStringLiteral("Wide Teacher Room"));
    insertTeacher.bindValue(6, QStringLiteral("Wide Teacher Network"));
    insertTeacher.bindValue(7, QStringLiteral("Wide Teacher WiFi Password"));
    insertTeacher.bindValue(8, QStringLiteral("wide-teacher-zoom-id"));
    insertTeacher.bindValue(9, QStringLiteral("wide-teacher-zoom-password"));
    QVERIFY2(
        insertTeacher.exec(),
        qPrintable(insertTeacher.lastError().text())
        );

    QSqlQuery assignTeacher(database);
    QVERIFY(assignTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    assignTeacher.bindValue(
        0,
        QVariant::fromValue<qlonglong>(wideTeacherId)
        );
    assignTeacher.bindValue(1, classIdValue);
    QVERIFY2(
        assignTeacher.exec(),
        qPrintable(assignTeacher.lastError().text())
        );
    QCOMPARE(assignTeacher.numRowsAffected(), 1);

    QSqlQuery persistedTeacher(database);
    QVERIFY(persistedTeacher.prepare(QStringLiteral(
        "SELECT typeof(id), id FROM teachers WHERE id = ?"
        )));
    persistedTeacher.bindValue(
        0,
        QVariant::fromValue<qlonglong>(wideTeacherId)
        );
    QVERIFY2(
        persistedTeacher.exec(),
        qPrintable(persistedTeacher.lastError().text())
        );
    QVERIFY(persistedTeacher.next());
    QCOMPARE(persistedTeacher.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(persistedTeacher.value(1).toLongLong(), wideTeacherId);

    QSqlQuery persistedAssignment(database);
    QVERIFY(persistedAssignment.prepare(QStringLiteral(
        "SELECT typeof(teacher_id), teacher_id FROM class_info "
        "WHERE class_id = ?"
        )));
    persistedAssignment.bindValue(0, classIdValue);
    QVERIFY2(
        persistedAssignment.exec(),
        qPrintable(persistedAssignment.lastError().text())
        );
    QVERIFY(persistedAssignment.next());
    QCOMPARE(
        persistedAssignment.value(0).toString(),
        QStringLiteral("integer")
        );
    QCOMPARE(persistedAssignment.value(1).toLongLong(), wideTeacherId);

    QSqlQuery validTeacherAssignment(database);
    QVERIFY(validTeacherAssignment.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM teachers AS teacher "
        "JOIN class_info AS info ON info.teacher_id = teacher.id "
        "WHERE info.class_id = ? AND teacher.id = ?"
        )));
    validTeacherAssignment.bindValue(0, classIdValue);
    validTeacherAssignment.bindValue(
        1,
        QVariant::fromValue<qlonglong>(wideTeacherId)
        );
    QVERIFY2(
        validTeacherAssignment.exec(),
        qPrintable(validTeacherAssignment.lastError().text())
        );
    QVERIFY(validTeacherAssignment.next());
    QCOMPARE(validTeacherAssignment.value(0).toInt(), 1);

    QVERIFY(services.rosterService()->saveRoster(
        classIdValue,
        rosterWithColumns()
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const SubPrepRosterOutputSourceQuery query(port);
    const auto result = query.execute(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(result);
    QCOMPARE(result.value().teachers().size(), std::size_t(1));
    const auto expectedTeacherId = *TeacherId::fromString("2147483648");
    QCOMPARE(
        result.value().teachers().front(),
        (SubPrepRosterOutputTeacher{
            expectedTeacherId,
            "Wide Teacher English",
            utf8(koreanTeacherName),
            "Wide Preferred Name",
            "Wide Preferred Romanization"
        })
        );

    QCOMPARE(result.value().classes().size(), std::size_t(1));
    const auto& outputClass = result.value().classes().front();
    QCOMPARE(outputClass.id, classId(classIdValue));
    QVERIFY(outputClass.teacherId.has_value());
    QCOMPARE(*outputClass.teacherId, expectedTeacherId);
    QCOMPARE(outputClass.classroomName, std::string("Wide teacher ID class"));
    QCOMPARE(outputClass.grade, std::string("E4"));
    QCOMPARE(outputClass.level, std::string("Hercules"));
    QCOMPARE(
        outputClass.classTeacherEnglishName,
        std::string("Wide Teacher English")
        );
    QCOMPARE(outputClass.classTeacherKoreanName, utf8(koreanTeacherName));
    QCOMPARE(outputClass.room, std::string("Wide Teacher Room"));
    QCOMPARE(outputClass.wifiName, std::string("Wide Teacher Network"));
    QCOMPARE(
        outputClass.wifiPassword,
        std::string("Wide Teacher WiFi Password")
        );
    QCOMPARE(outputClass.zoomId, std::string("wide-teacher-zoom-id"));
    QCOMPARE(
        outputClass.zoomPassword,
        std::string("wide-teacher-zoom-password")
        );
    QCOMPARE(outputClass.meetings.size(), std::size_t(1));
    QCOMPARE(outputClass.meetings.front().weekday, SubPrepWeekday::Monday);
    QCOMPARE(outputClass.meetings.front().startTime, std::string("9:00 AM"));
    QCOMPARE(outputClass.meetings.front().endTime, std::string("10:00 AM"));
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
batchesDistinctAssignedTeachersInFirstSeenOrder()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int firstTeacher = createTeacher(
        services,
        QStringLiteral("First teacher")
        );
    const int secondTeacher = createTeacher(
        services,
        QStringLiteral("Second teacher")
        );
    QVERIFY(firstTeacher > 0);
    QVERIFY(secondTeacher > 0);

    const int secondTeacherFirstClass = createClass(
        services,
        QStringLiteral("Second teacher first"),
        secondTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    const int firstTeacherClass = createClass(
        services,
        QStringLiteral("First teacher class"),
        firstTeacher,
        QStringLiteral("E5"),
        QStringLiteral("Artemis"),
        QStringLiteral("Monday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("11:00 AM")
        );
    const int secondTeacherAgainClass = createClass(
        services,
        QStringLiteral("Second teacher again"),
        secondTeacher,
        QStringLiteral("E6"),
        QStringLiteral("Helios"),
        QStringLiteral("Monday"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("12:00 PM")
        );
    QVERIFY(secondTeacherFirstClass > 0);
    QVERIFY(firstTeacherClass > 0);
    QVERIFY(secondTeacherAgainClass > 0);
    for (const int classIdValue : {
             secondTeacherFirstClass,
             firstTeacherClass,
             secondTeacherAgainClass
         })
    {
        QVERIFY(services.rosterService()->saveRoster(
            classIdValue,
            rosterWithColumns()
            ));
    }

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    TeacherRepository* const teacherRepository = store->teacherRepository();
    QVERIFY(teacherRepository);
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics before =
        teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {
            classId(secondTeacherFirstClass),
            classId(firstTeacherClass),
            classId(secondTeacherAgainClass)
        },
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(3));
    QCOMPARE(result.value().classes[0].id, classId(secondTeacherFirstClass));
    QCOMPARE(result.value().classes[1].id, classId(firstTeacherClass));
    QCOMPARE(result.value().classes[2].id, classId(secondTeacherAgainClass));
    QCOMPARE(result.value().teachers.size(), std::size_t(2));
    QVERIFY(result.value().teachers[0].id == teacherId(secondTeacher));
    QVERIFY(result.value().teachers[1].id == teacherId(firstTeacher));
    QCOMPARE(
        result.value().teachers[0].englishName,
        std::string("Second Teacher")
        );
    QCOMPARE(
        result.value().teachers[1].englishName,
        std::string("First Teacher")
        );

    const SubPrepRosterOutputTeacherProfileBatchReadMetrics after =
        teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.requestedTeacherCount - before.requestedTeacherCount, 2);
    QCOMPARE(after.statementCount - before.statementCount, 1);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
includesUnassignedTeacherAndUsesSelectedMode()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int intensiveClass = createClass(
        services,
        QStringLiteral("Intensive only"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Tuesday"),
        QStringLiteral("1:00 PM"),
        QStringLiteral("2:00 PM"),
        {{
            QStringLiteral("Monday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:45 PM")
        }}
        );
    QVERIFY(intensiveClass > 0);
    QVERIFY(services.rosterService()->saveRoster(
        intensiveClass,
        rosterWithColumns()
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const SubPrepRosterOutputSourceQuery query(port);
    const std::vector<ClassId> ids{classId(intensiveClass)};
    const std::vector<SubPrepWeekday> days{SubPrepWeekday::Monday};

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassRepository* const classRepository = store->classRepository();
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(classInfoRepository);
    TeacherRepository* const teacherRepository = store->teacherRepository();
    QVERIFY(teacherRepository);
    const ClassRepository::ReadMetrics classMetricsBefore =
        classRepository->readMetrics();
    const auto classInfoMetricsBefore =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();
    const auto scheduleMetricsBefore =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics
        teacherProfileMetricsBefore =
            teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();

    const auto regular = query.execute(requestFor(ids, days));
    QVERIFY(regular);
    QVERIFY(regular.value().empty());
    QCOMPARE(
        classRepository->readMetrics().getClassesByIdsCallCount,
        classMetricsBefore.getClassesByIdsCallCount
        );
    QCOMPARE(
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics().callCount,
        classInfoMetricsBefore.callCount
        );
    const auto scheduleMetricsAfterRegular =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    QCOMPARE(
        scheduleMetricsAfterRegular.callCount - scheduleMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        scheduleMetricsAfterRegular.statementCount
            - scheduleMetricsBefore.statementCount,
        1
        );

    const auto intensive = query.execute(
        requestFor(ids, days, ScheduleViewMode::Intensive)
        );
    QVERIFY(intensive);
    QCOMPARE(intensive.value().classes().size(), std::size_t(1));
    QVERIFY(intensive.value().teachers().empty());
    QVERIFY(!intensive.value().classes().front().teacherId.has_value());
    QCOMPARE(
        intensive.value().classes().front().meetings.front().startTime,
        std::string("3:00 PM")
        );
    QCOMPARE(
        classRepository->readMetrics().getClassByIdCallCount
            - classMetricsBefore.getClassByIdCallCount,
        0
        );
    QCOMPARE(
        classRepository->readMetrics().getClassesByIdsCallCount
            - classMetricsBefore.getClassesByIdsCallCount,
        1
        );
    QCOMPARE(
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics().callCount
            - classInfoMetricsBefore.callCount,
        1
        );
    const auto scheduleMetricsAfterIntensive =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    QCOMPARE(
        scheduleMetricsAfterIntensive.callCount - scheduleMetricsBefore.callCount,
        2
        );
    QCOMPARE(
        scheduleMetricsAfterIntensive.statementCount
            - scheduleMetricsBefore.statementCount,
        2
        );
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics
        teacherProfileMetricsAfter =
            teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();
    QCOMPARE(
        teacherProfileMetricsAfter.callCount
            - teacherProfileMetricsBefore.callCount,
        0
        );
    QCOMPARE(
        teacherProfileMetricsAfter.requestedTeacherCount
            - teacherProfileMetricsBefore.requestedTeacherCount,
        0
        );
    QCOMPARE(
        teacherProfileMetricsAfter.statementCount
            - teacherProfileMetricsBefore.statementCount,
        0
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
emptyScopeAndNoncanonicalIdsAvoidDatabaseReads()
{
    ApplicationServices services;
    ApplicationServicesSubPrepRosterOutputSourcePort port(services);

    auto emptyRequest = requestFor({}, {SubPrepWeekday::Monday});
    const auto empty = port.loadSource(emptyRequest);
    QVERIFY(empty);
    QVERIFY(empty.value().classes.empty());

    auto emptyDayRequest = requestFor(
        {classId(1)},
        {}
        );
    const auto emptyDays = port.loadSource(emptyDayRequest);
    QVERIFY(emptyDays);
    QVERIFY(emptyDays.value().classes.empty());

    const auto aliasedId = ClassId::fromString("01");
    QVERIFY(aliasedId.has_value());
    auto invalidRequest = requestFor(
        {*aliasedId},
        {SubPrepWeekday::Monday}
        );
    const auto invalid = port.loadSource(invalidRequest);
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
unavailableSessionsDoNotFallBackToDataService()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto request = requestFor(
        {classId(1)},
        {SubPrepWeekday::Monday}
        );

    const auto unopened = port.loadSource(request);
    QVERIFY(!unopened);
    QCOMPARE(unopened.error().code, ErrorCode::NotFound);

    QVERIFY(openDatabase(services, m_directory));
    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());

    const auto closed = port.loadSource(request);
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, ErrorCode::NotFound);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
staleTeacherFailureDoesNotReturnPartialInput()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int firstClass = createClass(
        services,
        QStringLiteral("Unassigned first class"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    const int staleTeacherClass = createClass(
        services,
        QStringLiteral("Stale teacher second class"),
        -1,
        QStringLiteral("E5"),
        QStringLiteral("Artemis"),
        QStringLiteral("Monday"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("12:00 PM")
        );
    QVERIFY(firstClass > 0);
    QVERIFY(staleTeacherClass > 0);
    QVERIFY(services.rosterService()->saveRoster(
        firstClass,
        rosterWithColumns()
        ));
    QVERIFY(services.rosterService()->saveRoster(
        staleTeacherClass,
        rosterWithColumns()
        ));
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    QVERIFY(executeSql(
        services,
        QStringLiteral("UPDATE class_info SET teacher_id = 999999 WHERE class_id = %1")
            .arg(staleTeacherClass)
        ));
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));

    TeacherRepository* const teacherRepository =
        services.workspaceStore()->teacherRepository();
    QVERIFY(teacherRepository);
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics
        teacherProfileMetricsBefore =
            teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(firstClass), classId(staleTeacherClass)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
    const SubPrepRosterOutputTeacherProfileBatchReadMetrics
        teacherProfileMetricsAfter =
            teacherRepository->subPrepRosterOutputTeacherProfileBatchReadMetrics();
    QCOMPARE(
        teacherProfileMetricsAfter.callCount
            - teacherProfileMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        teacherProfileMetricsAfter.requestedTeacherCount
            - teacherProfileMetricsBefore.requestedTeacherCount,
        1
        );
    QCOMPARE(
        teacherProfileMetricsAfter.statementCount
            - teacherProfileMetricsBefore.statementCount,
        1
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
activeSessionScheduleReadFailureIsTechnical()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_times")));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classInfoRepository);
    const auto scheduleMetricsBefore =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(1)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    const auto scheduleMetricsAfter =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    QCOMPARE(scheduleMetricsAfter.callCount - scheduleMetricsBefore.callCount, 1);
    QCOMPARE(
        scheduleMetricsAfter.statementCount
            - scheduleMetricsBefore.statementCount,
        1
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
classNameBatchFailureAbortsSource()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Class name query failure"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    QVERIFY(services.rosterService()->saveRoster(
        classIdValue,
        rosterWithColumns()
        ));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassRepository* const classRepository = store->classRepository();
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(classInfoRepository);
    const ClassRepository::ReadMetrics classMetricsBefore =
        classRepository->readMetrics();
    const auto classInfoMetricsBefore =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();

    QVERIFY(executeSql(
        services,
        QStringLiteral("ALTER TABLE classes RENAME COLUMN name TO old_name")
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        classRepository->readMetrics().getClassByIdCallCount
            - classMetricsBefore.getClassByIdCallCount,
        0
        );
    QCOMPARE(
        classRepository->readMetrics().getClassesByIdsCallCount
            - classMetricsBefore.getClassesByIdsCallCount,
        1
        );
    QCOMPARE(
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics().callCount,
        classInfoMetricsBefore.callCount
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
classNameBatchMissingRowPropagatesNotFound()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Missing class row"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    QVERIFY(services.rosterService()->saveRoster(
        classIdValue,
        rosterWithColumns()
        ));

    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DELETE FROM classes WHERE id = %1").arg(classIdValue)
        ));
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));

    ClassRepository* const classRepository =
        services.workspaceStore()->classRepository();
    QVERIFY(classRepository);
    const auto missing = classRepository->getClassesByIds({classIdValue});
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, RepositoryErrorCode::NotFound);

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::NotFound);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(
        result.error().message,
        std::string(
            "Loading class failed for class id "
            + std::to_string(classIdValue)
            + ": no matching record exists."
            )
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
classInfoBatchFailureAbortsSource()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Class details query failure"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    QVERIFY(services.rosterService()->saveRoster(
        classIdValue,
        rosterWithColumns()
        ));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassRepository* const classRepository = store->classRepository();
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(classInfoRepository);
    const ClassRepository::ReadMetrics classMetricsBefore =
        classRepository->readMetrics();
    const auto classInfoMetricsBefore =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();
    const ScheduleClassInfoReadMetrics scheduleReadMetricsBefore =
        classInfoRepository->scheduleClassInfoReadMetrics();

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "ALTER TABLE teachers RENAME COLUMN room_number TO old_room_number"
            )
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        classRepository->readMetrics().getClassesByIdsCallCount
            - classMetricsBefore.getClassesByIdsCallCount,
        1
        );
    QCOMPARE(
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics().callCount
            - classInfoMetricsBefore.callCount,
        1
        );
    QCOMPARE(
        classInfoRepository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        scheduleReadMetricsBefore.singleClassInfoReadCount
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
classInfoBatchRejectsMissingMetadataRecord()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Class without information")
        );
    QVERIFY(createdClass);
    QVERIFY(*createdClass > 0);

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classInfoRepository);

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto source = port.loadSource(requestFor(
        {classId(*createdClass)},
        {SubPrepWeekday::Monday}
        ));
    QVERIFY(source);
    QVERIFY(source.value().classes.empty());

    const auto before =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();
    const auto loaded =
        classInfoRepository->loadSubPrepRosterOutputClassInfoRecords(
            {*createdClass}
            );
    QVERIFY(!loaded);
    QVERIFY(toLegacyDiagnostic(loaded.error())
        .contains(QStringLiteral("no matching record")));

    const auto after =
        classInfoRepository->subPrepRosterOutputClassInfoBatchReadMetrics();
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.requestedClassCount - before.requestedClassCount, 1);
    QCOMPARE(after.statementCount - before.statementCount, 1);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsOutOfBoundRosterAndClassText()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Bounded source"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    const auto savedRoster = services.rosterService()->saveRoster(
        classIdValue,
        rosterWithColumns()
        );
    QVERIFY(savedRoster);

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto request = requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        );
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (%1, 4096, 0, 'outside cap')"
            ).arg(classIdValue)
        ));
    const auto rosterOverflow = port.loadSource(request);
    QVERIFY(!rosterOverflow);
    QCOMPARE(rosterOverflow.error().code, ErrorCode::Validation);

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "DELETE FROM roster_data WHERE class_id=%1 AND row_index=4096"
            ).arg(classIdValue)
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "UPDATE teachers SET zoom_id='%1' WHERE id=%2"
            ).arg(
                QString(257, QLatin1Char('z')),
                QString::number(teacher)
                )
        ));
    const auto oversizedField = port.loadSource(request);
    QVERIFY(!oversizedField);
    QCOMPARE(oversizedField.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsScheduleMeetingOverflow()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int classIdValue = createClass(
        services,
        QStringLiteral("Schedule meeting overflow"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    for (int index = 0; index < 64; ++index)
    {
        QVERIFY(executeSql(
            services,
            QStringLiteral(
                "INSERT INTO class_times (class_id, day, start_time, end_time) "
                "VALUES (%1, 'Monday', '11:00 AM', '12:00 PM')"
                ).arg(classIdValue)
            ));
    }

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classInfoRepository);
    const auto scheduleMetricsBefore =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
    const auto scheduleMetricsAfter =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    QCOMPARE(scheduleMetricsAfter.callCount - scheduleMetricsBefore.callCount, 1);
    QCOMPARE(
        scheduleMetricsAfter.statementCount
            - scheduleMetricsBefore.statementCount,
        1
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsAggregateMeetingOverflow()
{
    constexpr std::size_t classMeetingLimit =
        kSubPrepPrintSourceMaxMeetingsPerClass;
    constexpr std::size_t aggregateMeetingLimit =
        kSubPrepPrintSourceMaxMeetings;
    constexpr std::size_t classCount =
        aggregateMeetingLimit / classMeetingLimit + 1;
    constexpr std::size_t finalClassMeetingCount =
        aggregateMeetingLimit % classMeetingLimit + 1;
    static_assert(classCount <= kSubPrepPrintSourceMaxClassIds);

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    ClassInfoRepository* const classInfoRepository =
        store->classInfoRepository();
    QVERIFY(classInfoRepository);

    constexpr int firstClassId = 700'000;
    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(classCount);
    QStringList classValues;
    classValues.reserve(static_cast<qsizetype>(classCount));
    QStringList classInfoValues;
    classInfoValues.reserve(static_cast<qsizetype>(classCount));
    QStringList scheduleValues;
    scheduleValues.reserve(static_cast<qsizetype>(aggregateMeetingLimit + 1));

    for (std::size_t classIndex = 0; classIndex < classCount; ++classIndex)
    {
        const int id = firstClassId + static_cast<int>(classIndex);
        selectedClassIds.push_back(classId(id));
        classValues.append(QStringLiteral(
            "(%1, 'aggregate-overflow-%2')"
            ).arg(id).arg(classIndex));
        classInfoValues.append(QStringLiteral("(%1, NULL)").arg(id));

        const std::size_t meetingCount = classIndex + 1 == classCount
            ? finalClassMeetingCount
            : classMeetingLimit;
        for (std::size_t meetingIndex = 0;
             meetingIndex < meetingCount;
             ++meetingIndex)
        {
            scheduleValues.append(QStringLiteral(
                "(%1, 'Monday', '9:00 AM', '10:00 AM')"
                ).arg(id));
        }
    }

    QSqlDatabase database = store->database();
    QVERIFY(database.transaction());
    QVERIFY(executeSql(
        services,
        QStringLiteral("INSERT INTO classes (id, name) VALUES %1")
            .arg(classValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id) VALUES %1"
            ).arg(classInfoValues.join(QStringLiteral(", ")))
        ));

    constexpr qsizetype rowsPerStatement = 512;
    for (qsizetype offset = 0; offset < scheduleValues.size();
         offset += rowsPerStatement)
    {
        const QStringList rows = scheduleValues.mid(
            offset,
            rowsPerStatement
            );
        QVERIFY(executeSql(
            services,
            QStringLiteral(
                "INSERT INTO class_times (class_id, day, start_time, end_time) "
                "VALUES %1"
                ).arg(rows.join(QStringLiteral(", ")))
            ));
    }
    QVERIFY(database.commit());

    const auto scheduleMetricsBefore =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        selectedClassIds,
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
    QVERIFY(
        result.error().message.find("total limit") != std::string::npos
        );
    const auto scheduleMetricsAfter =
        classInfoRepository->subPrepRosterOutputScheduleBatchReadMetrics();
    QCOMPARE(scheduleMetricsAfter.callCount - scheduleMetricsBefore.callCount, 1);
    QCOMPARE(
        scheduleMetricsAfter.requestedClassCount
            - scheduleMetricsBefore.requestedClassCount,
        static_cast<int>(classCount)
        );
    QCOMPARE(
        scheduleMetricsAfter.statementCount
            - scheduleMetricsBefore.statementCount,
        1
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsAggregateRosterRowOverflowAcrossClasses()
{
    constexpr std::size_t classCount =
        kSubPrepRosterOutputMaxTotalRows
            / kSubPrepRosterOutputMaxRowsPerClass
        + 1;
    static_assert(classCount <= kSubPrepPrintSourceMaxClassIds);

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);

    constexpr int firstClassId = 800'000;
    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(classCount);
    QStringList classValues;
    classValues.reserve(static_cast<qsizetype>(classCount));
    QStringList classInfoValues;
    classInfoValues.reserve(static_cast<qsizetype>(classCount));
    QStringList scheduleValues;
    scheduleValues.reserve(static_cast<qsizetype>(classCount));
    QStringList rosterColumnValues;
    rosterColumnValues.reserve(static_cast<qsizetype>(classCount * 3));
    QStringList rosterDataValues;
    rosterDataValues.reserve(static_cast<qsizetype>(classCount));

    for (std::size_t index = 0; index < classCount; ++index)
    {
        const int id = firstClassId + static_cast<int>(index);
        selectedClassIds.push_back(classId(id));
        classValues.append(
            QStringLiteral("(%1, 'aggregate-roster-%2')")
                .arg(id)
                .arg(static_cast<qulonglong>(index))
            );
        classInfoValues.append(QStringLiteral("(%1, NULL)").arg(id));
        scheduleValues.append(
            QStringLiteral(
                "(%1, 'Monday', '9:00 AM', '10:00 AM')"
                ).arg(id)
            );
        rosterColumnValues.append(
            QStringLiteral("(%1, 'English', 0, 0)").arg(id)
            );
        rosterColumnValues.append(
            QStringLiteral("(%1, 'Korean', 1, 0)").arg(id)
            );
        rosterColumnValues.append(
            QStringLiteral("(%1, 'Notes', 2, 0)").arg(id)
            );
        rosterDataValues.append(
            QStringLiteral("(%1, 4095, 0, 'last row')").arg(id)
            );
    }

    QSqlDatabase database = store->database();
    QVERIFY(database.transaction());
    QVERIFY(executeSql(
        services,
        QStringLiteral("INSERT INTO classes (id, name) VALUES %1")
            .arg(classValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id) VALUES %1"
            ).arg(classInfoValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES %1"
            ).arg(scheduleValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES %1"
            ).arg(rosterColumnValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES %1"
            ).arg(rosterDataValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(database.commit());

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        selectedClassIds,
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
    QVERIFY(
        result.error().message.find("roster rows exceed the requested limit")
        != std::string::npos
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsAggregateRosterCellOverflowAcrossClasses()
{
    constexpr std::size_t classCount = 2;
    constexpr std::size_t rowsPerClass =
        kSubPrepRosterOutputMaxRowsPerClass;
    constexpr std::size_t requestedColumnCount =
        kSubPrepRosterOutputMaxRosterColumns;
    static_assert(
        classCount * rowsPerClass * requestedColumnCount
        > kSubPrepRosterOutputMaxTotalCells
        );

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);

    constexpr int firstClassId = 900'000;
    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(classCount);
    QStringList classValues;
    QStringList classInfoValues;
    QStringList scheduleValues;
    QStringList rosterColumnValues;
    QStringList rosterDataValues;
    std::vector<std::string> selectedExtraColumns;
    selectedExtraColumns.reserve(requestedColumnCount - 2);
    for (std::size_t column = 0;
         column < requestedColumnCount - 2;
         ++column)
    {
        selectedExtraColumns.push_back(
            "Extra " + std::to_string(column)
            );
    }

    for (std::size_t index = 0; index < classCount; ++index)
    {
        const int id = firstClassId + static_cast<int>(index);
        selectedClassIds.push_back(classId(id));
        classValues.append(
            QStringLiteral("(%1, 'aggregate-cell-%2')")
                .arg(id)
                .arg(static_cast<qulonglong>(index))
            );
        classInfoValues.append(QStringLiteral("(%1, NULL)").arg(id));
        scheduleValues.append(
            QStringLiteral(
                "(%1, 'Monday', '9:00 AM', '10:00 AM')"
                ).arg(id)
            );
        rosterColumnValues.append(
            QStringLiteral("(%1, 'English', 0, 0)").arg(id)
            );
        rosterColumnValues.append(
            QStringLiteral("(%1, 'Korean', 1, 0)").arg(id)
            );
        for (std::size_t column = 0;
             column < selectedExtraColumns.size();
             ++column)
        {
            rosterColumnValues.append(
                QStringLiteral("(%1, 'Extra %2', %3, 0)")
                    .arg(id)
                    .arg(static_cast<qulonglong>(column))
                    .arg(static_cast<qulonglong>(column + 2))
                );
        }
        rosterDataValues.append(
            QStringLiteral("(%1, 4095, 0, 'last row')").arg(id)
            );
    }

    QSqlDatabase database = store->database();
    QVERIFY(database.transaction());
    QVERIFY(executeSql(
        services,
        QStringLiteral("INSERT INTO classes (id, name) VALUES %1")
            .arg(classValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id) VALUES %1"
            ).arg(classInfoValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES %1"
            ).arg(scheduleValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES %1"
            ).arg(rosterColumnValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES %1"
            ).arg(rosterDataValues.join(QStringLiteral(", ")))
        ));
    QVERIFY(database.commit());

    SubPrepRosterOutputSourceRequest request{
        .selectedClassIds = std::move(selectedClassIds),
        .selectedDays = {SubPrepWeekday::Monday},
        .mode = ScheduleViewMode::Regular,
        .selectedExtraColumns = std::move(selectedExtraColumns)
    };
    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(request);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
    QVERIFY(
        result.error().message.find("roster cells exceed the requested limit")
        != std::string::npos
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsAggregateRosterTextOverflowBeforeReturningInput()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int classIdValue = createClass(
        services,
        QStringLiteral("Aggregate text overflow"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (%1, 'English', 0, 0), (%1, 'Korean', 1, 0), "
            "(%1, 'Notes', 2, 0)"
            ).arg(classIdValue)
        ));

    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlQuery rosterData(store->database());
    QVERIFY(rosterData.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, 0, ?)"
        )));
    const QString maximumCell(QString::fromLatin1(
        QByteArray(kSubPrepRosterOutputMaxCellBytes, 'x')
        ));
    constexpr int rowCount =
        static_cast<int>(
            kSubPrepRosterOutputMaxTotalTextBytes
                / kSubPrepRosterOutputMaxCellBytes
            + 1
            );
    QSqlDatabase database = store->database();
    QVERIFY(database.transaction());
    for (int row = 0; row < rowCount; ++row)
    {
        rosterData.bindValue(0, classIdValue);
        rosterData.bindValue(1, row);
        rosterData.bindValue(2, maximumCell);
        QVERIFY2(
            rosterData.exec(),
            qPrintable(rosterData.lastError().text())
            );
    }
    QVERIFY(database.commit());

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
    QVERIFY(
        result.error().message.find("cell exceeds the text byte limit")
        != std::string::npos
        );
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests)

#include "next_platform_application_services_sub_prep_roster_output_source_port_tests.moc"
