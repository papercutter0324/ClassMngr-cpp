#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/database/sql_query_utils.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/platform/application_services_class_co_teacher_assignment_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("co-teacher-assignment-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created ? *created : -1;
}

int createTeacher(ApplicationServices& services, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto saved = services.teacherService()->save(teacher);
    return saved ? *saved : -1;
}

Domain::ClassId asClassId(const std::int64_t value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Domain::TeacherId asTeacherId(const std::int64_t value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

ClassInfo newClassInfo(
    ApplicationServices& services,
    const int classId,
    const int teacherId = -1
    )
{
    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    if (!repository)
    {
        qFatal("Class info repository must be available in test setup.");
    }

    const auto loaded = repository->loadClassInfo(classId);
    if (!loaded)
    {
        qFatal("Class info must load during test setup.");
    }

    ClassInfo info = *loaded;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook.clear();
    info.essayBook.clear();
    info.classColor = QStringLiteral("#123456");
    info.fontColor = QStringLiteral("#654321");
    info.notes = QStringLiteral("Preserved class notes");
    info.timeFillerActivities = QStringLiteral("Preserved activities");
    return info;
}

bool saveClassInfo(
    ApplicationServices& services,
    const ClassInfo& info,
    QString* failureMessage = nullptr
    )
{
    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    if (!repository)
    {
        if (failureMessage)
        {
            *failureMessage = QStringLiteral("Class info repository unavailable");
        }
        return false;
    }

    const RepositoryStatus saved = repository->saveClassInfo(info);
    if (!saved && failureMessage)
    {
        *failureMessage = toLegacyDiagnostic(saved.error());
    }
    return saved.has_value();
}

bool sameTimes(const QList<ClassTime>& left, const QList<ClassTime>& right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (qsizetype index = 0; index < left.size(); ++index)
    {
        if (left[index].day != right[index].day
            || left[index].startTime != right[index].startTime
            || left[index].endTime != right[index].endTime)
        {
            return false;
        }
    }

    return true;
}

void copyTeacherMetadata(ClassInfo& info, const Teacher& teacher)
{
    info.teacherKr = teacher.teacherKr;
    info.teacherEn = teacher.teacherEn;
    info.teacherPreferredName = teacher.preferredName;
    info.roomNumber = teacher.roomNumber;
    info.wifiName = teacher.wifiName;
    info.wifiPassword = teacher.wifiPassword;
    info.internetType = teacher.internetType;
    info.zoomId = teacher.zoomId;
    info.zoomPassword = teacher.zoomPassword;
    info.projectionType = teacher.projectionType;
}

bool sameTeacherMetadata(const ClassInfo& actual, const ClassInfo& expected)
{
    return actual.teacherKr == expected.teacherKr
        && actual.teacherEn == expected.teacherEn
        && actual.teacherPreferredName == expected.teacherPreferredName
        && actual.roomNumber == expected.roomNumber
        && actual.wifiName == expected.wifiName
        && actual.wifiPassword == expected.wifiPassword
        && actual.internetType == expected.internetType
        && actual.zoomId == expected.zoomId
        && actual.zoomPassword == expected.zoomPassword
        && actual.projectionType == expected.projectionType;
}

bool teacherMetadataIsEmpty(const ClassInfo& actual)
{
    const ClassInfo emptyMetadata;
    return sameTeacherMetadata(actual, emptyMetadata);
}

bool sameStoredClassFieldsExceptTeacherId(
    const ClassInfo& actual,
    const ClassInfo& expected
    )
{
    return actual.classId == expected.classId
        && actual.classGrade == expected.classGrade
        && actual.classLevel == expected.classLevel
        && actual.readingBook == expected.readingBook
        && actual.essayBook == expected.essayBook
        && actual.classColor == expected.classColor
        && actual.fontColor == expected.fontColor
        && actual.notes == expected.notes
        && actual.timeFillerActivities == expected.timeFillerActivities
        && sameTimes(actual.classTimes, expected.classTimes)
        && sameTimes(actual.intensiveTimes, expected.intensiveTimes);
}

bool sameCompleteClassInfo(const ClassInfo& actual, const ClassInfo& expected)
{
    return actual.teacherId == expected.teacherId
        && sameStoredClassFieldsExceptTeacherId(actual, expected)
        && sameTeacherMetadata(actual, expected);
}

QVariantList rawClassInfoFields(QSqlDatabase database, const int classId)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT class_grade, class_level, reading_book, essay_book, "
        "class_color, font_color, notes, time_filler_activities "
        "FROM class_info WHERE class_id=?"
        ));
    query.addBindValue(classId);
    if (!query.exec() || !query.next())
    {
        qFatal("Raw class info row must be readable in test setup.");
    }

    QVariantList values;
    for (int column = 0; column < query.record().count(); ++column)
    {
        values.append(query.value(column));
    }
    return values;
}

QList<QVariantList> rawScheduleRows(
    QSqlDatabase database,
    const int classId,
    const QString& tableName
    )
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT id, day, start_time, end_time FROM %1 "
        "WHERE class_id=? ORDER BY id"
        ).arg(tableName));
    query.addBindValue(classId);
    if (!query.exec())
    {
        qFatal("Raw schedule rows must be readable in test setup.");
    }

    QList<QVariantList> rows;
    while (query.next())
    {
        QVariantList row;
        for (int column = 0; column < query.record().count(); ++column)
        {
            row.append(query.value(column));
        }
        rows.append(row);
    }
    return rows;
}

QList<QVariantList> rawJsonScheduleRows(
    QSqlDatabase database,
    const QString& tableName
    )
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT id, class_id, payload FROM %1 ORDER BY id"
            ).arg(tableName)))
    {
        qFatal("Raw JSON schedule rows must be readable in test setup.");
    }

    QList<QVariantList> rows;
    while (query.next())
    {
        QVariantList row;
        for (int column = 0; column < query.record().count(); ++column)
        {
            row.append(query.value(column));
        }
        rows.append(row);
    }
    return rows;
}

int scheduleWriteAuditCount(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM schedule_write_audit"))
        || !query.next())
    {
        qFatal("Schedule write audit count must be readable in test setup.");
    }
    return query.value(0).toInt();
}

QVariant readRawTeacherId(QSqlDatabase database, const int classId)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT teacher_id FROM class_info WHERE class_id=?"
        ));
    query.addBindValue(classId);
    if (!query.exec() || !query.next())
    {
        qFatal("Raw class teacher id must be readable in test setup.");
    }
    return query.value(0);
}

bool installMalformedJsonScheduleView(
    QSqlDatabase database,
    const QString& scheduleTable,
    const QString& backingTable,
    const int classId,
    const QString& validPayload,
    QString* failureMessage
    )
{
    auto execute = [&](const QString& statement)
    {
        QSqlQuery query(database);
        if (query.exec(statement))
        {
            return true;
        }

        if (failureMessage)
        {
            *failureMessage = query.lastError().text();
        }
        return false;
    };

    if (!execute(QStringLiteral("DROP TABLE %1").arg(scheduleTable))
        || !execute(QStringLiteral(
            "CREATE TABLE %1 ("
            "id INTEGER PRIMARY KEY, class_id INTEGER, payload TEXT NOT NULL)"
            ).arg(backingTable)))
    {
        return false;
    }

    for (int id = 1; id <= 2; ++id)
    {
        QSqlQuery insertRow(database);
        insertRow.prepare(QStringLiteral(
            "INSERT INTO %1 (id, class_id, payload) VALUES (?, ?, ?)"
            ).arg(backingTable));
        insertRow.addBindValue(id);
        insertRow.addBindValue(classId);
        insertRow.addBindValue(
            id == 1 ? validPayload : QStringLiteral("{ malformed json")
            );
        if (!insertRow.exec())
        {
            if (failureMessage)
            {
                *failureMessage = insertRow.lastError().text();
            }
            return false;
        }
    }

    return execute(QStringLiteral(
        "CREATE VIEW %1 AS "
        "SELECT id, class_id, "
        "json_extract(payload, '$.day') AS day, "
        "json_extract(payload, '$.start_time') AS start_time, "
        "json_extract(payload, '$.end_time') AS end_time "
        "FROM %2"
        ).arg(scheduleTable, backingTable));
}

}

class NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void assignmentUpdatesOnlyTeacherAndSupportsUnassignedState();
    void malformedClassIdIsRejectedWithoutChangingPersistedFields();
    void invalidLoadedClassInfoIsRejectedWithoutWriting();
    void regularScheduleConflictUsesLegacyMessageAndPreservesState();
    void intensiveScheduleConflictUsesLegacyMessageAndPreservesState();
    void regularConflictPrecedesIntensiveConflict();
    void assignmentPreservesRawMetadataAndScheduleRows();
    void assignmentCreatesMissingClassInfoRowWithDefaults();
    void missingTeacherReturnsRecoverableNotFoundWithoutMutation();
    void teacherIdStatementFailurePreservesClassInfoAndSchedules();
    void validationSnapshotScheduleQueryFailureReturnsTechnicalWithoutMutation();
    void regularScheduleLateFetchFailureReturnsTechnicalWithoutMutation();
    void intensiveScheduleLateFetchFailureReturnsTechnicalWithoutMutation();
    void unavailableSessionReturnsStructuredFailure();
};

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
assignmentUpdatesOnlyTeacherAndSupportsUnassignedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Co-Teacher Assignment Test")
        );
    QVERIFY(createdClass);

    const int firstTeacherId = createTeacher(
        services,
        QStringLiteral("First Teacher")
        );
    const int secondTeacherId = createTeacher(
        services,
        QStringLiteral("Second Teacher")
        );
    Teacher secondTeacher;
    secondTeacher.teacherEn = QStringLiteral("Second Teacher");
    secondTeacher.preferredName = secondTeacher.teacherEn;
    QVERIFY(firstTeacherId > 0);
    QVERIFY(secondTeacherId > 0);

    const auto loaded = services.classService()->classInfo(*createdClass);
    QVERIFY(loaded);
    ClassInfo original = *loaded;
    original.teacherId = firstTeacherId;
    original.classGrade = QStringLiteral("E4");
    original.classLevel = QStringLiteral("Theseus");
    original.classColor = QStringLiteral("#123456");
    original.fontColor = QStringLiteral("#654321");
    original.notes = QStringLiteral("Preserved class notes");
    original.timeFillerActivities = QStringLiteral("Preserved activities");
    original.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:50 AM")},
        {QStringLiteral("Wednesday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:50 AM")}
    };
    original.intensiveTimes = {
        {QStringLiteral("Friday"), QStringLiteral("10:00 AM"),
         QStringLiteral("10:50 AM")}
    };
    const Status initialSave = services.classService()->saveClassInfo(original);
    QVERIFY(initialSave);

    const auto persistedOriginal = services.workspaceStore()
        ->classInfoRepository()
        ->loadClassInfo(*createdClass);
    QVERIFY(persistedOriginal);

    ClassInfoRepository* const repository = services.workspaceStore()
        ->classInfoRepository();
    QVERIFY(repository);
    const int readsBeforeAssignment = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;
    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto assigned = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(*createdClass),
        asTeacherId(secondTeacherId),
        port
        );
    QVERIFY(assigned);
    QCOMPARE(
        repository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        readsBeforeAssignment
        );

    auto afterAssign = services.classService()->classInfo(*createdClass);
    QVERIFY(afterAssign);
    QCOMPARE(afterAssign->teacherId, secondTeacherId);
    ClassInfo expectedAssigned = *persistedOriginal;
    copyTeacherMetadata(expectedAssigned, secondTeacher);
    QVERIFY(sameStoredClassFieldsExceptTeacherId(
        *afterAssign,
        *persistedOriginal
        ));
    QVERIFY(sameTeacherMetadata(*afterAssign, expectedAssigned));

    const int readsBeforeUnassignment = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;
    const auto unassigned =
        Application::ClassCoTeacherAssignmentUseCase::execute(
            asClassId(*createdClass),
            asTeacherId(-1),
            port
            );
    QVERIFY(unassigned);
    QCOMPARE(
        repository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        readsBeforeUnassignment
        );
    QVERIFY(readRawTeacherId(
        services.workspaceStore()->database(),
        *createdClass
        ).isNull());

    const auto afterUnassign = services.classService()->classInfo(*createdClass);
    QVERIFY(afterUnassign);
    QCOMPARE(afterUnassign->teacherId, -1);
    QVERIFY(sameStoredClassFieldsExceptTeacherId(
        *afterUnassign,
        *persistedOriginal
        ));
    QVERIFY(teacherMetadataIsEmpty(*afterUnassign));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
malformedClassIdIsRejectedWithoutChangingPersistedFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Malformed Co-Teacher Assignment ID")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Same Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Same Teacher")
        );
    QVERIFY(classId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        classId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(classId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const std::vector<std::string> malformedIds{
        "0",
        "-1",
        "+1",
        "1x"
    };
    for (const std::string& malformedId : malformedIds)
    {
        const auto typedClassId = Domain::ClassId::fromString(malformedId);
        QVERIFY(typedClassId);
        const auto result = port.assignClassCoTeacher({
            .classId = *typedClassId,
            .teacherId = Domain::TeacherId::fromString(
                std::to_string(assignedTeacherId)
                )
        });
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    const auto zeroTeacherId = Domain::TeacherId::fromString("0");
    QVERIFY(zeroTeacherId);
    const auto invalidTeacher = port.assignClassCoTeacher({
        .classId = *Domain::ClassId::fromString(std::to_string(classId)),
        .teacherId = *zeroTeacherId
    });
    QVERIFY(!invalidTeacher);
    QCOMPARE(invalidTeacher.error().code, Domain::ErrorCode::InvalidInput);

    const int unknownClassId = classId + 100000;
    const QString unknownClassDiagnostic = QStringLiteral(
        "Loading class information failed for class id %1: class not found."
        ).arg(unknownClassId);
    const auto legacyUnknownClass =
        repository->loadClassCoTeacherAssignmentValidationSnapshot(
            unknownClassId
            );
    QVERIFY(!legacyUnknownClass);
    QCOMPARE(
        toLegacyDiagnostic(legacyUnknownClass.error()),
        unknownClassDiagnostic
        );

    const auto detailedUnknownClass =
        repository->loadClassCoTeacherAssignmentValidationSnapshotDetailed(
            unknownClassId
            );
    QVERIFY(!detailedUnknownClass);
    QCOMPARE(
        detailedUnknownClass.error().code,
        RepositoryErrorCode::NotFound
        );
    QCOMPARE(
        toLegacyDiagnostic(detailedUnknownClass.error()),
        unknownClassDiagnostic
        );

    const auto missingParentSave =
        repository->saveClassCoTeacherAssignmentDetailed(
            unknownClassId,
            static_cast<qint64>(assignedTeacherId)
            );
    QVERIFY(!missingParentSave);
    QCOMPARE(missingParentSave.error().code, RepositoryErrorCode::NotFound);
    QVERIFY(missingParentSave.error().recoverableOverride.has_value());
    QVERIFY(!*missingParentSave.error().recoverableOverride);
    QCOMPARE(
        toLegacyDiagnostic(missingParentSave.error()),
        QStringLiteral(
            "Saving class information failed for class id %1: "
            "FOREIGN KEY constraint failed Unable to fetch row "
            "(database error 787)"
            ).arg(unknownClassId)
        );

    const auto unknownClass =
        Application::ClassCoTeacherAssignmentUseCase::execute(
            asClassId(unknownClassId),
            asTeacherId(assignedTeacherId),
            port
            );
    QVERIFY(!unknownClass);
    QCOMPARE(unknownClass.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!unknownClass.error().recoverable);
    QCOMPARE(
        QString::fromStdString(unknownClass.error().message),
        unknownClassDiagnostic
        );

    const auto savedAfter = repository->loadClassInfo(classId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));

    const auto leadingZeroClassId = Domain::ClassId::fromString(
        "0" + std::to_string(classId)
        );
    QVERIFY(leadingZeroClassId);
    const auto legacyPositiveIdResult = port.assignClassCoTeacher({
        .classId = *leadingZeroClassId,
        .teacherId = Domain::TeacherId::fromString(
            std::to_string(assignedTeacherId)
            )
    });
    QVERIFY(legacyPositiveIdResult);

    const auto savedAfterLegacyPositiveId = repository->loadClassInfo(classId);
    QVERIFY(savedAfterLegacyPositiveId);
    QVERIFY(sameStoredClassFieldsExceptTeacherId(
        *savedAfterLegacyPositiveId,
        *savedBefore
        ));
    QCOMPARE(savedAfterLegacyPositiveId->teacherId, assignedTeacherId);
    QVERIFY(sameTeacherMetadata(*savedAfterLegacyPositiveId, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
invalidLoadedClassInfoIsRejectedWithoutWriting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Invalid Co-Teacher Assignment State")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(classId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        classId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    const QString invalidNotes(10001, QChar(u'x'));
    QSqlQuery corruptStoredNotes(services.workspaceStore()->database());
    corruptStoredNotes.prepare(QStringLiteral(
        "UPDATE class_info SET notes=? WHERE class_id=?"
        ));
    corruptStoredNotes.addBindValue(invalidNotes);
    corruptStoredNotes.addBindValue(classId);
    QVERIFY2(
        corruptStoredNotes.exec(),
        qPrintable(corruptStoredNotes.lastError().text())
        );
    QCOMPARE(corruptStoredNotes.numRowsAffected(), 1);

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(classId);
    QVERIFY(savedBefore);
    QCOMPARE(savedBefore->notes, invalidNotes);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(classId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString message = QString::fromStdString(result.error().message);
    QVERIFY(message.contains(
        QStringLiteral("Class information validation failed")
        ));
    QVERIFY(message.contains(QStringLiteral("notes")));

    const auto savedAfter = repository->loadClassInfo(classId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
regularScheduleConflictUsesLegacyMessageAndPreservesState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int conflictSourceId = createClass(
        services,
        QStringLiteral("Regular Conflict Source")
        );
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Regular Conflict Selected")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(conflictSourceId > 0);
    QVERIFY(selectedClassId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo source = newClassInfo(services, conflictSourceId);
    source.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, source));

    ClassInfo selected = newClassInfo(services, selectedClassId);
    selected.classTimes = source.classTimes;
    QVERIFY(saveClassInfo(services, selected));

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Class schedule conflict: Monday 9:00 AM\u20139:50 AM conflicts with "
            "Regular Conflict Source."
            )
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
intensiveScheduleConflictUsesLegacyMessageAndPreservesState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int conflictSourceId = createClass(
        services,
        QStringLiteral("Intensive Conflict Source")
        );
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Intensive Conflict Selected")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(conflictSourceId > 0);
    QVERIFY(selectedClassId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo source = newClassInfo(services, conflictSourceId);
    source.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, source));

    ClassInfo selected = newClassInfo(services, selectedClassId);
    selected.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:10 AM"),
        QStringLiteral("10:40 AM")
    }};
    QString saveError;
    QVERIFY2(
        saveClassInfo(services, selected, &saveError),
        qPrintable(saveError)
        );

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Class schedule conflict: Friday 10:10 AM\u201310:40 AM conflicts with "
            "Intensive Conflict Source."
            )
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
regularConflictPrecedesIntensiveConflict()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int conflictSourceId = createClass(
        services,
        QStringLiteral("Both Schedule Conflict Source")
        );
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Both Schedule Conflict Selected")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(conflictSourceId > 0);
    QVERIFY(selectedClassId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo source = newClassInfo(services, conflictSourceId);
    source.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    source.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, source));

    ClassInfo selected = newClassInfo(services, selectedClassId);
    selected.classTimes = source.classTimes;
    selected.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:10 AM"),
        QStringLiteral("10:40 AM")
    }};
    QVERIFY(saveClassInfo(services, selected));

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Class schedule conflict: Monday 9:00 AM\u20139:50 AM conflicts with "
            "Both Schedule Conflict Source."
            )
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
assignmentPreservesRawMetadataAndScheduleRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Raw Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery insertInfo(database);
    insertInfo.prepare(QStringLiteral(
        "INSERT INTO class_info (class_id, teacher_id, class_grade, "
        "class_level, reading_book, essay_book, class_color, font_color, "
        "notes, time_filler_activities) "
        "VALUES (?, ?, NULL, '', '  ', NULL, '', NULL, '   ', '')"
        ));
    insertInfo.addBindValue(selectedClassId);
    insertInfo.addBindValue(originalTeacherId);
    QVERIFY2(insertInfo.exec(), qPrintable(insertInfo.lastError().text()));

    QSqlQuery insertRegular(database);
    insertRegular.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, ?, ?, ?)"
        ));
    insertRegular.addBindValue(selectedClassId);
    insertRegular.addBindValue(QStringLiteral("Monday"));
    insertRegular.addBindValue(QStringLiteral("9:00 AM"));
    insertRegular.addBindValue(QStringLiteral("9:50 AM"));
    QVERIFY2(insertRegular.exec(), qPrintable(insertRegular.lastError().text()));

    QSqlQuery insertIntensive(database);
    insertIntensive.prepare(QStringLiteral(
        "INSERT INTO class_intensive_times "
        "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
        ));
    insertIntensive.addBindValue(selectedClassId);
    insertIntensive.addBindValue(QStringLiteral("Friday"));
    insertIntensive.addBindValue(QStringLiteral("10:00 AM"));
    insertIntensive.addBindValue(QStringLiteral("10:50 AM"));
    QVERIFY2(
        insertIntensive.exec(),
        qPrintable(insertIntensive.lastError().text())
        );

    QSqlQuery createScheduleWriteAudit(database);
    QVERIFY2(
        createScheduleWriteAudit.exec(QStringLiteral(
            "CREATE TABLE schedule_write_audit ("
            "table_name TEXT NOT NULL, operation TEXT NOT NULL)"
            )),
        qPrintable(createScheduleWriteAudit.lastError().text())
        );
    const QStringList auditedTables = {
        QStringLiteral("class_times"),
        QStringLiteral("class_intensive_times")
    };
    const QStringList auditedOperations = {
        QStringLiteral("INSERT"),
        QStringLiteral("UPDATE"),
        QStringLiteral("DELETE")
    };
    for (const QString& tableName : auditedTables)
    {
        for (const QString& operation : auditedOperations)
        {
            const QString triggerName = QStringLiteral("audit_%1_%2")
                .arg(tableName, operation.toLower());
            QSqlQuery installAuditTrigger(database);
            QVERIFY2(
                installAuditTrigger.exec(QStringLiteral(
                    "CREATE TRIGGER %1 AFTER %2 ON %3 "
                    "BEGIN INSERT INTO schedule_write_audit "
                    "(table_name, operation) VALUES ('%3', '%2'); END"
                    ).arg(triggerName, operation, tableName)),
                qPrintable(installAuditTrigger.lastError().text())
                );
        }
    }
    QCOMPARE(scheduleWriteAuditCount(database), 0);

    const QVariantList rawInfoBefore = rawClassInfoFields(
        database,
        selectedClassId
        );
    QVERIFY(rawInfoBefore.at(0).isNull());
    QCOMPARE(rawInfoBefore.at(1).toString(), QString());
    QCOMPARE(rawInfoBefore.at(2).toString(), QStringLiteral("  "));
    QVERIFY(rawInfoBefore.at(3).isNull());
    QCOMPARE(rawInfoBefore.at(4).toString(), QString());
    QVERIFY(rawInfoBefore.at(5).isNull());
    QCOMPARE(rawInfoBefore.at(6).toString(), QStringLiteral("   "));
    QCOMPARE(rawInfoBefore.at(7).toString(), QString());
    const QList<QVariantList> rawRegularBefore = rawScheduleRows(
        database,
        selectedClassId,
        QStringLiteral("class_times")
        );
    const QList<QVariantList> rawIntensiveBefore = rawScheduleRows(
        database,
        selectedClassId,
        QStringLiteral("class_intensive_times")
        );
    QVERIFY(!rawRegularBefore.isEmpty());
    QVERIFY(!rawIntensiveBefore.isEmpty());

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const int readsBefore = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;
    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(result);
    QCOMPARE(
        repository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        readsBefore
        );
    QCOMPARE(scheduleWriteAuditCount(database), 0);

    QCOMPARE(
        rawClassInfoFields(database, selectedClassId),
        rawInfoBefore
        );
    QCOMPARE(readRawTeacherId(database, selectedClassId).toInt(), assignedTeacherId);
    QCOMPARE(
        rawScheduleRows(database, selectedClassId, QStringLiteral("class_times")),
        rawRegularBefore
        );
    QCOMPARE(
        rawScheduleRows(
            database,
            selectedClassId,
            QStringLiteral("class_intensive_times")
            ),
        rawIntensiveBefore
        );

    const int readsBeforeUnassignment = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;
    const auto unassigned = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(-1),
        port
        );
    QVERIFY(unassigned);
    QCOMPARE(
        repository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        readsBeforeUnassignment
        );
    QCOMPARE(scheduleWriteAuditCount(database), 0);
    QVERIFY(readRawTeacherId(database, selectedClassId).isNull());
    QCOMPARE(
        rawClassInfoFields(database, selectedClassId),
        rawInfoBefore
        );
    QCOMPARE(
        rawScheduleRows(database, selectedClassId, QStringLiteral("class_times")),
        rawRegularBefore
        );
    QCOMPARE(
        rawScheduleRows(
            database,
            selectedClassId,
            QStringLiteral("class_intensive_times")
            ),
        rawIntensiveBefore
        );
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
assignmentCreatesMissingClassInfoRowWithDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Missing Co-Teacher Metadata")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(classId > 0);
    QVERIFY(assignedTeacherId > 0);

    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery beforeCount(database);
    QVERIFY(beforeCount.exec(QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_id=%1"
        ).arg(classId)));
    QVERIFY(beforeCount.next());
    QCOMPARE(beforeCount.value(0).toInt(), 0);

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const int readsBefore = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;
    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto assigned = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(classId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(assigned);
    QCOMPARE(
        repository->scheduleClassInfoReadMetrics().singleClassInfoReadCount,
        readsBefore
        );

    QSqlQuery after(database);
    after.prepare(QStringLiteral(
        "SELECT teacher_id, class_grade, class_level, reading_book, "
        "essay_book, class_color, font_color, notes, time_filler_activities "
        "FROM class_info WHERE class_id=?"
        ));
    after.addBindValue(classId);
    QVERIFY2(after.exec(), qPrintable(after.lastError().text()));
    QVERIFY(after.next());
    QCOMPARE(after.value(0).toInt(), assignedTeacherId);
    QVERIFY(after.value(1).isNull());
    QVERIFY(after.value(2).isNull());
    QVERIFY(after.value(3).isNull());
    QVERIFY(after.value(4).isNull());
    QCOMPARE(after.value(5).toString(), QStringLiteral("#FFFFFF"));
    QCOMPARE(after.value(6).toString(), QStringLiteral("#000000"));
    QVERIFY(after.value(7).isNull());
    QVERIFY(after.value(8).isNull());
    QVERIFY(rawScheduleRows(database, classId, QStringLiteral("class_times"))
        .isEmpty());
    QVERIFY(rawScheduleRows(
        database,
        classId,
        QStringLiteral("class_intensive_times")
        ).isEmpty());
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
missingTeacherReturnsRecoverableNotFoundWithoutMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Missing Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        selectedClassId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    original.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);
    QSqlDatabase database = services.workspaceStore()->database();
    const QVariantList rawInfoBefore = rawClassInfoFields(
        database,
        selectedClassId
        );
    const QList<QVariantList> regularBefore = rawScheduleRows(
        database,
        selectedClassId,
        QStringLiteral("class_times")
        );
    const QList<QVariantList> intensiveBefore = rawScheduleRows(
        database,
        selectedClassId,
        QStringLiteral("class_intensive_times")
        );

    const qint64 missingTeacherId = std::numeric_limits<qint64>::max();
    QSqlQuery checkTeacher(database);
    checkTeacher.prepare(QStringLiteral(
        "SELECT 1 FROM teachers WHERE id=?"
        ));
    checkTeacher.addBindValue(missingTeacherId);
    QVERIFY2(checkTeacher.exec(), qPrintable(checkTeacher.lastError().text()));
    QVERIFY(!checkTeacher.next());

    const auto typedSave = repository->saveClassCoTeacherAssignmentDetailed(
        selectedClassId,
        missingTeacherId
        );
    QVERIFY(!typedSave);
    QCOMPARE(typedSave.error().code, RepositoryErrorCode::NotFound);
    QVERIFY(typedSave.error().recoverableOverride.has_value());
    QVERIFY(*typedSave.error().recoverableOverride);
    QCOMPARE(
        toLegacyDiagnostic(typedSave.error()),
        QStringLiteral(
            "Saving class information failed: teacher id %1 was not found."
            ).arg(missingTeacherId)
        );

    QSqlQuery legacyDiagnosticQuery(database);
    legacyDiagnosticQuery.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id
        )
        VALUES (?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id
    )");
    legacyDiagnosticQuery.addBindValue(selectedClassId);
    legacyDiagnosticQuery.addBindValue(missingTeacherId);
    const auto legacyExpectedDiagnostic = SqlQueryUtils::executePrepared(
        legacyDiagnosticQuery,
        QObject::tr("Saving class information"),
        QObject::tr("class id %1").arg(selectedClassId)
        );
    QVERIFY(!legacyExpectedDiagnostic);

    const RepositoryStatus legacySave = repository->saveClassCoTeacherAssignment(
        selectedClassId,
        missingTeacherId
        );
    QVERIFY(!legacySave);
    QCOMPARE(
        toLegacyDiagnostic(legacySave.error()),
        legacyExpectedDiagnostic.error().userMessage()
        );

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(missingTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        toLegacyDiagnostic(typedSave.error())
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
    QCOMPARE(rawClassInfoFields(database, selectedClassId), rawInfoBefore);
    QCOMPARE(
        rawScheduleRows(
            database,
            selectedClassId,
            QStringLiteral("class_times")
            ),
        regularBefore
        );
    QCOMPARE(
        rawScheduleRows(
            database,
            selectedClassId,
            QStringLiteral("class_intensive_times")
            ),
        intensiveBefore
        );
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
teacherIdStatementFailurePreservesClassInfoAndSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Transactional Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        selectedClassId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    original.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    ClassInfoRepository* const repository =
        services.workspaceStore()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    QSqlQuery installFailure(services.workspaceStore()->database());
    QVERIFY2(
        installFailure.exec(QStringLiteral(
            "CREATE TRIGGER fail_co_teacher_teacher_id_update "
            "BEFORE UPDATE OF teacher_id ON class_info "
            "BEGIN SELECT RAISE(FAIL, 'forced co-teacher update failure'); END"
            )),
        qPrintable(installFailure.lastError().text())
        );

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("forced co-teacher update failure")
        ));

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
validationSnapshotScheduleQueryFailureReturnsTechnicalWithoutMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Snapshot Read Failure Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        selectedClassId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    QSqlDatabase database = services.workspaceStore()->database();
    const QVariantList persistedFieldsBefore = rawClassInfoFields(
        database,
        selectedClassId
        );
    const QVariant persistedTeacherIdBefore = readRawTeacherId(
        database,
        selectedClassId
        );

    QSqlQuery removeRequiredScheduleTable(database);
    QVERIFY2(
        removeRequiredScheduleTable.exec(QStringLiteral(
            "DROP TABLE class_times"
            )),
        qPrintable(removeRequiredScheduleTable.lastError().text())
        );

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("Loading regular class times failed")
        ));

    QCOMPARE(
        rawClassInfoFields(database, selectedClassId),
        persistedFieldsBefore
        );
    QCOMPARE(readRawTeacherId(database, selectedClassId), persistedTeacherIdBefore);
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
regularScheduleLateFetchFailureReturnsTechnicalWithoutMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Regular Late Fetch Failure Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        selectedClassId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    QSqlDatabase database = services.workspaceStore()->database();
    const QVariantList persistedFieldsBefore = rawClassInfoFields(
        database,
        selectedClassId
        );
    const QVariant persistedTeacherIdBefore = readRawTeacherId(
        database,
        selectedClassId
        );

    QString fixtureFailure;
    QVERIFY2(
        installMalformedJsonScheduleView(
            database,
            QStringLiteral("class_times"),
            QStringLiteral("f934_regular_json_schedule_rows"),
            selectedClassId,
            QStringLiteral(
                R"({"day":"Monday","start_time":"9:00 AM","end_time":"9:50 AM"})"
                ),
            &fixtureFailure
            ),
        qPrintable(fixtureFailure)
        );

    QSqlQuery lateFetchProbe(database);
    lateFetchProbe.prepare(QStringLiteral(
        "SELECT day, start_time, end_time FROM class_times "
        "WHERE class_id = ? ORDER BY id"
        ));
    lateFetchProbe.addBindValue(selectedClassId);
    QVERIFY2(lateFetchProbe.exec(), qPrintable(lateFetchProbe.lastError().text()));
    QVERIFY(lateFetchProbe.next());
    QCOMPARE(lateFetchProbe.value(0).toString(), QStringLiteral("Monday"));
    QCOMPARE(lateFetchProbe.value(1).toString(), QStringLiteral("9:00 AM"));
    QCOMPARE(lateFetchProbe.value(2).toString(), QStringLiteral("9:50 AM"));
    QVERIFY(!lateFetchProbe.next());
    const QSqlError lateFetchError = lateFetchProbe.lastError();
    QVERIFY2(lateFetchError.isValid(), qPrintable(lateFetchError.text()));
    QVERIFY(lateFetchError.text().contains(
        QStringLiteral("malformed"),
        Qt::CaseInsensitive
        ));
    lateFetchProbe.finish();
    const QList<QVariantList> jsonScheduleRowsBefore = rawJsonScheduleRows(
        database,
        QStringLiteral("f934_regular_json_schedule_rows")
        );

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("Loading regular class times failed")
        ));

    QCOMPARE(
        rawClassInfoFields(database, selectedClassId),
        persistedFieldsBefore
        );
    QCOMPARE(readRawTeacherId(database, selectedClassId), persistedTeacherIdBefore);
    QCOMPARE(
        rawJsonScheduleRows(
            database,
            QStringLiteral("f934_regular_json_schedule_rows")
            ),
        jsonScheduleRowsBefore
        );
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
intensiveScheduleLateFetchFailureReturnsTechnicalWithoutMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Intensive Late Fetch Failure Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        selectedClassId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    QSqlDatabase database = services.workspaceStore()->database();
    const QVariantList persistedFieldsBefore = rawClassInfoFields(
        database,
        selectedClassId
        );
    const QVariant persistedTeacherIdBefore = readRawTeacherId(
        database,
        selectedClassId
        );

    QSqlQuery regularScheduleProbe(database);
    regularScheduleProbe.prepare(QStringLiteral(
        "SELECT day, start_time, end_time FROM class_times "
        "WHERE class_id = ? ORDER BY id"
        ));
    regularScheduleProbe.addBindValue(selectedClassId);
    QVERIFY2(
        regularScheduleProbe.exec(),
        qPrintable(regularScheduleProbe.lastError().text())
        );
    QVERIFY(regularScheduleProbe.next());
    QCOMPARE(regularScheduleProbe.value(0).toString(), QStringLiteral("Monday"));
    QCOMPARE(regularScheduleProbe.value(1).toString(), QStringLiteral("9:00 AM"));
    QCOMPARE(regularScheduleProbe.value(2).toString(), QStringLiteral("9:50 AM"));
    QVERIFY(!regularScheduleProbe.next());
    QVERIFY(!regularScheduleProbe.lastError().isValid());
    regularScheduleProbe.finish();
    const QList<QVariantList> regularScheduleRowsBefore = rawScheduleRows(
        database,
        selectedClassId,
        QStringLiteral("class_times")
        );

    QString fixtureFailure;
    QVERIFY2(
        installMalformedJsonScheduleView(
            database,
            QStringLiteral("class_intensive_times"),
            QStringLiteral("f934_intensive_json_schedule_rows"),
            selectedClassId,
            QStringLiteral(
                R"({"day":"Friday","start_time":"10:00 AM","end_time":"10:50 AM"})"
                ),
            &fixtureFailure
            ),
        qPrintable(fixtureFailure)
        );

    QSqlQuery lateFetchProbe(database);
    lateFetchProbe.prepare(QStringLiteral(
        "SELECT day, start_time, end_time FROM class_intensive_times "
        "WHERE class_id = ? ORDER BY id"
        ));
    lateFetchProbe.addBindValue(selectedClassId);
    QVERIFY2(lateFetchProbe.exec(), qPrintable(lateFetchProbe.lastError().text()));
    QVERIFY(lateFetchProbe.next());
    QCOMPARE(lateFetchProbe.value(0).toString(), QStringLiteral("Friday"));
    QCOMPARE(lateFetchProbe.value(1).toString(), QStringLiteral("10:00 AM"));
    QCOMPARE(lateFetchProbe.value(2).toString(), QStringLiteral("10:50 AM"));
    QVERIFY(!lateFetchProbe.next());
    const QSqlError lateFetchError = lateFetchProbe.lastError();
    QVERIFY2(lateFetchError.isValid(), qPrintable(lateFetchError.text()));
    QVERIFY(lateFetchError.text().contains(
        QStringLiteral("malformed"),
        Qt::CaseInsensitive
        ));
    lateFetchProbe.finish();
    const QList<QVariantList> jsonScheduleRowsBefore = rawJsonScheduleRows(
        database,
        QStringLiteral("f934_intensive_json_schedule_rows")
        );

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(selectedClassId),
        asTeacherId(assignedTeacherId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("Loading intensive class times failed")
        ));

    QCOMPARE(
        rawClassInfoFields(database, selectedClassId),
        persistedFieldsBefore
        );
    QCOMPARE(readRawTeacherId(database, selectedClassId), persistedTeacherIdBefore);
    QCOMPARE(
        rawScheduleRows(
            database,
            selectedClassId,
            QStringLiteral("class_times")
            ),
        regularScheduleRowsBefore
        );
    QCOMPARE(
        rawJsonScheduleRows(
            database,
            QStringLiteral("f934_intensive_json_schedule_rows")
            ),
        jsonScheduleRowsBefore
        );
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
unavailableSessionReturnsStructuredFailure()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);

    const auto unavailable =
        Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(1),
        asTeacherId(-1),
        port
        );
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);
    QVERIFY(QString::fromStdString(unavailable.error().message).contains(
        QStringLiteral("unavailable")
        ));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(createClass(services, QStringLiteral("Closed Session Class")) > 0);
    services.closeDatabase();

    const auto closed = Application::ClassCoTeacherAssignmentUseCase::execute(
        asClassId(1),
        asTeacherId(-1),
        port
        );
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(closed.error().recoverable);
    QVERIFY(QString::fromStdString(closed.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests)

#include "next_platform_application_services_class_co_teacher_assignment_port_tests.moc"
