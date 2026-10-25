#include "data/database/sql_query_utils.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/testing_class_repository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QtTest>

#include <array>
#include <limits>
#include <utility>

class TestingClassRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void createsUpdatesFiltersAndDeletesTestingClasses();
    void rejectsIncompleteTestingClasses();
    void returnsTypedInputAndMissingRowErrors();
    void rejectsUnavailableGeneratedTestingClassIds();
    void classifiesAssignmentConflictsWithoutParsingDiagnostics();
    void distinguishesMissingParentsAndTestingSubtypes();
    void reportsCursorFailuresForTestingClassReads();
    void persistsEverySupportedMixedLevel();
    void exposesTestingWorkspaceGradeAndLevelChoices();
    void createsClassAndAssignmentAtomically();
    void rejectsGeneratedTestingClassIdsAboveIntRangeAndRollsBack();
    void acceptsMaximumGeneratedTestingClassId();
    void preservesSupportedPersistedTestingClassIdsAndOrder();
    void rejectsInvalidPersistedTestingClassIdsWithoutMutation();
    void deletesEveryCascadeRowAndPreservesSiblingClass();
    void reportsTypedMissingAndPreservesLegacyDeleteDiagnostic();
    void preservesRegularClassWithoutTestingSubtype();
    void rollsBackEveryCascadeDeleteWhenFinalClassDeleteFails();
    void returnsEmptyTestingClassListWhenNoneExist();
    void loadsRegularClassTeacherAssignmentsInOneSnapshot();
    void rejectsPartialClassTeacherAssignmentsAfterCursorError();
    void classInfoRepositoryPreservesErrorCategoriesAndParentOutcomes();
    void classInfoVisitorsPreserveTypedFailuresAndRejectStoredIds();
    void classInfoCoTeacherMissingParentsRollback();
    void classInfoReadersValidateTeacherIdWidths();
    void classInfoSavesReportCheckedRollbackFailures();
    void classInfoReadersRejectMalformedClassIds();
    void classInfoCoTeacherCommitFailuresRollback();
};

namespace
{
bool createSchema(
    QSqlDatabase& database
    )
{
    QSqlQuery query(database);
    const QStringList statements{
        QStringLiteral(R"(
            CREATE TABLE classes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE class_info (
                class_id INTEGER PRIMARY KEY,
                teacher_id INTEGER,
                class_grade TEXT,
                class_level TEXT,
                reading_book TEXT,
                essay_book TEXT,
                class_color TEXT,
                font_color TEXT,
                notes TEXT,
                time_filler_activities TEXT
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE testing_classes (
                class_id INTEGER PRIMARY KEY,
                room TEXT NOT NULL
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE schedule_testing_blocks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                room TEXT NOT NULL DEFAULT '',
                class_id INTEGER,
                UNIQUE(day, start_time)
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE roster_columns (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER,
                name TEXT,
                position INTEGER,
                width INTEGER
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE roster_data (
                class_id INTEGER,
                row_index INTEGER,
                col_index INTEGER,
                value TEXT,
                PRIMARY KEY (class_id, row_index, col_index)
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE speaking_evaluations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER,
                evaluation_name TEXT
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE speaking_eval_data (
                evaluation_id INTEGER,
                row_index INTEGER
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE class_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE class_intensive_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER
            )
        )")
    };

    for (const QString& statement : statements)
    {
        if (!query.exec(statement))
        {
            return false;
        }
    }

    return true;
}

bool addClassInfoQuerySchema(QSqlDatabase& database)
{
    QSqlQuery query(database);
    const QStringList statements{
        QStringLiteral(R"(
            CREATE TABLE teachers (
                id INTEGER PRIMARY KEY,
                teacher_kr TEXT,
                teacher_en TEXT,
                preferred_name TEXT,
                preferred_romanization TEXT,
                room_number TEXT,
                wifi_name TEXT,
                wifi_password TEXT,
                internet_type TEXT,
                zoom_id TEXT,
                zoom_password TEXT,
                projection_type TEXT,
                notes TEXT
            )
        )"),
        QStringLiteral("ALTER TABLE class_times ADD COLUMN day TEXT"),
        QStringLiteral("ALTER TABLE class_times ADD COLUMN start_time TEXT"),
        QStringLiteral("ALTER TABLE class_times ADD COLUMN end_time TEXT"),
        QStringLiteral("ALTER TABLE class_intensive_times ADD COLUMN day TEXT"),
        QStringLiteral("ALTER TABLE class_intensive_times ADD COLUMN start_time TEXT"),
        QStringLiteral("ALTER TABLE class_intensive_times ADD COLUMN end_time TEXT")
    };
    for (const QString& statement : statements)
    {
        if (!query.exec(statement))
        {
            return false;
        }
    }
    return true;
}

int rowCount(
    QSqlDatabase& database,
    const QString& table
    )
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

bool insertPersistedTestingClass(
    QSqlDatabase& database,
    const qint64 classId,
    const QString& name,
    const QString& grade,
    const QString& level,
    const QString& room,
    const qint64 teacherId,
    const QString& classColor,
    const QString& fontColor,
    const QString& notes
    )
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO classes (id, name) VALUES (?, ?)"
        ));
    query.addBindValue(classId);
    query.addBindValue(name);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id,
            class_grade,
            class_level,
            class_color,
            font_color,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, '')
    )"));
    query.addBindValue(classId);
    query.addBindValue(teacherId);
    query.addBindValue(grade);
    query.addBindValue(level);
    query.addBindValue(classColor);
    query.addBindValue(fontColor);
    query.addBindValue(notes);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
        ));
    query.addBindValue(classId);
    query.addBindValue(room);
    return query.exec();
}

bool readPersistedTestingClassSnapshot(
    QSqlDatabase& database,
    QStringList* snapshot
    )
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(R"(
        SELECT
            c.id,
            typeof(c.id),
            c.name,
            tc.class_id,
            typeof(tc.class_id),
            tc.room,
            ci.class_id,
            typeof(ci.class_id),
            ci.teacher_id,
            ci.class_grade,
            ci.class_level,
            ci.class_color,
            ci.font_color,
            ci.notes
        FROM classes c
        JOIN testing_classes tc ON tc.class_id = c.id
        JOIN class_info ci ON ci.class_id = c.id
        ORDER BY c.id
    )")))
    {
        return false;
    }

    snapshot->clear();
    while (query.next())
    {
        QStringList values;
        for (int column = 0; column < query.record().count(); ++column)
        {
            values.append(query.value(column).toString());
        }
        snapshot->append(values.join(QChar(0x1f)));
    }
    return true;
}

bool persistedTestingClassSnapshotRowHasId(
    const QString& row,
    const qint64 classId
    )
{
    const QStringList values = row.split(QChar(0x1f));
    if (values.size() != 14)
    {
        return false;
    }

    const QString idText = QString::number(classId);
    return values.at(0) == idText
        && values.at(1) == QStringLiteral("integer")
        && values.at(3) == idText
        && values.at(4) == QStringLiteral("integer")
        && values.at(6) == idText
        && values.at(7) == QStringLiteral("integer");
}

struct InvalidTestingClassLoadEvidence
{
    bool databaseReady = false;
    bool rowsSeeded = false;
    bool beforeSnapshotRead = false;
    bool loadRejected = false;
    RepositoryErrorCode errorCode = RepositoryErrorCode::Technical;
    QString error;
    bool choicesRejected = false;
    RepositoryErrorCode choicesErrorCode = RepositoryErrorCode::Technical;
    QString choicesError;
    bool afterSnapshotRead = false;
    QStringList before;
    QStringList after;
};

InvalidTestingClassLoadEvidence loadTestingClassesWithInvalidPersistedId(
    const qint64 invalidClassId,
    const QString& connectionName
    )
{
    InvalidTestingClassLoadEvidence evidence;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        evidence.databaseReady =
            database.isValid()
            && database.open()
            && createSchema(database);
        if (evidence.databaseReady)
        {
            evidence.rowsSeeded =
                insertPersistedTestingClass(
                    database,
                    1,
                    QStringLiteral("Valid First Class"),
                    QStringLiteral("M1"),
                    QStringLiteral("Major"),
                    QStringLiteral("Room 1"),
                    7,
                    QStringLiteral("#112233"),
                    QStringLiteral("#FFFFFF"),
                    QStringLiteral("Valid row")
                    )
                && insertPersistedTestingClass(
                    database,
                    invalidClassId,
                    QStringLiteral("Invalid Later Class"),
                    QStringLiteral("M2"),
                    QStringLiteral("Mixed (High)"),
                    QStringLiteral("Room 2"),
                    8,
                    QStringLiteral("#445566"),
                    QStringLiteral("#000000"),
                    QStringLiteral("Invalid row")
                    );
            if (evidence.rowsSeeded)
            {
                evidence.beforeSnapshotRead =
                    readPersistedTestingClassSnapshot(
                        database,
                        &evidence.before
                        );
                if (evidence.beforeSnapshotRead)
                {
                    TestingClassRepository repository(database);
                    const RepositoryResult<QList<TestingClass>> loaded =
                        repository.loadTestingClasses();
                    evidence.loadRejected = !loaded;
                    if (!loaded)
                    {
                        evidence.errorCode = loaded.error().code;
                        evidence.error = loaded.error().diagnostic;
                    }
                    evidence.afterSnapshotRead =
                        readPersistedTestingClassSnapshot(
                            database,
                            &evidence.after
                            );
                    const auto choices = repository.loadTestingClassChoices();
                    evidence.choicesRejected = !choices;
                    if (!choices)
                    {
                        evidence.choicesErrorCode = choices.error().code;
                        evidence.choicesError = choices.error().diagnostic;
                    }
                }
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return evidence;
}

bool insertCascadeRows(
    QSqlDatabase& database,
    const int classId,
    const QString& day,
    const QString& evaluationName
    )
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO schedule_testing_blocks (day, start_time, room, class_id) "
        "VALUES (?, '16:00', 'Library', ?)"
        ));
    query.addBindValue(day);
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, 'English', 0, 120)"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }
    query.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, 0, 0, 'A')"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, ?)"
        ));
    query.addBindValue(classId);
    query.addBindValue(evaluationName);
    if (!query.exec())
    {
        return false;
    }
    const QVariant evaluationId = query.lastInsertId();
    query.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data (evaluation_id, row_index) "
        "VALUES (?, 0)"
        ));
    query.addBindValue(evaluationId);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id) VALUES (?)"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }
    query.prepare(QStringLiteral(
        "INSERT INTO class_intensive_times (class_id) VALUES (?)"
        ));
    query.addBindValue(classId);
    return query.exec();
}

using CascadeCounts = std::array<int, 10>;

CascadeCounts cascadeCounts(QSqlDatabase& database, const int classId)
{
    const QStringList queries{
        QStringLiteral("SELECT COUNT(*) FROM classes WHERE id=?"),
        QStringLiteral("SELECT COUNT(*) FROM class_info WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM testing_classes WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM roster_columns WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM roster_data WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM speaking_evaluations WHERE class_id=?"),
        QStringLiteral(
            "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id IN ("
            "SELECT id FROM speaking_evaluations WHERE class_id=?)"
            ),
        QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM class_intensive_times WHERE class_id=?")
    };
    CascadeCounts counts{};
    for (qsizetype index = 0; index < queries.size(); ++index)
    {
        QSqlQuery query(database);
        query.prepare(queries.at(index));
        query.addBindValue(classId);
        if (!query.exec() || !query.next())
        {
            counts.at(static_cast<std::size_t>(index)) = -1;
            continue;
        }
        counts.at(static_cast<std::size_t>(index)) = query.value(0).toInt();
    }
    return counts;
}
}

void TestingClassRepositoryTests
    ::createsUpdatesFiltersAndDeletesTestingClasses()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_crud");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        ClassRepository classRepository(database);
        const RepositoryResult<int> regularClass =
            classRepository.createClass(QStringLiteral("Regular"));
        QVERIFY(regularClass);
        const int regularClassId = *regularClass;

        TestingClassRepository repository(database);
        TestingClass testingClass;
        testingClass.name = QStringLiteral("Writing Support");
        testingClass.grade = QStringLiteral("M2");
        testingClass.level = QStringLiteral("Mixed (Low)");
        testingClass.room = QStringLiteral("402");
        testingClass.teacherId = 9;
        testingClass.classColor = QStringLiteral("#123456");
        testingClass.fontColor = QStringLiteral("#FFFFFF");
        testingClass.notes = QStringLiteral("Bring dictionaries.");

        const RepositoryResult<int> created =
            repository.createTestingClass(testingClass);
        QVERIFY(created);
        QVERIFY(*created > regularClassId);

        const RepositoryResult<QList<Classroom>> regularClasses =
            classRepository.getClasses();
        QVERIFY(regularClasses);
        QCOMPARE(regularClasses->size(), 1);
        QCOMPARE(regularClasses->first().id, regularClassId);

        auto loaded =
            repository.loadTestingClass(*created);
        QVERIFY(loaded);
        QCOMPARE(loaded->name, QStringLiteral("Writing Support"));
        QCOMPARE(loaded->level, QStringLiteral("Mixed (Low)"));
        QCOMPARE(loaded->room, QStringLiteral("402"));
        QCOMPARE(loaded->teacherId, 9);
        QCOMPARE(loaded->notes, QStringLiteral("Bring dictionaries."));

        QSqlQuery query(database);
        query.prepare(R"(
            INSERT INTO roster_data (
                class_id,
                row_index,
                col_index,
                value
            )
            VALUES (?, 0, 0, 'Student')
        )");
        query.addBindValue(*created);
        QVERIFY(query.exec());
        query.prepare(R"(
            INSERT INTO roster_columns (
                class_id,
                name,
                position,
                width
            )
            VALUES (?, 'English', 0, 120)
        )");
        query.addBindValue(*created);
        QVERIFY(query.exec());
        query.prepare(R"(
            INSERT INTO schedule_testing_blocks (
                day,
                start_time,
                room,
                class_id
            )
            VALUES ('Monday', '16:00', '', ?)
        )");
        query.addBindValue(*created);
        QVERIFY(query.exec());

        testingClass = *loaded;
        testingClass.name = QStringLiteral("Writing Lab");
        testingClass.level = QStringLiteral("Mixed (All)");
        testingClass.room = QStringLiteral("Library");
        QVERIFY(repository.updateTestingClass(testingClass));

        loaded = repository.loadTestingClass(*created);
        QVERIFY(loaded);
        QCOMPARE(loaded->name, QStringLiteral("Writing Lab"));
        QCOMPARE(loaded->level, QStringLiteral("Mixed (All)"));
        QCOMPARE(loaded->room, QStringLiteral("Library"));

        const auto all =
            repository.loadTestingClasses();
        QVERIFY(all);
        QCOMPARE(all->size(), 1);

        QVERIFY(repository.deleteTestingClass(*created));
        const auto exists =
            repository.isTestingClass(*created);
        QVERIFY(exists);
        QVERIFY(!*exists);

        const QStringList tables{
            QStringLiteral("classes"),
            QStringLiteral("class_info"),
            QStringLiteral("testing_classes"),
            QStringLiteral("roster_columns"),
            QStringLiteral("roster_data"),
            QStringLiteral("schedule_testing_blocks")
        };
        for (const QString& table : tables)
        {
            query.prepare(
                QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2=?")
                    .arg(
                        table,
                        table == QStringLiteral("classes")
                            ? QStringLiteral("id")
                            : QStringLiteral("class_id")
                        )
                );
            query.addBindValue(*created);
            QVERIFY(query.exec());
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 0);
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::rejectsIncompleteTestingClasses()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_validation");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        TestingClassRepository repository(database);
        TestingClass testingClass;
        const auto missingName = repository.createTestingClass(testingClass);
        QVERIFY(!missingName);
        QCOMPARE(missingName.error().code, RepositoryErrorCode::Validation);
        QVERIFY(!missingName.error().recoverableOverride.has_value());

        testingClass.name = QStringLiteral("Support");
        testingClass.grade = QStringLiteral("M1");
        testingClass.level = QStringLiteral("Mixed (High)");
        const auto missingRoom = repository.createTestingClass(testingClass);
        QVERIFY(!missingRoom);
        QCOMPARE(missingRoom.error().code, RepositoryErrorCode::Validation);

        testingClass.room = QStringLiteral("305");
        const RepositoryResult<int> created =
            repository.createTestingClass(testingClass);
        QVERIFY(created);

        testingClass.classId = *created;
        testingClass.level.clear();
        QVERIFY(!repository.updateTestingClass(testingClass));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::returnsTypedInputAndMissingRowErrors()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_typed_input");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        TestingClassRepository repository(database);
        TestingClass value;
        value.name = QStringLiteral("Typed Validation");
        value.grade = QStringLiteral("M1");
        value.level = QStringLiteral("Major");
        value.room = QStringLiteral("Room 1");

        const std::array<std::pair<QString TestingClass::*, QString>, 4>
            requiredFields{{
                {&TestingClass::name, QStringLiteral("Testing class name is required.")},
                {&TestingClass::grade, QStringLiteral("Testing class grade is required.")},
                {&TestingClass::level, QStringLiteral("Testing class level is required.")},
                {&TestingClass::room, QStringLiteral("Testing class room is required.")}
            }};
        for (const auto& [field, diagnostic] : requiredFields)
        {
            TestingClass invalid = value;
            (invalid.*field).clear();
            const auto validation = repository.createTestingClass(invalid);
            QVERIFY(!validation);
            QCOMPARE(validation.error().code, RepositoryErrorCode::Validation);
            QCOMPARE(toLegacyDiagnostic(validation.error()), diagnostic);
        }

        const auto invalidClass = repository.loadTestingClass(0);
        QVERIFY(!invalidClass);
        QCOMPARE(invalidClass.error().code, RepositoryErrorCode::InvalidInput);
        QCOMPARE(
            toLegacyDiagnostic(invalidClass.error()),
            QStringLiteral("A valid testing class is required.")
            );

        const auto missingClass = repository.loadTestingClass(4321);
        QVERIFY(!missingClass);
        QCOMPARE(missingClass.error().code, RepositoryErrorCode::NotFound);
        QCOMPARE(
            toLegacyDiagnostic(missingClass.error()),
            QStringLiteral("The testing class was not found.")
            );

        const auto invalidDetailsId =
            repository.loadTestingClassDetailsReadRecordDetailed(0);
        QVERIFY(!invalidDetailsId);
        QCOMPARE(
            invalidDetailsId.error().code,
            RepositoryErrorCode::InvalidInput
            );
        const auto legacyInvalidDetailsId =
            repository.loadTestingClassDetailsReadRecord(0);
        QVERIFY(!legacyInvalidDetailsId);
        QCOMPARE(
            legacyInvalidDetailsId.error().code,
            invalidDetailsId.error().code
            );
        QCOMPARE(
            legacyInvalidDetailsId.error().diagnostic,
            invalidDetailsId.error().diagnostic
            );

        const auto partialAssignment = repository.createTestingClass(
            value,
            QStringLiteral("Monday"),
            QString{}
            );
        QVERIFY(!partialAssignment);
        QCOMPARE(
            partialAssignment.error().code,
            RepositoryErrorCode::InvalidInput
            );

        const auto malformedAssignment = repository.createTestingClass(
            value,
            QStringLiteral("Funday"),
            QStringLiteral("16:00")
            );
        QVERIFY(!malformedAssignment);
        QCOMPARE(
            malformedAssignment.error().code,
            RepositoryErrorCode::Validation
            );

        const auto blankAssignment = repository.createTestingClass(
            value,
            QStringLiteral("  "),
            QStringLiteral("\t")
            );
        QVERIFY(blankAssignment);
        QCOMPARE(
            rowCount(database, QStringLiteral("schedule_testing_blocks")),
            0
            );

        value.classId = 0;
        const auto invalidUpdateId = repository.updateTestingClass(value);
        QVERIFY(!invalidUpdateId);
        QCOMPARE(invalidUpdateId.error().code, RepositoryErrorCode::InvalidInput);
        const auto invalidDetailedUpdateId =
            repository.updateTestingClassDetailed(value);
        QVERIFY(!invalidDetailedUpdateId);
        QCOMPARE(
            invalidDetailedUpdateId.error().code,
            RepositoryErrorCode::InvalidInput
            );

        value.classId = 1;
        value.grade.clear();
        const auto incompleteUpdate = repository.updateTestingClass(value);
        QVERIFY(!incompleteUpdate);
        QCOMPARE(incompleteUpdate.error().code, RepositoryErrorCode::Validation);

        const auto invalidDeleteId = repository.deleteTestingClass(0);
        QVERIFY(!invalidDeleteId);
        QCOMPARE(invalidDeleteId.error().code, RepositoryErrorCode::InvalidInput);
        QCOMPARE(
            toLegacyDiagnostic(invalidDeleteId.error()),
            QStringLiteral("The testing class no longer exists.")
            );

        const auto nonpositivePredicate = repository.isTestingClass(0);
        QVERIFY(nonpositivePredicate);
        QVERIFY(!*nonpositivePredicate);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::rejectsUnavailableGeneratedTestingClassIds()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_missing_generated_id");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        QSqlQuery trigger(database);
        QVERIFY2(trigger.exec(QStringLiteral(R"(
            CREATE TRIGGER ignore_testing_class_parent_insert
            BEFORE INSERT ON classes
            BEGIN
                SELECT RAISE(IGNORE);
            END
        )")), qPrintable(trigger.lastError().text()));

        TestingClass value;
        value.name = QStringLiteral("No Generated ID");
        value.grade = QStringLiteral("M1");
        value.level = QStringLiteral("Major");
        value.room = QStringLiteral("Room 1");
        TestingClassRepository repository(database);

        const auto result = repository.createTestingClass(value);

        QVERIFY(!result);
        QCOMPARE(result.error().code, RepositoryErrorCode::Technical);
        QCOMPARE(
            toLegacyDiagnostic(result.error()),
            QStringLiteral("Creating the testing class did not return a valid ID.")
            );
        QCOMPARE(rowCount(database, QStringLiteral("classes")), 0);
        QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
        QCOMPARE(rowCount(database, QStringLiteral("testing_classes")), 0);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::classifiesAssignmentConflictsWithoutParsingDiagnostics()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_assignment_error_codes");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        TestingClassRepository repository(database);
        TestingClass value;
        value.name = QStringLiteral("Slot Owner");
        value.grade = QStringLiteral("M1");
        value.level = QStringLiteral("Major");
        value.room = QStringLiteral("Room 1");
        const auto owner = repository.createTestingClass(
            value,
            QStringLiteral("Monday"),
            QStringLiteral("16:00")
            );
        QVERIFY(owner);

        value.name = QStringLiteral("Duplicate Slot");
        const auto duplicate = repository.createTestingClass(
            value,
            QStringLiteral("Monday"),
            QStringLiteral("16:00")
            );
        QVERIFY(!duplicate);
        QCOMPARE(duplicate.error().code, RepositoryErrorCode::Conflict);
        QVERIFY(toLegacyDiagnostic(duplicate.error()).contains(
            QStringLiteral("Assigning the new testing class failed")
            ));

        QSqlQuery trigger(database);
        QVERIFY2(trigger.exec(QStringLiteral(R"(
            CREATE TRIGGER fail_testing_assignment_with_constraint_text
            BEFORE INSERT ON schedule_testing_blocks
            BEGIN
                SELECT RAISE(ABORT,
                    'UNIQUE constraint failed: schedule_testing_blocks.day');
            END
        )")), qPrintable(trigger.lastError().text()));

        value.name = QStringLiteral("Trigger Failure");
        const auto otherSqlFailure = repository.createTestingClass(
            value,
            QStringLiteral("Tuesday"),
            QStringLiteral("16:00")
            );
        QVERIFY(!otherSqlFailure);
        QCOMPARE(
            otherSqlFailure.error().code,
            RepositoryErrorCode::Technical
            );
        QVERIFY(toLegacyDiagnostic(otherSqlFailure.error()).contains(
            QStringLiteral("UNIQUE constraint failed"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(rowCount(database, QStringLiteral("classes")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("class_info")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("testing_classes")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("schedule_testing_blocks")), 1);

        QVERIFY2(trigger.exec(QStringLiteral(
            "DROP TRIGGER fail_testing_assignment_with_constraint_text"
            )), qPrintable(trigger.lastError().text()));
        QVERIFY2(trigger.exec(QStringLiteral(
            "DROP TABLE schedule_testing_blocks"
            )), qPrintable(trigger.lastError().text()));
        QVERIFY2(trigger.exec(QStringLiteral(
            "CREATE TABLE teachers (id INTEGER PRIMARY KEY)"
            )), qPrintable(trigger.lastError().text()));
        QVERIFY2(trigger.exec(QStringLiteral(R"(
            CREATE TABLE schedule_testing_blocks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                room TEXT NOT NULL DEFAULT '',
                class_id INTEGER REFERENCES teachers(id),
                UNIQUE(day, start_time)
            )
        )")), qPrintable(trigger.lastError().text()));
        QVERIFY2(trigger.exec(QStringLiteral("PRAGMA foreign_keys=ON")),
            qPrintable(trigger.lastError().text()));

        value.name = QStringLiteral("Foreign Key Failure");
        const auto foreignKeyFailure = repository.createTestingClass(
            value,
            QStringLiteral("Wednesday"),
            QStringLiteral("17:00")
            );
        QVERIFY(!foreignKeyFailure);
        QCOMPARE(
            foreignKeyFailure.error().code,
            RepositoryErrorCode::Technical
            );
        QVERIFY(toLegacyDiagnostic(foreignKeyFailure.error()).contains(
            QStringLiteral("FOREIGN KEY constraint failed"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(rowCount(database, QStringLiteral("classes")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("class_info")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("testing_classes")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("schedule_testing_blocks")), 0);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::distinguishesMissingParentsAndTestingSubtypes()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_missing_targets");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        ClassRepository classRepository(database);
        TestingClassRepository repository(database);
        TestingClass value;
        value.name = QStringLiteral("Missing Target");
        value.grade = QStringLiteral("M1");
        value.level = QStringLiteral("Major");
        value.room = QStringLiteral("Room 1");
        value.classId = 87;

        const auto missingParentUpdate =
            repository.updateTestingClassDetailed(value);
        QVERIFY(!missingParentUpdate);
        QCOMPARE(
            missingParentUpdate.error().code,
            RepositoryErrorCode::NotFound
            );
        QCOMPARE(
            toLegacyDiagnostic(missingParentUpdate.error()),
            QStringLiteral("The testing class parent class was not found for class id 87.")
            );

        const auto regularClass = classRepository.createClass(
            QStringLiteral("Regular Class")
            );
        QVERIFY(regularClass);
        value.classId = *regularClass;

        const auto missingSubtypeUpdate =
            repository.updateTestingClassDetailed(value);
        QVERIFY(!missingSubtypeUpdate);
        QCOMPARE(
            missingSubtypeUpdate.error().code,
            RepositoryErrorCode::Technical
            );
        QCOMPARE(
            toLegacyDiagnostic(missingSubtypeUpdate.error()),
            QStringLiteral("The testing class no longer exists.")
            );

        const auto missingSubtypeRead =
            repository.loadTestingClassDetailsReadRecordDetailed(*regularClass);
        QVERIFY(!missingSubtypeRead);
        QCOMPARE(
            missingSubtypeRead.error().code,
            RepositoryErrorCode::Technical
            );
        QCOMPARE(
            toLegacyDiagnostic(missingSubtypeRead.error()),
            QStringLiteral("The testing class was not found.")
            );

        const auto missingParentRead =
            repository.loadTestingClassDetailsReadRecordDetailed(654321);
        QVERIFY(!missingParentRead);
        QCOMPARE(
            missingParentRead.error().code,
            RepositoryErrorCode::NotFound
            );
        QCOMPARE(
            toLegacyDiagnostic(missingParentRead.error()),
            QStringLiteral("The testing class was not found.")
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::reportsCursorFailuresForTestingClassReads()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_cursor_failures");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Valid'), (2, 'Bad JSON')"
            )));
        QVERIFY(query.exec(QStringLiteral("DROP TABLE testing_classes")));
        QVERIFY(query.exec(QStringLiteral(R"(
            CREATE TABLE testing_class_json_backing (
                row_id INTEGER PRIMARY KEY,
                payload TEXT NOT NULL
            )
        )")));
        QVERIFY(query.exec(QStringLiteral(R"(
            INSERT INTO testing_class_json_backing (row_id, payload)
            VALUES (1, '{"class_id":1,"room":"A"}'), (2, '{')
        )")));
        QVERIFY(query.exec(QStringLiteral(R"(
            CREATE VIEW testing_classes AS
            SELECT
                json_extract(payload, '$.class_id') AS class_id,
                json_extract(payload, '$.room') AS room
            FROM testing_class_json_backing
            ORDER BY row_id
        )")));

        TestingClassRepository repository(database);
        const auto predicate = repository.isTestingClass(999);
        QVERIFY(!predicate);
        QCOMPARE(predicate.error().code, RepositoryErrorCode::Technical);

        const auto singleLoad = repository.loadTestingClass(999);
        QVERIFY(!singleLoad);
        QCOMPARE(singleLoad.error().code, RepositoryErrorCode::Technical);

        const auto loaded = repository.loadTestingClasses();
        QVERIFY(!loaded);
        QCOMPARE(loaded.error().code, RepositoryErrorCode::Technical);
        QVERIFY(toLegacyDiagnostic(loaded.error()).contains(
            QStringLiteral("Loading testing classes failed"),
            Qt::CaseInsensitive
            ));

        const auto choices = repository.loadTestingClassChoices();
        QVERIFY(!choices);
        QCOMPARE(choices.error().code, RepositoryErrorCode::Technical);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::persistsEverySupportedMixedLevel()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_mixed_levels");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        TestingClassRepository repository(database);
        for (const QString& level : testingClassMixedLevels())
        {
            TestingClass testingClass;
            testingClass.name =
                QStringLiteral("Testing %1").arg(level);
            testingClass.grade = QStringLiteral("M2");
            testingClass.level = level;
            testingClass.room = QStringLiteral("401");

            const RepositoryResult<int> created =
                repository.createTestingClass(testingClass);
            QVERIFY(created);

            const RepositoryResult<TestingClass> loaded =
                repository.loadTestingClass(*created);
            QVERIFY(loaded);
            QCOMPARE(loaded->level, level);
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::exposesTestingWorkspaceGradeAndLevelChoices()
{
    QCOMPARE(
        testingClassGrades(),
        QStringList({
            QStringLiteral("M1"),
            QStringLiteral("M2"),
            QStringLiteral("Mixed")
        })
        );
    QCOMPARE(
        testingClassMixedLevels(),
        QStringList({
            QStringLiteral("Mixed (All)"),
            QStringLiteral("Mixed (High)"),
            QStringLiteral("Mixed (Low)")
        })
        );
    QCOMPARE(
        testingClassLevelsForGrade(
            QStringLiteral("M1")
            ),
        QStringList({
            QStringLiteral("Mixed (All)"),
            QStringLiteral("Mixed (High)"),
            QStringLiteral("Mixed (Low)"),
            QStringLiteral("Song's"),
            QStringLiteral("Major"),
            QStringLiteral("Solis"),
            QStringLiteral("Galaxia"),
            QStringLiteral("Elephantus")
        })
        );
    QCOMPARE(
        testingClassLevelsForGrade(
            QStringLiteral("M2")
            ),
        QStringList({
            QStringLiteral("Mixed (All)"),
            QStringLiteral("Mixed (High)"),
            QStringLiteral("Mixed (Low)"),
            QStringLiteral("Song's"),
            QStringLiteral("Major"),
            QStringLiteral("Tigris"),
            QStringLiteral("Leo"),
            QStringLiteral("Ursa")
        })
        );
    QCOMPARE(
        testingClassLevelsForGrade(
            QStringLiteral("Mixed")
            ),
        testingClassMixedLevels()
        );
}

void TestingClassRepositoryTests
    ::createsClassAndAssignmentAtomically()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_atomic_assignment");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        TestingClassRepository repository(database);
        TestingClass testingClass;
        testingClass.name = QStringLiteral("Atomic Testing");
        testingClass.grade = QStringLiteral("M1");
        testingClass.level = QStringLiteral("Mixed (All)");
        testingClass.room = QStringLiteral("Library");

        const RepositoryResult<int> created =
            repository.createTestingClass(
                testingClass,
                QStringLiteral("monday"),
                QStringLiteral("16:00")
                );
        QVERIFY(created);

        QSqlQuery query(database);
        query.prepare(R"(
            SELECT day, start_time, class_id
            FROM schedule_testing_blocks
            WHERE class_id=?
        )");
        query.addBindValue(*created);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("16:00"));
        QCOMPARE(query.value(2).toInt(), *created);
        QVERIFY(!query.next());

        const QStringList atomicTables{
            QStringLiteral("classes"),
            QStringLiteral("class_info"),
            QStringLiteral("testing_classes"),
            QStringLiteral("schedule_testing_blocks")
        };
        QList<int> rowsBeforeFailure;
        for (const QString& table : atomicTables)
        {
            const int count = rowCount(database, table);
            QVERIFY(count >= 0);
            rowsBeforeFailure.append(count);
        }

        testingClass.name = QStringLiteral("Must Roll Back");
        const RepositoryResult<int> rejected =
            repository.createTestingClass(
                testingClass,
                QStringLiteral("Monday"),
                QStringLiteral("16:00")
                );
        QVERIFY(!rejected);

        QVERIFY(query.exec(
            QStringLiteral(
                "SELECT COUNT(*) FROM classes "
                "WHERE name='Must Roll Back'"
                )
            ));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);

        for (qsizetype index = 0; index < atomicTables.size(); ++index)
        {
            QCOMPARE(
                rowCount(database, atomicTables.at(index)),
                rowsBeforeFailure.at(index)
                );
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::rejectsGeneratedTestingClassIdsAboveIntRangeAndRollsBack()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_wide_id_rollback");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        ClassRepository classRepository(database);
        TestingClassRepository testingClassRepository(database);
        const RepositoryResult<int> existingClassId =
            classRepository.createClass(QStringLiteral("Existing ordinary"));
        QVERIFY(existingClassId);
        QCOMPARE(*existingClassId, 1);

        QSqlQuery query(database);
        QVERIFY(query.prepare(
            QStringLiteral(
                "UPDATE sqlite_sequence SET seq=? WHERE name='classes'"
                )
            ));
        query.addBindValue(QVariant::fromValue<qlonglong>(4294967296LL));
        QVERIFY(query.exec());
        QCOMPARE(query.numRowsAffected(), 1);

        TestingClass testingClass;
        testingClass.name = QStringLiteral("Rejected wide testing class");
        testingClass.grade = QStringLiteral("M1");
        testingClass.level = QStringLiteral("Mixed (All)");
        testingClass.room = QStringLiteral("Library");
        const RepositoryResult<int> rejected =
            testingClassRepository.createTestingClass(
                testingClass,
                QStringLiteral("Monday"),
                QStringLiteral("16:00")
                );
        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code, RepositoryErrorCode::Validation);
        QVERIFY(rejected.error().recoverableOverride.has_value());
        QVERIFY(!*rejected.error().recoverableOverride);
        QCOMPARE(
            toLegacyDiagnostic(rejected.error()),
            QStringLiteral(
                "Creating the testing class returned an ID outside the "
                "supported integer range."
                )
            );

        QVERIFY(query.exec(QStringLiteral(
            "SELECT id, name FROM classes"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toLongLong(), 1LL);
        QCOMPARE(query.value(1).toString(), QStringLiteral("Existing ordinary"));
        QVERIFY(!query.next());

        QCOMPARE(rowCount(database, QStringLiteral("classes")), 1);
        QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
        QCOMPARE(rowCount(database, QStringLiteral("testing_classes")), 0);
        QCOMPARE(rowCount(database, QStringLiteral("schedule_testing_blocks")), 0);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::acceptsMaximumGeneratedTestingClassId()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_maximum_id");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        ClassRepository classRepository(database);
        TestingClassRepository testingClassRepository(database);
        const RepositoryResult<int> existingClassId =
            classRepository.createClass(QStringLiteral("Existing ordinary"));
        QVERIFY(existingClassId);
        QCOMPARE(*existingClassId, 1);

        const int maxClassId = std::numeric_limits<int>::max();
        QSqlQuery query(database);
        QVERIFY(query.prepare(
            QStringLiteral(
                "UPDATE sqlite_sequence SET seq=? WHERE name='classes'"
                )
            ));
        query.addBindValue(
            QVariant::fromValue<qlonglong>(
                static_cast<qlonglong>(maxClassId) - 1
                )
            );
        QVERIFY(query.exec());
        QCOMPARE(query.numRowsAffected(), 1);

        TestingClass testingClass;
        testingClass.name = QStringLiteral("Maximum ID testing class");
        testingClass.grade = QStringLiteral("M1");
        testingClass.level = QStringLiteral("Mixed (All)");
        testingClass.room = QStringLiteral("Library");
        const RepositoryResult<int> created =
            testingClassRepository.createTestingClass(
                testingClass,
                QStringLiteral("Monday"),
                QStringLiteral("16:00")
                );
        QVERIFY(created);
        QCOMPARE(*created, maxClassId);

        QVERIFY(query.exec(QStringLiteral(
            "SELECT id, name FROM classes WHERE id=2147483647"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toLongLong(),
            static_cast<qlonglong>(maxClassId));
        QCOMPARE(query.value(1).toString(), testingClass.name);
        QVERIFY(!query.next());

        QVERIFY(query.exec(QStringLiteral(
            "SELECT class_id FROM class_info"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toLongLong(),
            static_cast<qlonglong>(maxClassId));
        QVERIFY(!query.next());

        QVERIFY(query.exec(QStringLiteral(
            "SELECT class_id FROM testing_classes"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toLongLong(),
            static_cast<qlonglong>(maxClassId));
        QVERIFY(!query.next());

        QVERIFY(query.exec(QStringLiteral(
            "SELECT class_id, day, start_time FROM schedule_testing_blocks"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toLongLong(),
            static_cast<qlonglong>(maxClassId));
        QCOMPARE(query.value(1).toString(), QStringLiteral("Monday"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("16:00"));
        QVERIFY(!query.next());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::preservesSupportedPersistedTestingClassIdsAndOrder()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_supported_persisted_ids");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        constexpr qint64 supportedClassId = 1;
        constexpr int maximumClassId = std::numeric_limits<int>::max();
        QVERIFY(insertPersistedTestingClass(
            database,
            supportedClassId,
            QStringLiteral("Zulu Supported Class"),
            QStringLiteral("M2"),
            QStringLiteral("Mixed (High)"),
            QStringLiteral("Room 1"),
            17,
            QStringLiteral("#112233"),
            QStringLiteral("#FFFFFF"),
            QStringLiteral("Supported row notes")
            ));
        QVERIFY(insertPersistedTestingClass(
            database,
            maximumClassId,
            QStringLiteral("Alpha Maximum Class"),
            QStringLiteral("M1"),
            QStringLiteral("Major"),
            QStringLiteral("Room 2"),
            23,
            QStringLiteral("#445566"),
            QStringLiteral("#000000"),
            QStringLiteral("Maximum row notes")
            ));

        QStringList rowsBefore;
        QVERIFY(readPersistedTestingClassSnapshot(database, &rowsBefore));
        QCOMPARE(rowsBefore.size(), 2);
        QVERIFY(persistedTestingClassSnapshotRowHasId(rowsBefore.at(0), 1));
        QVERIFY(persistedTestingClassSnapshotRowHasId(
            rowsBefore.at(1),
            maximumClassId
            ));

        TestingClassRepository repository(database);
        const RepositoryResult<QList<TestingClass>> loaded =
            repository.loadTestingClasses();
        QVERIFY(loaded);
        QCOMPARE(loaded->size(), 2);
        QCOMPARE(loaded->at(0).classId, maximumClassId);
        QCOMPARE(loaded->at(0).name, QStringLiteral("Alpha Maximum Class"));
        QCOMPARE(loaded->at(0).grade, QStringLiteral("M1"));
        QCOMPARE(loaded->at(0).level, QStringLiteral("Major"));
        QCOMPARE(loaded->at(0).room, QStringLiteral("Room 2"));
        QCOMPARE(loaded->at(0).teacherId, 23);
        QCOMPARE(loaded->at(0).classColor, QStringLiteral("#445566"));
        QCOMPARE(loaded->at(0).fontColor, QStringLiteral("#000000"));
        QCOMPARE(loaded->at(0).notes, QStringLiteral("Maximum row notes"));
        QCOMPARE(loaded->at(1).classId, 1);
        QCOMPARE(loaded->at(1).name, QStringLiteral("Zulu Supported Class"));
        QCOMPARE(loaded->at(1).grade, QStringLiteral("M2"));
        QCOMPARE(loaded->at(1).level, QStringLiteral("Mixed (High)"));
        QCOMPARE(loaded->at(1).room, QStringLiteral("Room 1"));
        QCOMPARE(loaded->at(1).teacherId, 17);
        QCOMPARE(loaded->at(1).classColor, QStringLiteral("#112233"));
        QCOMPARE(loaded->at(1).fontColor, QStringLiteral("#FFFFFF"));
        QCOMPARE(loaded->at(1).notes, QStringLiteral("Supported row notes"));

        const RepositoryResult<QList<TestingClassChoiceRecord>> choices =
            repository.loadTestingClassChoices();
        QVERIFY(choices);
        QCOMPARE(choices->size(), 2);
        QCOMPARE(choices->at(0).classId, maximumClassId);
        QCOMPARE(choices->at(0).name, QStringLiteral("Alpha Maximum Class"));
        QCOMPARE(choices->at(1).classId, 1);
        QCOMPARE(choices->at(1).name, QStringLiteral("Zulu Supported Class"));

        QStringList rowsAfter;
        QVERIFY(readPersistedTestingClassSnapshot(database, &rowsAfter));
        QCOMPARE(rowsAfter, rowsBefore);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::rejectsInvalidPersistedTestingClassIdsWithoutMutation()
{
    const InvalidTestingClassLoadEvidence wideIdEvidence =
        loadTestingClassesWithInvalidPersistedId(
            4294967297LL,
            QStringLiteral("testing_class_repository_wide_persisted_id")
            );
    QVERIFY(wideIdEvidence.databaseReady);
    QVERIFY(wideIdEvidence.rowsSeeded);
    QVERIFY(wideIdEvidence.beforeSnapshotRead);
    QCOMPARE(wideIdEvidence.before.size(), 2);
    QVERIFY(persistedTestingClassSnapshotRowHasId(
        wideIdEvidence.before.at(0),
        1
        ));
    QVERIFY(persistedTestingClassSnapshotRowHasId(
        wideIdEvidence.before.at(1),
        4294967297LL
        ));
    QVERIFY(wideIdEvidence.loadRejected);
    QCOMPARE(wideIdEvidence.errorCode, RepositoryErrorCode::Validation);
    QVERIFY(wideIdEvidence.error.contains(
        QStringLiteral("invalid class identifier")
        ));
    QVERIFY(wideIdEvidence.choicesRejected);
    QCOMPARE(wideIdEvidence.choicesErrorCode, RepositoryErrorCode::Validation);
    QVERIFY(wideIdEvidence.choicesError.contains(
        QStringLiteral("invalid class identifier")
        ));
    QVERIFY(wideIdEvidence.afterSnapshotRead);
    QCOMPARE(wideIdEvidence.after, wideIdEvidence.before);

    const InvalidTestingClassLoadEvidence zeroIdEvidence =
        loadTestingClassesWithInvalidPersistedId(
            0,
            QStringLiteral("testing_class_repository_zero_persisted_id")
            );
    QVERIFY(zeroIdEvidence.databaseReady);
    QVERIFY(zeroIdEvidence.rowsSeeded);
    QVERIFY(zeroIdEvidence.beforeSnapshotRead);
    QCOMPARE(zeroIdEvidence.before.size(), 2);
    QVERIFY(persistedTestingClassSnapshotRowHasId(
        zeroIdEvidence.before.at(0),
        0
        ));
    QVERIFY(persistedTestingClassSnapshotRowHasId(
        zeroIdEvidence.before.at(1),
        1
        ));
    QVERIFY(zeroIdEvidence.loadRejected);
    QCOMPARE(zeroIdEvidence.errorCode, RepositoryErrorCode::Validation);
    QVERIFY(zeroIdEvidence.error.contains(
        QStringLiteral("invalid class identifier")
        ));
    QVERIFY(zeroIdEvidence.choicesRejected);
    QCOMPARE(zeroIdEvidence.choicesErrorCode, RepositoryErrorCode::Validation);
    QVERIFY(zeroIdEvidence.afterSnapshotRead);
    QCOMPARE(zeroIdEvidence.after, zeroIdEvidence.before);
}

void TestingClassRepositoryTests::
deletesEveryCascadeRowAndPreservesSiblingClass()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_delete_cascade");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        TestingClassRepository repository(database);

        TestingClass target;
        target.name = QStringLiteral("Cascade Target");
        target.grade = QStringLiteral("M1");
        target.level = QStringLiteral("Major");
        target.room = QStringLiteral("Target Room");
        const auto targetId = repository.createTestingClass(target);
        QVERIFY(targetId);
        TestingClass sibling = target;
        sibling.name = QStringLiteral("Cascade Sibling");
        sibling.room = QStringLiteral("Sibling Room");
        const auto siblingId = repository.createTestingClass(sibling);
        QVERIFY(siblingId);
        QVERIFY(insertCascadeRows(
            database, *targetId, QStringLiteral("Monday"),
            QStringLiteral("Target Speaking")
            ));
        QVERIFY(insertCascadeRows(
            database, *siblingId, QStringLiteral("Tuesday"),
            QStringLiteral("Sibling Speaking")
            ));

        QCOMPARE(cascadeCounts(database, *targetId), CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        QCOMPARE(cascadeCounts(database, *siblingId), CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        QCOMPARE(rowCount(database, QStringLiteral("speaking_eval_data")), 2);
        QVERIFY(repository.deleteTestingClass(*targetId));

        QCOMPARE(cascadeCounts(database, *targetId), CascadeCounts({0,0,0,0,0,0,0,0,0,0}));
        QCOMPARE(cascadeCounts(database, *siblingId), CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        QCOMPARE(rowCount(database, QStringLiteral("speaking_eval_data")), 1);
        const auto remaining = repository.loadTestingClass(*siblingId);
        QVERIFY(remaining);
        QCOMPARE(remaining->name, QStringLiteral("Cascade Sibling"));
        QCOMPARE(remaining->room, QStringLiteral("Sibling Room"));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::
reportsTypedMissingAndPreservesLegacyDeleteDiagnostic()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_typed_missing_delete");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        TestingClassRepository repository(database);
        constexpr int missingClassId = 987654;

        QSqlQuery setupQuery(database);
        setupQuery.prepare(QStringLiteral(R"(
            INSERT INTO schedule_testing_blocks (
                day,
                start_time,
                room,
                class_id
            )
            VALUES ('Monday', '16:00', 'Orphan', ?)
        )"));
        setupQuery.addBindValue(missingClassId);
        QVERIFY(setupQuery.exec());

        const auto missing =
            repository.deleteTestingClassDetailed(missingClassId);
        QVERIFY(!missing);
        QCOMPARE(missing.error().code, RepositoryErrorCode::NotFound);
        QCOMPARE(
            missing.error().diagnostic,
            QStringLiteral("The testing class no longer exists.")
            );

        QSqlQuery orphanCount(database);
        orphanCount.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?"
            ));
        orphanCount.addBindValue(missingClassId);
        QVERIFY(orphanCount.exec());
        QVERIFY(orphanCount.next());
        QCOMPARE(orphanCount.value(0).toInt(), 1);

        const RepositoryStatus legacyMissing =
            repository.deleteTestingClass(missingClassId);
        QVERIFY(!legacyMissing);
        QCOMPARE(
            toLegacyDiagnostic(legacyMissing.error()),
            QStringLiteral("The testing class no longer exists.")
            );

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::
preservesRegularClassWithoutTestingSubtype()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_regular_class_delete");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        ClassRepository classRepository(database);
        TestingClassRepository testingClassRepository(database);

        const auto regularClassId = classRepository.createClass(
            QStringLiteral("Regular Class")
            );
        QVERIFY(regularClassId);

        QSqlQuery setupQuery(database);
        setupQuery.prepare(QStringLiteral(R"(
            INSERT INTO class_info (class_id, class_grade, class_level)
            VALUES (?, 'M1', 'Major')
        )"));
        setupQuery.addBindValue(*regularClassId);
        QVERIFY(setupQuery.exec());
        setupQuery.prepare(QStringLiteral(R"(
            INSERT INTO schedule_testing_blocks (
                day,
                start_time,
                room,
                class_id
            )
            VALUES ('Monday', '16:00', 'Regular Room', ?)
        )"));
        setupQuery.addBindValue(*regularClassId);
        QVERIFY(setupQuery.exec());

        const auto deletion =
            testingClassRepository.deleteTestingClassDetailed(*regularClassId);
        QVERIFY(!deletion);
        QCOMPARE(deletion.error().code, RepositoryErrorCode::NotFound);

        const auto regularClass = classRepository.getClassById(*regularClassId);
        QVERIFY(regularClass);
        QCOMPARE(regularClass->name, QStringLiteral("Regular Class"));

        QSqlQuery retainedRows(database);
        retainedRows.prepare(QStringLiteral(R"(
            SELECT
                (SELECT COUNT(*) FROM testing_classes WHERE class_id=?),
                (SELECT COUNT(*) FROM class_info WHERE class_id=?),
                (SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?)
        )"));
        retainedRows.addBindValue(*regularClassId);
        retainedRows.addBindValue(*regularClassId);
        retainedRows.addBindValue(*regularClassId);
        QVERIFY(retainedRows.exec());
        QVERIFY(retainedRows.next());
        QCOMPARE(retainedRows.value(0).toInt(), 0);
        QCOMPARE(retainedRows.value(1).toInt(), 1);
        QCOMPARE(retainedRows.value(2).toInt(), 1);

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::
rollsBackEveryCascadeDeleteWhenFinalClassDeleteFails()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_delete_rollback");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        TestingClassRepository repository(database);

        TestingClass target;
        target.name = QStringLiteral("Rollback Target");
        target.grade = QStringLiteral("M2");
        target.level = QStringLiteral("Song's");
        target.room = QStringLiteral("Target Room");
        const auto targetId = repository.createTestingClass(target);
        QVERIFY(targetId);
        TestingClass sibling = target;
        sibling.name = QStringLiteral("Rollback Sibling");
        sibling.room = QStringLiteral("Sibling Room");
        const auto siblingId = repository.createTestingClass(sibling);
        QVERIFY(siblingId);
        QVERIFY(insertCascadeRows(
            database, *targetId, QStringLiteral("Monday"),
            QStringLiteral("Target Speaking")
            ));
        QVERIFY(insertCascadeRows(
            database, *siblingId, QStringLiteral("Tuesday"),
            QStringLiteral("Sibling Speaking")
            ));
        const CascadeCounts targetBefore = cascadeCounts(database, *targetId);
        const CascadeCounts siblingBefore = cascadeCounts(database, *siblingId);
        QCOMPARE(targetBefore, CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        QCOMPARE(siblingBefore, CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        const int evaluationDataBefore =
            rowCount(database, QStringLiteral("speaking_eval_data"));
        QCOMPARE(evaluationDataBefore, 2);

        QSqlQuery trigger(database);
        QVERIFY(trigger.exec(QStringLiteral(
            "CREATE TRIGGER fail_testing_class_delete "
            "BEFORE DELETE ON classes WHEN OLD.id=%1 "
            "BEGIN SELECT RAISE(ABORT, 'injected final delete failure'); END"
            ).arg(*targetId)));

        const auto deletion =
            repository.deleteTestingClassDetailed(*targetId);
        QVERIFY(!deletion);
        QCOMPARE(deletion.error().code, RepositoryErrorCode::Technical);
        QVERIFY(deletion.error().diagnostic.contains(
            QStringLiteral("injected final delete failure")
            ));
        QCOMPARE(cascadeCounts(database, *targetId), targetBefore);
        QCOMPARE(cascadeCounts(database, *siblingId), siblingBefore);
        QCOMPARE(
            rowCount(database, QStringLiteral("speaking_eval_data")),
            evaluationDataBefore
            );
        const auto remaining = repository.loadTestingClass(*targetId);
        QVERIFY(remaining);
        QCOMPARE(remaining->name, QStringLiteral("Rollback Target"));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::loadsRegularClassTeacherAssignmentsInOneSnapshot()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_sidebar_snapshot");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        ClassRepository classRepository(database);
        const int firstClassId =
            classRepository.createClass(QStringLiteral("Alpha")).value_or(-1);
        const int secondClassId =
            classRepository.createClass(QStringLiteral("Beta")).value_or(-1);
        const int testingClassId =
            classRepository.createClass(QStringLiteral("Testing")).value_or(-1);
        QVERIFY(firstClassId > 0);
        QVERIFY(secondClassId > 0);
        QVERIFY(testingClassId > 0);

        auto insertAssignment = [&](int classId, const QVariant& teacherId)
        {
            QSqlQuery query(database);
            query.prepare(
                QStringLiteral(
                    "INSERT INTO class_info (class_id, teacher_id) VALUES (?, ?)"
                    )
                );
            query.addBindValue(classId);
            query.addBindValue(teacherId);
            return query.exec();
        };

        QVERIFY(insertAssignment(firstClassId, 11));
        QVERIFY(insertAssignment(secondClassId, QVariant()));
        QVERIFY(insertAssignment(testingClassId, 22));

        QSqlQuery query(database);
        QVERIFY(query.exec(
            QStringLiteral(
                "INSERT INTO testing_classes (class_id, room) VALUES (%1, 'T1')"
                ).arg(testingClassId)
            ));

        ClassInfoRepository repository(database);
        const auto assignments = repository.loadClassTeacherAssignments();
        QVERIFY(assignments);
        QCOMPARE(assignments->size(), 2);
        QCOMPARE(assignments->at(0).classId, firstClassId);
        QCOMPARE(assignments->at(0).teacherId, 11);
        QCOMPARE(assignments->at(1).classId, secondClassId);
        QCOMPARE(assignments->at(1).teacherId, -1);

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::returnsEmptyTestingClassListWhenNoneExist()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_empty_list");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        TestingClassRepository repository(database);
        const auto testingClasses = repository.loadTestingClasses();
        QVERIFY(testingClasses);
        QCOMPARE(testingClasses->size(), 0);

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests
    ::rejectsPartialClassTeacherAssignmentsAfterCursorError()
{
    const QString connectionName =
        QStringLiteral("testing_class_repository_assignment_cursor_error");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));

        ClassRepository classRepository(database);
        const int firstClassId =
            classRepository.createClass(QStringLiteral("Alpha")).value_or(-1);
        const int secondClassId =
            classRepository.createClass(QStringLiteral("Beta")).value_or(-1);
        QVERIFY(firstClassId > 0);
        QVERIFY(secondClassId > firstClassId);

        QSqlQuery setupQuery(database);
        QVERIFY(setupQuery.exec(QStringLiteral("DROP TABLE class_info")));
        QVERIFY(setupQuery.exec(QStringLiteral(R"(
            CREATE TABLE class_info_json_backing (
                class_id INTEGER PRIMARY KEY,
                teacher_json TEXT NOT NULL
            )
        )")));
        QVERIFY(setupQuery.prepare(QStringLiteral(R"(
            INSERT INTO class_info_json_backing (class_id, teacher_json)
            VALUES (?, ?), (?, ?)
        )")));
        setupQuery.addBindValue(firstClassId);
        setupQuery.addBindValue(QStringLiteral("{\"teacher_id\":11}"));
        setupQuery.addBindValue(secondClassId);
        setupQuery.addBindValue(QStringLiteral("{"));
        QVERIFY(setupQuery.exec());
        QVERIFY(setupQuery.exec(QStringLiteral(R"(
            CREATE VIEW class_info AS
            SELECT
                class_id,
                json_extract(teacher_json, '$.teacher_id') AS teacher_id
            FROM class_info_json_backing
        )")));
        QVERIFY(setupQuery.exec(QStringLiteral(
            "CREATE INDEX classes_name_id_idx ON classes(name, id)"
            )));

        QSqlQuery cursorProbe(database);
        QVERIFY(cursorProbe.exec(QStringLiteral(R"(
            SELECT
                c.id AS class_id,
                ci.teacher_id
            FROM classes c
            LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
            LEFT JOIN class_info ci
            ON ci.class_id = c.id
            WHERE tc.class_id IS NULL
            ORDER BY c.name, c.id
        )")));
        QVERIFY(cursorProbe.next());
        QCOMPARE(cursorProbe.value(QStringLiteral("class_id")).toInt(), firstClassId);
        QCOMPARE(cursorProbe.value(QStringLiteral("teacher_id")).toInt(), 11);
        QVERIFY(!cursorProbe.next());
        QVERIFY(cursorProbe.lastError().isValid());
        const QString cursorError = cursorProbe.lastError().text().trimmed();
        QVERIFY(!cursorError.isEmpty());
        QVERIFY(cursorError.contains(QStringLiteral("malformed"), Qt::CaseInsensitive));

        ClassInfoRepository repository(database);
        const auto assignments = repository.loadClassTeacherAssignments();
        QVERIFY(!assignments);
        QCOMPARE(assignments.error().code, RepositoryErrorCode::Technical);
        QVERIFY(assignments.error().diagnostic.contains(
            QStringLiteral("Loading class teacher assignments failed")
            ));
        QVERIFY(assignments.error().diagnostic.contains(cursorError));

        const auto exactAssignments =
            repository.loadClassTeacherAssignmentRecords();
        QVERIFY(!exactAssignments);
        QCOMPARE(exactAssignments.error().code, RepositoryErrorCode::Technical);
        QVERIFY(exactAssignments.error().diagnostic.contains(cursorError));

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::
classInfoRepositoryPreservesErrorCategoriesAndParentOutcomes()
{
    const QString connectionName = QStringLiteral("class_info_typed_errors");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        ClassInfoRepository repository(database);

        const auto invalidRead = repository.loadClassInfo(0);
        QVERIFY(!invalidRead);
        QCOMPARE(invalidRead.error().code, RepositoryErrorCode::InvalidInput);
        QCOMPARE(toLegacyDiagnostic(invalidRead.error()), QStringLiteral(
            "Loading class information failed: invalid class id 0."));

        for (const int invalidId : {0, -1})
        {
            const auto conflicts = repository.getClassTimeConflicts(
                invalidId, {}, ScheduleType::Regular);
            QVERIFY(!conflicts);
            QCOMPARE(conflicts.error().code, RepositoryErrorCode::InvalidInput);
            QCOMPARE(conflicts.error().diagnostic, QStringLiteral(
                "Loading class time conflicts failed: invalid class id %1.").arg(invalidId));
        }

        const auto invalidParent = repository.checkClassDetailsParentDetailed(0);
        QVERIFY(!invalidParent);
        QCOMPARE(invalidParent.error().code, RepositoryErrorCode::InvalidInput);

        const auto missingNotes = repository.saveClassNotesDetailed(
            87, QStringLiteral("Draft"), QStringLiteral("Activities"));
        QVERIFY(!missingNotes);
        QCOMPARE(missingNotes.error().code, RepositoryErrorCode::NotFound);
        QCOMPARE(toLegacyDiagnostic(missingNotes.error()), QStringLiteral(
            "Saving class notes failed: parent class id 87 was not found."));
        QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);

        const auto missingSnapshot =
            repository.loadClassCoTeacherAssignmentValidationSnapshotDetailed(87);
        QVERIFY(!missingSnapshot);
        QCOMPARE(missingSnapshot.error().code, RepositoryErrorCode::NotFound);

        const auto absentGrade = repository.loadSelectedClassGradeRecordDetailed(87);
        QVERIFY(absentGrade);
        QVERIFY(absentGrade->parentMissing);
        const auto legacyDefault = repository.loadClassInfo(87);
        QVERIFY(legacyDefault);
        QCOMPARE(legacyDefault->classId, 87);
        QVERIFY(legacyDefault->classGrade.isEmpty());

        const auto absentSubPrep = repository.loadSubPrepClassDetailsDetailed(87);
        QVERIFY(absentSubPrep);
        QVERIFY(absentSubPrep->parentMissing);
        const auto missingSubPrep = repository.loadSubPrepClassDetails(87);
        QVERIFY(!missingSubPrep);
        QCOMPARE(missingSubPrep.error().code, RepositoryErrorCode::NotFound);
        QCOMPARE(toLegacyDiagnostic(missingSubPrep.error()), QStringLiteral(
            "Loading selected Sub Prep class details failed: no matching record exists for class id 87."));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Regular')")));
        const auto missingTeacher = repository.saveClassCoTeacherAssignmentDetailed(
            1, qint64{99});
        QVERIFY(!missingTeacher);
        QCOMPARE(missingTeacher.error().code, RepositoryErrorCode::NotFound);
        QVERIFY(missingTeacher.error().recoverableOverride.has_value());
        QVERIFY(*missingTeacher.error().recoverableOverride);
        QCOMPARE(toLegacyDiagnostic(missingTeacher.error()), QStringLiteral(
            "Saving class information failed: teacher id 99 was not found."));
        QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);

        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id) VALUES (1, 'not-an-id')")));
        const auto malformedTeacher = repository.loadClassSubtitleRecord(1);
        QVERIFY(!malformedTeacher);
        QCOMPARE(malformedTeacher.error().code, RepositoryErrorCode::Validation);
        QCOMPARE(toLegacyDiagnostic(malformedTeacher.error()), QStringLiteral(
            "Loading class subtitle failed: the database returned an invalid teacher identifier."));
        const auto malformedSaveInfo = repository.loadClassDetailsSaveInfo(1);
        QVERIFY(!malformedSaveInfo);
        QCOMPARE(malformedSaveInfo.error().code, RepositoryErrorCode::Validation);

        QVERIFY(query.exec(QStringLiteral("DROP TABLE class_info")));
        const auto failedRead = repository.loadSelectedClassGradeRecordDetailed(1);
        QVERIFY(!failedRead);
        QCOMPARE(failedRead.error().code, RepositoryErrorCode::Technical);
        QVERIFY(failedRead.error().diagnostic.contains(
            QStringLiteral("Loading class information failed")));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::
classInfoVisitorsPreserveTypedFailuresAndRejectStoredIds()
{
    const QString connectionName = QStringLiteral("class_info_typed_visitors");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Regular')")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (1, 'Monday', '9:00 AM', '9:50 AM')")));
        ClassInfoRepository repository(database);
        const auto acceptClass = [](const ClassInfo&) -> RepositoryStatus
        {
            return {};
        };
        const auto acceptTime = [](int, const ClassTime&, bool) -> RepositoryStatus
        {
            return {};
        };

        const auto missingClassVisitor = repository.visitScheduleClassInfoRows({}, acceptTime);
        QVERIFY(!missingClassVisitor);
        QCOMPARE(missingClassVisitor.error().code, RepositoryErrorCode::InvalidInput);
        const auto missingTimeVisitor = repository.visitScheduleClassInfoRows(acceptClass, {});
        QVERIFY(!missingTimeVisitor);
        QCOMPARE(missingTimeVisitor.error().code, RepositoryErrorCode::InvalidInput);
        QCOMPARE(missingClassVisitor.error().diagnostic, QStringLiteral(
            "Loading schedule class information failed: both visitors are required."));
        QCOMPARE(repository.scheduleClassInfoReadMetrics().metadataStatementCount, 0);

        const auto classRejected = repository.visitScheduleClassInfoRows(
            [](const ClassInfo&) -> RepositoryStatus
            {
                return std::unexpected(RepositoryError{
                    RepositoryErrorCode::Conflict,
                    QStringLiteral("Class visitor rejected the row.")
                });
            }, acceptTime);
        QVERIFY(!classRejected);
        QCOMPARE(classRejected.error().code, RepositoryErrorCode::Conflict);
        QCOMPARE(classRejected.error().diagnostic,
                 QStringLiteral("Class visitor rejected the row."));

        const auto timeRejected = repository.visitScheduleClassInfoRows(
            acceptClass,
            [](int, const ClassTime&, bool) -> RepositoryStatus
            {
                return std::unexpected(RepositoryError{
                    RepositoryErrorCode::NotFound,
                    QStringLiteral("Time visitor rejected the row.")
                });
            });
        QVERIFY(!timeRejected);
        QCOMPARE(timeRejected.error().code, RepositoryErrorCode::NotFound);
        QCOMPARE(timeRejected.error().diagnostic,
                 QStringLiteral("Time visitor rejected the row."));
        QCOMPARE(repository.scheduleClassInfoReadMetrics().completedClassCount, 0);
        QCOMPARE(repository.scheduleClassInfoReadMetrics().regularScheduleRowCount, 0);

        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (2147483648, 'Wide')")));
        const auto malformedId = repository.visitScheduleClassInfoRows(
            acceptClass, acceptTime);
        QVERIFY(!malformedId);
        QCOMPARE(malformedId.error().code, RepositoryErrorCode::Validation);
        QCOMPARE(malformedId.error().diagnostic, QStringLiteral(
            "Loading schedule class information failed: invalid class id 2147483648."));
        QCOMPARE(repository.scheduleClassInfoReadMetrics().completedClassCount, 0);

        QVERIFY(query.exec(QStringLiteral(
            "DELETE FROM classes WHERE id=2147483648")));
        const auto completed = repository.visitScheduleClassInfoRows(
            acceptClass, acceptTime);
        QVERIFY(completed);
        QCOMPARE(repository.scheduleClassInfoReadMetrics().completedClassCount, 1);
        QCOMPARE(repository.scheduleClassInfoReadMetrics().regularScheduleRowCount, 1);
        QCOMPARE(repository.scheduleClassInfoReadMetrics().intensiveScheduleRowCount, 0);
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::classInfoCoTeacherMissingParentsRollback()
{
    const QString connectionName = QStringLiteral("class_info_missing_assignment_parent");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("DROP TABLE class_info")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE class_info ("
            "class_id INTEGER PRIMARY KEY REFERENCES classes(id), "
            "teacher_id INTEGER REFERENCES teachers(id))")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO teachers (id) VALUES (7)")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO classes (id, name) VALUES (1, 'Regular')")));
        ClassInfoRepository repository(database);

        for (const bool enforceForeignKeys : {true, false})
        {
            QVERIFY(query.exec(enforceForeignKeys
                ? QStringLiteral("PRAGMA foreign_keys=ON")
                : QStringLiteral("PRAGMA foreign_keys=OFF")));
            QString expectedDiagnostic = QStringLiteral(
                "Saving class information failed: parent class id 87 was not found.");
            if (enforceForeignKeys)
            {
                QSqlQuery originalStatement(database);
                originalStatement.prepare(QStringLiteral(
                    "INSERT INTO class_info (class_id, teacher_id) VALUES (?, ?) "
                    "ON CONFLICT(class_id) DO UPDATE SET teacher_id=excluded.teacher_id"));
                originalStatement.addBindValue(87);
                originalStatement.addBindValue(7);
                QVERIFY(!originalStatement.exec());
                expectedDiagnostic = SqlQueryUtils::errorFor(
                    originalStatement, QStringLiteral("Saving class information"),
                    {}, QStringLiteral("class id 87")).userMessage();
            }

            for (const bool detailed : {true, false})
            {
                for (const auto teacherId : {std::optional<qint64>{7}, std::optional<qint64>{}})
                {
                    const auto saved = detailed
                        ? repository.saveClassCoTeacherAssignmentDetailed(87, teacherId)
                        : repository.saveClassCoTeacherAssignment(87, teacherId);
                    QVERIFY(!saved);
                    QCOMPARE(saved.error().code, RepositoryErrorCode::NotFound);
                    QVERIFY(saved.error().recoverableOverride.has_value());
                    QVERIFY(!*saved.error().recoverableOverride);
                    QCOMPARE(toLegacyDiagnostic(saved.error()), expectedDiagnostic);
                    QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
                    // The missing-parent path must close its transaction, including
                    // when the original statement succeeded with FK checks off.
                    QVERIFY(database.transaction());
                    QVERIFY(database.rollback());
                }
            }

            QString expectedTeacherDiagnostic = QStringLiteral(
                "Saving class information failed: teacher id 99 was not found.");
            if (enforceForeignKeys)
            {
                QSqlQuery originalStatement(database);
                originalStatement.prepare(QStringLiteral(
                    "INSERT INTO class_info (class_id, teacher_id) VALUES (?, ?) "
                    "ON CONFLICT(class_id) DO UPDATE SET teacher_id=excluded.teacher_id"));
                originalStatement.addBindValue(1);
                originalStatement.addBindValue(99);
                QVERIFY(!originalStatement.exec());
                expectedTeacherDiagnostic = SqlQueryUtils::errorFor(
                    originalStatement, QStringLiteral("Saving class information"),
                    {}, QStringLiteral("class id 1")).userMessage();
            }
            for (const bool detailed : {true, false})
            {
                const auto missingTeacher = detailed
                    ? repository.saveClassCoTeacherAssignmentDetailed(1, qint64{99})
                    : repository.saveClassCoTeacherAssignment(1, qint64{99});
                QVERIFY(!missingTeacher);
                QCOMPARE(missingTeacher.error().code, RepositoryErrorCode::NotFound);
                QVERIFY(missingTeacher.error().recoverableOverride.has_value());
                QVERIFY(*missingTeacher.error().recoverableOverride);
                QCOMPARE(toLegacyDiagnostic(missingTeacher.error()), detailed
                    ? QStringLiteral("Saving class information failed: teacher id 99 was not found.")
                    : expectedTeacherDiagnostic);
                QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
                QVERIFY(database.transaction());
                QVERIFY(database.rollback());

                QVERIFY(query.exec(QStringLiteral(
                    "INSERT INTO class_info (class_id, teacher_id) VALUES (1, 7)")));
                const auto rejectedUpdate = detailed
                    ? repository.saveClassCoTeacherAssignmentDetailed(1, qint64{99})
                    : repository.saveClassCoTeacherAssignment(1, qint64{99});
                QVERIFY(!rejectedUpdate);
                QCOMPARE(rejectedUpdate.error().code, RepositoryErrorCode::NotFound);
                QVERIFY(rejectedUpdate.error().recoverableOverride.has_value());
                QVERIFY(*rejectedUpdate.error().recoverableOverride);
                QVERIFY(query.exec(QStringLiteral(
                    "SELECT teacher_id FROM class_info WHERE class_id=1")));
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toLongLong(), qint64{7});
                query.finish();
                QVERIFY(query.exec(QStringLiteral("DELETE FROM class_info WHERE class_id=1")));
            }

            const auto missingTeacher = repository.saveClassCoTeacherAssignmentDetailed(
                87, qint64{99});
            QVERIFY(!missingTeacher);
            QCOMPARE(missingTeacher.error().code, RepositoryErrorCode::NotFound);
            QVERIFY(missingTeacher.error().recoverableOverride.has_value());
            QVERIFY(*missingTeacher.error().recoverableOverride);
            QCOMPARE(missingTeacher.error().diagnostic, QStringLiteral(
                "Saving class information failed: teacher id 99 was not found."));
            QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
            QVERIFY(database.transaction());
            QVERIFY(database.rollback());
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::classInfoReadersValidateTeacherIdWidths()
{
    const QString connectionName = QStringLiteral("class_info_teacher_id_widths");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Regular')")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_info (class_id) VALUES (1)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (1, 'Monday', '9:00 AM', '9:50 AM')")));
        ClassInfoRepository repository(database);
        const auto acceptTime = [](int, const ClassTime&, bool) -> RepositoryStatus
        {
            return {};
        };

        const QStringList malformedIds{
            QStringLiteral("not-an-id"), QStringLiteral("2147483648"),
            QStringLiteral("-2147483649"), QStringLiteral("9223372036854775807"),
            QStringLiteral("-9223372036854775808"),
            QStringLiteral("9223372036854775808"), QStringLiteral("-9223372036854775809")
        };
        for (const QString& storedId : malformedIds)
        {
            query.prepare(QStringLiteral("UPDATE class_info SET teacher_id=? WHERE class_id=1"));
            query.addBindValue(storedId);
            QVERIFY(query.exec());
            const auto info = repository.loadClassInfo(1);
            QVERIFY(!info);
            QCOMPARE(info.error().code, RepositoryErrorCode::Validation);
            QCOMPARE(info.error().diagnostic, QStringLiteral(
                "Loading class information failed: invalid assigned teacher identifier."));
            const auto assignments = repository.loadClassTeacherAssignments();
            QVERIFY(!assignments);
            QCOMPARE(assignments.error().code, RepositoryErrorCode::Validation);
            const auto schedule = repository.loadScheduleClassInfos();
            QVERIFY(!schedule);
            QCOMPARE(schedule.error().code, RepositoryErrorCode::Validation);
            int visitedCount = 0;
            const auto visited = repository.visitScheduleClassInfoRows(
                [&visitedCount](const ClassInfo&) -> RepositoryStatus
                {
                    ++visitedCount;
                    return {};
                }, acceptTime);
            QVERIFY(!visited);
            QCOMPARE(visited.error().code, RepositoryErrorCode::Validation);
            QCOMPARE(visitedCount, 0);

            const auto scoped = repository.loadClassInfosForScheduleScope(
                {1}, {QStringLiteral("Monday")}, ScheduleType::Regular, 5, 5, true);
            QVERIFY(!scoped);
            QCOMPARE(scoped.error().code, RepositoryErrorCode::Validation);
            const auto exactAssignments = repository.loadClassTeacherAssignmentRecords();
            const auto saveInfo = repository.loadClassDetailsSaveInfo(1);
            const auto page = repository.loadClassPageDetails(1);
            const auto detailedPage = repository.loadClassPageDetailsDetailed(1);
            const auto subPrep = repository.loadSubPrepClassDetailsDetailed(1);
            const auto snapshot = repository.loadScheduleImportStateSnapshotClassInfos();
            if (storedId == QStringLiteral("2147483648")
                || storedId == QStringLiteral("-2147483649")
                || storedId == QStringLiteral("9223372036854775807")
                || storedId == QStringLiteral("-9223372036854775808"))
            {
                QVERIFY(exactAssignments);
                QCOMPARE(exactAssignments->size(), 1);
                QCOMPARE(exactAssignments->first().teacherId, storedId.toLongLong());
                QVERIFY(saveInfo);
                QCOMPARE(saveInfo->teacherId, storedId.toLongLong());
                QCOMPARE(saveInfo->info.teacherId,
                         storedId.startsWith(QLatin1Char('-')) ? -2 : -1);
                QVERIFY(page);
                QCOMPARE(page->teacherId, storedId.toLongLong());
                QVERIFY(detailedPage);
                QCOMPARE(detailedPage->record.teacherId, storedId.toLongLong());
                QVERIFY(!detailedPage->parentMissing);
                QVERIFY(subPrep);
                QCOMPARE(subPrep->record.teacherId, qint64{-1});
                QVERIFY(subPrep->record.teacherKr.isEmpty());
                QVERIFY(!subPrep->parentMissing);
                QVERIFY(snapshot);
                QCOMPARE(snapshot->first().teacherId, storedId.toLongLong());
            }
            else
            {
                QVERIFY(!exactAssignments);
                QCOMPARE(exactAssignments.error().code, RepositoryErrorCode::Validation);
                QVERIFY(!saveInfo);
                QCOMPARE(saveInfo.error().code, RepositoryErrorCode::Validation);
                QVERIFY(!page);
                QCOMPARE(page.error().code, RepositoryErrorCode::Validation);
                QVERIFY(!detailedPage);
                QCOMPARE(detailedPage.error().code, RepositoryErrorCode::Validation);
                QVERIFY(!subPrep);
                QCOMPARE(subPrep.error().code, RepositoryErrorCode::Validation);
                QVERIFY(!snapshot);
                QCOMPARE(snapshot.error().code, RepositoryErrorCode::Validation);
            }
        }
        QCOMPARE(repository.scheduleClassInfoReadMetrics().completedClassCount, 0);

        // Preserve NULL/empty/unassigned sentinels and valid assignments even
        // when their optional teacher-profile join has no matching row.
        for (const QVariant& storedId : {QVariant(), QVariant(QStringLiteral("")),
                QVariant(-1), QVariant(0), QVariant(7),
                QVariant(std::numeric_limits<int>::min()),
                QVariant(std::numeric_limits<int>::max())})
        {
            query.prepare(QStringLiteral("UPDATE class_info SET teacher_id=? WHERE class_id=1"));
            query.addBindValue(storedId);
            QVERIFY(query.exec());
            const int expectedId = storedId.isNull() ? -1 : storedId.toInt();
            const auto info = repository.loadClassInfo(1);
            QVERIFY(info);
            QCOMPARE(info->teacherId, expectedId);
            const auto assignments = repository.loadClassTeacherAssignments();
            QVERIFY(assignments);
            QCOMPARE(assignments->first().teacherId, expectedId);
            const auto exactAssignments = repository.loadClassTeacherAssignmentRecords();
            QVERIFY(exactAssignments);
            QCOMPARE(exactAssignments->first().teacherId, qint64{expectedId});
            const auto page = repository.loadClassPageDetails(1);
            QVERIFY(page);
            QCOMPARE(page->teacherId, qint64{expectedId});
            const auto detailedPage = repository.loadClassPageDetailsDetailed(1);
            QVERIFY(detailedPage);
            QVERIFY(!detailedPage->parentMissing);
            QCOMPARE(detailedPage->record.teacherId, qint64{expectedId});
            const auto subPrep = repository.loadSubPrepClassDetailsDetailed(1);
            QVERIFY(subPrep);
            QVERIFY(!subPrep->parentMissing);
            QCOMPARE(subPrep->record.teacherId, qint64{-1});
            QVERIFY(subPrep->record.teacherKr.isEmpty());
            const auto scoped = repository.loadClassInfosForScheduleScope(
                {1}, {QStringLiteral("Monday")}, ScheduleType::Regular, 5, 5, true);
            QVERIFY(scoped);
            QCOMPARE(scoped->size(), 1);
            QCOMPARE(scoped->first().teacherId, expectedId);
            const auto schedule = repository.loadScheduleClassInfos();
            QVERIFY(schedule);
            QCOMPARE(schedule->first().teacherId, expectedId);
            int visitedCount = 0;
            const auto visited = repository.visitScheduleClassInfoRows(
                [&](const ClassInfo& record) -> RepositoryStatus
                {
                    ++visitedCount;
                    if (record.teacherId != expectedId)
                    {
                        return std::unexpected(RepositoryError{
                            RepositoryErrorCode::Validation,
                            QStringLiteral("Teacher sentinel was changed.")});
                    }
                    return {};
                }, acceptTime);
            QVERIFY(visited);
            QCOMPARE(visitedCount, 1);
        }
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr) VALUES (7, 'Resolved teacher')")));
        QVERIFY(query.exec(QStringLiteral("UPDATE class_info SET teacher_id=7 WHERE class_id=1")));
        const auto resolvedSubPrep = repository.loadSubPrepClassDetailsDetailed(1);
        QVERIFY(resolvedSubPrep);
        QCOMPARE(resolvedSubPrep->record.teacherId, qint64{7});
        QCOMPARE(resolvedSubPrep->record.teacherKr, QStringLiteral("Resolved teacher"));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::classInfoSavesReportCheckedRollbackFailures()
{
    const QString connectionName = QStringLiteral("class_info_checked_rollback");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Regular')")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_info (class_id, notes) VALUES (1, 'Original')")));
        ClassInfoRepository repository(database);
        ClassInfo info;
        info.classId = 1;
        info.notes = QStringLiteral("Replacement");

        for (const QString& triggerAction : {QStringLiteral("ABORT"), QStringLiteral("ROLLBACK")})
        {
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TRIGGER reject_class_info_update BEFORE UPDATE ON class_info "
                "BEGIN SELECT RAISE(%1, 'injected save failure'); END").arg(triggerAction)));
            for (const bool notesOnly : {true, false})
            {
                const auto saved = notesOnly
                    ? repository.saveClassNotesDetailed(1, info.notes, QStringLiteral("Activities"))
                    : repository.saveClassDetailsDetailed(info, -1);
                QVERIFY(!saved);
                QCOMPARE(saved.error().code, RepositoryErrorCode::Technical);
                QVERIFY(saved.error().diagnostic.contains(QStringLiteral("injected save failure")));
                QVERIFY(saved.error().diagnostic.startsWith(notesOnly
                    ? QStringLiteral("Saving class notes failed")
                    : QStringLiteral("Saving class information failed")));
                QCOMPARE(saved.error().diagnostic.contains(notesOnly
                    ? QStringLiteral("Rolling back class notes save failed for class id 1:")
                    : QStringLiteral("Rolling back class information save failed for class id 1:")),
                    triggerAction == QStringLiteral("ROLLBACK"));
                QVERIFY(query.exec(QStringLiteral("SELECT notes FROM class_info WHERE class_id=1")));
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toString(), QStringLiteral("Original"));
                query.finish();
                QVERIFY(database.transaction());
                QVERIFY(database.rollback());
            }
            QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_class_info_update")));
        }

        // Parent-query SQL errors also release the transaction explicitly.
        QVERIFY(query.exec(QStringLiteral("DROP TABLE classes")));
        const auto failedNotesParent = repository.saveClassNotesDetailed(1, info.notes, QString());
        QVERIFY(!failedNotesParent);
        QCOMPARE(failedNotesParent.error().code, RepositoryErrorCode::Technical);
        QVERIFY(failedNotesParent.error().diagnostic.startsWith(
            QStringLiteral("Checking class notes parent class failed")));
        QVERIFY(database.transaction());
        QVERIFY(database.rollback());
        const auto failedDetailsParent = repository.saveClassDetailsDetailed(info, -1);
        QVERIFY(!failedDetailsParent);
        QCOMPARE(failedDetailsParent.error().code, RepositoryErrorCode::Technical);
        QVERIFY(failedDetailsParent.error().diagnostic.startsWith(
            QStringLiteral("Checking class details parent class failed")));
        QVERIFY(database.transaction());
        QVERIFY(database.rollback());
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::classInfoReadersRejectMalformedClassIds()
{
    const QString connectionName = QStringLiteral("class_info_persisted_class_id_validation");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        QSqlQuery query(database);
        // A permissive legacy schema exposes invalid persisted values that an
        // INTEGER PRIMARY KEY would otherwise reject before the read.
        QVERIFY(query.exec(QStringLiteral("DROP TABLE classes")));
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE classes (id, name TEXT)")));
        ClassInfoRepository repository(database);
        for (const QVariant& storedId : {
                QVariant(QStringLiteral("not-an-id")), QVariant(qint64{2147483648LL}),
                QVariant(qint64{-2147483649LL}), QVariant(0), QVariant(-1),
                QVariant(QStringLiteral("9223372036854775808")), QVariant()})
        {
            QVERIFY(query.exec(QStringLiteral("DELETE FROM classes")));
            query.prepare(QStringLiteral("INSERT INTO classes (id, name) VALUES (?, 'Regular')"));
            query.addBindValue(storedId);
            QVERIFY(query.exec());
            const auto assignments = repository.loadClassTeacherAssignments();
            QVERIFY(!assignments);
            QCOMPARE(assignments.error().code, RepositoryErrorCode::Validation);
            const auto exactAssignments = repository.loadClassTeacherAssignmentRecords();
            QVERIFY(!exactAssignments);
            QCOMPARE(exactAssignments.error().code, RepositoryErrorCode::Validation);
            const auto snapshot = repository.loadScheduleImportStateSnapshotClassInfos();
            QVERIFY(!snapshot);
            QCOMPARE(snapshot.error().code, RepositoryErrorCode::Validation);
        }

        QVERIFY(query.exec(QStringLiteral("DELETE FROM classes")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (2147483647, 'Maximum')")));
        const auto assignments = repository.loadClassTeacherAssignments();
        QVERIFY(assignments);
        QCOMPARE(assignments->size(), 1);
        QCOMPARE(assignments->first().classId, std::numeric_limits<int>::max());
        const auto exactAssignments = repository.loadClassTeacherAssignmentRecords();
        QVERIFY(exactAssignments);
        QCOMPARE(exactAssignments->first().classId, std::numeric_limits<int>::max());
        const auto snapshot = repository.loadScheduleImportStateSnapshotClassInfos();
        QVERIFY(snapshot);
        QCOMPARE(snapshot->first().classId, std::numeric_limits<int>::max());

        // The class key in each schedule row must be checked independently of
        // the valid parent metadata; SQLite's numeric join can match this text.
        QVERIFY(query.exec(QStringLiteral("DROP TABLE classes")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE classes (id INTEGER PRIMARY KEY, name TEXT)")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO classes VALUES (1, 'Regular')")));
        QVERIFY(query.exec(QStringLiteral("DROP TABLE class_times")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE VIEW class_times AS SELECT 1 AS id, '1.0' AS class_id, "
            "'Monday' AS day, '9:00 AM' AS start_time, '9:50 AM' AS end_time")));
        const auto schedule = repository.loadScheduleClassInfos();
        QVERIFY(!schedule);
        QCOMPARE(schedule.error().code, RepositoryErrorCode::Validation);
        const auto scheduleSnapshot = repository.loadScheduleImportStateSnapshotClassInfos();
        QVERIFY(!scheduleSnapshot);
        QCOMPARE(scheduleSnapshot.error().code, RepositoryErrorCode::Validation);
        const auto visited = repository.visitScheduleClassInfoRows(
            [](const ClassInfo&) -> RepositoryStatus { return {}; },
            [](int, const ClassTime&, bool) -> RepositoryStatus { return {}; });
        QVERIFY(!visited);
        QCOMPARE(visited.error().code, RepositoryErrorCode::Validation);
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestingClassRepositoryTests::classInfoCoTeacherCommitFailuresRollback()
{
    const QString connectionName = QStringLiteral("class_info_assignment_commit_failure");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createSchema(database));
        QVERIFY(addClassInfoQuerySchema(database));
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO classes (id, name) VALUES (1, 'Regular')")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO teachers (id) VALUES (7)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE deferred_assignment_check (teacher_id INTEGER, "
            "FOREIGN KEY(teacher_id) REFERENCES teachers(id) DEFERRABLE INITIALLY DEFERRED)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER fail_assignment_commit AFTER INSERT ON class_info "
            "BEGIN INSERT INTO deferred_assignment_check VALUES (99); END")));
        ClassInfoRepository repository(database);
        for (const bool detailed : {true, false})
        {
            const auto saved = detailed
                ? repository.saveClassCoTeacherAssignmentDetailed(1, qint64{7})
                : repository.saveClassCoTeacherAssignment(1, qint64{7});
            QVERIFY(!saved);
            QCOMPARE(saved.error().code, RepositoryErrorCode::Technical);
            QVERIFY(saved.error().diagnostic.startsWith(QStringLiteral(
                "Committing class information failed for class id 1:")));
            QVERIFY(saved.error().diagnostic.contains(QStringLiteral("FOREIGN KEY")));
            QVERIFY(!saved.error().diagnostic.contains(QStringLiteral("Rolling back")));
            QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
            QCOMPARE(rowCount(database, QStringLiteral("deferred_assignment_check")), 0);
            QVERIFY(database.transaction());
            QVERIFY(database.rollback());
        }

        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER fail_assignment_commit")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER rollback_assignment_write BEFORE INSERT ON class_info "
            "BEGIN SELECT RAISE(ROLLBACK, 'injected assignment rollback'); END")));
        for (const bool detailed : {true, false})
        {
            const auto saved = detailed
                ? repository.saveClassCoTeacherAssignmentDetailed(1, qint64{7})
                : repository.saveClassCoTeacherAssignment(1, qint64{7});
            QVERIFY(!saved);
            QCOMPARE(saved.error().code, RepositoryErrorCode::Technical);
            QVERIFY(saved.error().diagnostic.startsWith(QStringLiteral(
                "Saving class information failed for class id 1:")));
            QVERIFY(saved.error().diagnostic.contains(QStringLiteral("injected assignment rollback")));
            QVERIFY(saved.error().diagnostic.contains(QStringLiteral(
                "Rolling back class information save failed for class id 1:")));
            QCOMPARE(rowCount(database, QStringLiteral("class_info")), 0);
            QVERIFY(database.transaction());
            QVERIFY(database.rollback());
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(TestingClassRepositoryTests)

#include "testing_class_repository_tests.moc"
