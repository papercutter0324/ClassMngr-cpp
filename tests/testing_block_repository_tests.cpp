#include "data/repositories/testing_block_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

#include <limits>

class TestingBlockRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void savesUpdatesDeletesAndClearsBlocks();
    void assignsTestingClassesAndRequiresExplicitReplacement();
    void classifiesMissingParentAndIncompleteClassSeparately();
    void reportsInvalidKeysAndDatabaseFailures();
    void rejectsMalformedClassIdsWithoutOverwritingBlocks();
};

namespace
{
bool createTable(
    QSqlDatabase& database
    )
{
    QSqlQuery query(database);
    return query.exec(R"(
        CREATE TABLE schedule_testing_blocks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            day TEXT NOT NULL,
            start_time TEXT NOT NULL,
            room TEXT NOT NULL DEFAULT '',
            class_id INTEGER,
            UNIQUE(day, start_time)
        )
    )");
}
}

void TestingBlockRepositoryTests
    ::assignsTestingClassesAndRequiresExplicitReplacement()
{
    const QString connectionName =
        QStringLiteral("testing_assignment_repository_crud");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createTable(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(R"(
            CREATE TABLE testing_classes (
                class_id INTEGER PRIMARY KEY,
                room TEXT NOT NULL
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE classes (
                id INTEGER PRIMARY KEY,
                name TEXT
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE class_info (
                class_id INTEGER PRIMARY KEY,
                class_grade TEXT,
                class_level TEXT
            )
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO classes (id, name)
            VALUES (42, 'Writing Lab'), (43, 'Reading Lab')
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO class_info (
                class_id,
                class_grade,
                class_level
            )
            VALUES
                (42, 'M2', 'Mixed (Low)'),
                (43, 'M2', 'Mixed (High)')
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO testing_classes (class_id, room)
            VALUES (42, '402'), (43, 'Library')
        )"));

        TestingBlockRepository repository(database);
        QVERIFY(
            repository.assignTestingClass(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                42
                )
            );

        auto assignments =
            repository.loadTestingAssignments();
        QVERIFY(assignments);
        QCOMPARE(assignments->size(), 1);
        QCOMPARE(
            assignments->first().kind,
            TestingAssignmentKind::SpecialClass
            );
        QCOMPARE(assignments->first().classId, 42);
        QVERIFY(assignments->first().room.isEmpty());

        const auto rejectedReplacement =
            repository.saveTestingBlock(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                QStringLiteral("405")
                );
        QVERIFY(!rejectedReplacement);
        QVERIFY(
            rejectedReplacement.error().contains(
                QStringLiteral("Confirm replacement first.")
                )
            );

        QSqlQuery unchangedAssignment(database);
        QVERIFY(unchangedAssignment.exec(R"(
            SELECT room, class_id
            FROM schedule_testing_blocks
            WHERE day='Monday' AND start_time='16:00'
        )"));
        QVERIFY(unchangedAssignment.next());
        QVERIFY(
            unchangedAssignment.value(QStringLiteral("room")).toString().isEmpty()
            );
        QCOMPARE(
            unchangedAssignment.value(QStringLiteral("class_id")).toInt(),
            42
            );

        QVERIFY(
            repository.saveTestingBlock(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                QStringLiteral("405"),
                true
                )
            );

        assignments = repository.loadTestingAssignments();
        QVERIFY(assignments);
        QCOMPARE(
            assignments->first().kind,
            TestingAssignmentKind::PlainTesting
            );
        QCOMPARE(assignments->first().classId, -1);
        QCOMPARE(assignments->first().room, QStringLiteral("405"));

        QVERIFY(
            !repository.assignTestingClass(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                43
                )
            );
        QVERIFY(
            repository.assignTestingClass(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                43,
                true
                )
            );
        QVERIFY(
            repository.assignTestingClass(
                QStringLiteral("Tuesday"),
                QStringLiteral("16:00"),
                43
                )
            );
        QVERIFY(
            !repository.assignTestingClass(
                QStringLiteral("Wednesday"),
                QStringLiteral("16:00"),
                99
                )
            );
        QVERIFY(query.exec(R"(
            INSERT INTO classes (id, name)
            VALUES (44, 'Incomplete')
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO class_info (
                class_id,
                class_grade,
                class_level
            )
            VALUES (44, 'M2', '')
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO testing_classes (class_id, room)
            VALUES (44, '403')
        )"));
        const Status missingRequiredDetails = repository.assignTestingClass(
            QStringLiteral("Wednesday"),
            QStringLiteral("16:00"),
            44
            );
        QVERIFY(!missingRequiredDetails);
        QCOMPARE(
            missingRequiredDetails.error(),
            QStringLiteral(
                "The selected testing class is missing required details."
                )
            );

        QVERIFY(
            repository.deleteTestingAssignment(
                QStringLiteral("Monday"),
                QStringLiteral("16:00")
                )
            );
        assignments = repository.loadTestingAssignments();
        QVERIFY(assignments);
        QCOMPARE(assignments->size(), 1);
        QCOMPARE(assignments->first().classId, 43);

        QVERIFY(repository.clearTestingAssignments());
        assignments = repository.loadTestingAssignments();
        QVERIFY(assignments);
        QVERIFY(assignments->isEmpty());

        QVERIFY(query.exec(
            QStringLiteral(
                "SELECT COUNT(*) FROM testing_classes"
                )
            ));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 3);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingBlockRepositoryTests
    ::classifiesMissingParentAndIncompleteClassSeparately()
{
    const QString connectionName =
        QStringLiteral("testing_assignment_repository_classification");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createTable(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(R"(
            CREATE TABLE testing_classes (
                class_id INTEGER PRIMARY KEY,
                room TEXT NOT NULL
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE classes (
                id INTEGER PRIMARY KEY,
                name TEXT
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE class_info (
                class_id INTEGER PRIMARY KEY,
                class_grade TEXT,
                class_level TEXT
            )
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO classes (id, name)
            VALUES (42, 'Ordinary Class')
        )"));

        TestingBlockRepository repository(database);
        QVERIFY(repository.saveTestingBlock(
            QStringLiteral("Monday"),
            QStringLiteral("16:00"),
            QStringLiteral("Original")
            ));

        const auto missingParent = repository.assignTestingClassDetailed(
            QStringLiteral("Tuesday"),
            QStringLiteral("17:00"),
            2000000000
            );
        QVERIFY(!missingParent);
        QCOMPARE(
            missingParent.error().kind,
            TestingClassAssignmentSaveFailureKind::ParentClassMissing
            );
        QCOMPARE(
            missingParent.error().message,
            QStringLiteral("The selected testing class no longer exists.")
            );

        const auto incompleteClass = repository.assignTestingClassDetailed(
            QStringLiteral("Tuesday"),
            QStringLiteral("17:00"),
            42
            );
        QVERIFY(!incompleteClass);
        QCOMPARE(
            incompleteClass.error().kind,
            TestingClassAssignmentSaveFailureKind::Technical
            );
        QCOMPARE(
            incompleteClass.error().message,
            QStringLiteral("The selected testing class no longer exists.")
            );

        const Status legacyFailure = repository.assignTestingClass(
            QStringLiteral("Tuesday"),
            QStringLiteral("17:00"),
            42
            );
        QVERIFY(!legacyFailure);
        QCOMPARE(
            legacyFailure.error(),
            QStringLiteral("The selected testing class no longer exists.")
            );

        const auto assignments = repository.loadTestingAssignments();
        QVERIFY(assignments);
        QCOMPARE(assignments->size(), 1);
        QCOMPARE(assignments->first().day, QStringLiteral("Monday"));
        QCOMPARE(assignments->first().classId, -1);
        QCOMPARE(assignments->first().room, QStringLiteral("Original"));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingBlockRepositoryTests::savesUpdatesDeletesAndClearsBlocks()
{
    const QString connectionName =
        QStringLiteral("testing_block_repository_crud");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createTable(database));

        TestingBlockRepository repository(database);
        QVERIFY(
            repository.saveTestingBlock(
                QStringLiteral(" tUESday "),
                QStringLiteral(" 16:00 "),
                QStringLiteral(" 402 ")
                )
            );

        auto loaded =
            repository.loadTestingBlocks();
        QVERIFY(loaded);
        QCOMPARE(loaded->size(), 1);
        QCOMPARE(loaded->first().day, QStringLiteral("Tuesday"));
        QCOMPARE(loaded->first().startTime, QStringLiteral("16:00"));
        QCOMPARE(loaded->first().room, QStringLiteral("402"));

        QVERIFY(
            repository.saveTestingBlock(
                QStringLiteral("Tuesday"),
                QStringLiteral("16:00"),
                QStringLiteral("Library")
                )
            );
        QVERIFY(
            repository.saveTestingBlock(
                QStringLiteral("Wednesday"),
                QStringLiteral("17:00"),
                QString()
                )
            );

        loaded = repository.loadTestingBlocks();
        QVERIFY(loaded);
        QCOMPARE(loaded->size(), 2);
        QCOMPARE(loaded->first().room, QStringLiteral("Library"));
        QVERIFY(loaded->last().room.isEmpty());

        QVERIFY(
            repository.deleteTestingBlock(
                QStringLiteral("Tuesday"),
                QStringLiteral("16:00")
                )
            );
        loaded = repository.loadTestingBlocks();
        QVERIFY(loaded);
        QCOMPARE(loaded->size(), 1);

        QVERIFY(repository.clearTestingBlocks());
        loaded = repository.loadTestingBlocks();
        QVERIFY(loaded);
        QVERIFY(loaded->isEmpty());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingBlockRepositoryTests
    ::reportsInvalidKeysAndDatabaseFailures()
{
    const QString connectionName =
        QStringLiteral("testing_block_repository_errors");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        TestingBlockRepository repository(database);
        QVERIFY(
            !repository.saveTestingBlock(
                QStringLiteral("Funday"),
                QStringLiteral("16:00"),
                QString()
                )
            );
        QVERIFY(
            !repository.deleteTestingBlock(
                QStringLiteral("Monday"),
                QString()
                )
            );
        QVERIFY(
            !repository.saveTestingBlock(
                QStringLiteral("Monday"),
                QStringLiteral("4:00 PM"),
                QString()
                )
            );
        QVERIFY(!repository.loadTestingBlocks());
        QVERIFY(!repository.clearTestingBlocks());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void TestingBlockRepositoryTests::
rejectsMalformedClassIdsWithoutOverwritingBlocks()
{
    const QString connectionName =
        QStringLiteral("testing_block_repository_malformed_ids");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(createTable(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(R"(
            CREATE TABLE testing_classes (
                class_id INTEGER PRIMARY KEY,
                room TEXT NOT NULL
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE classes (
                id INTEGER PRIMARY KEY,
                name TEXT
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE class_info (
                class_id INTEGER PRIMARY KEY,
                class_grade TEXT,
                class_level TEXT,
                class_color TEXT,
                font_color TEXT,
                teacher_id INTEGER
            )
        )"));
        QVERIFY(query.exec(R"(
            CREATE TABLE teachers (
                id INTEGER PRIMARY KEY,
                teacher_kr TEXT,
                teacher_en TEXT,
                preferred_name TEXT
            )
        )"));

        TestingBlockRepository repository(database);
        struct MalformedClassId
        {
            QString description;
            QVariant value;
            QString storageType;
        };
        const QList<MalformedClassId> malformedValues{
            {
                QStringLiteral("non-numeric TEXT"),
                QStringLiteral("not-an-id"),
                QStringLiteral("text")
            },
            {
                QStringLiteral("fractional REAL"),
                42.5,
                QStringLiteral("real")
            },
            {
                QStringLiteral("zero INTEGER"),
                0,
                QStringLiteral("integer")
            },
            {
                QStringLiteral("negative INTEGER"),
                -42,
                QStringLiteral("integer")
            },
            {
                QStringLiteral("above INT_MAX INTEGER"),
                QVariant::fromValue<qlonglong>(
                    static_cast<qlonglong>(
                        std::numeric_limits<int>::max()
                        ) + 1
                    ),
                QStringLiteral("integer")
            }
        };

        for (const MalformedClassId& malformed : malformedValues)
        {
            QVERIFY2(
                query.exec(QStringLiteral(
                    "DELETE FROM schedule_testing_blocks"
                    )),
                qPrintable(malformed.description)
                );

            QSqlQuery insert(database);
            insert.prepare(QStringLiteral(R"(
                INSERT INTO schedule_testing_blocks (
                    day, start_time, room, class_id
                ) VALUES ('Monday', '16:00', 'Original', ?)
            )"));
            insert.addBindValue(malformed.value);
            QVERIFY2(insert.exec(), qPrintable(malformed.description));

            QSqlQuery before(database);
            QVERIFY2(before.exec(QStringLiteral(R"(
                SELECT room, class_id, typeof(class_id) AS storage_type
                FROM schedule_testing_blocks
                WHERE day='Monday' AND start_time='16:00'
            )")), qPrintable(malformed.description));
            QVERIFY(before.next());
            QCOMPARE(
                before.value(QStringLiteral("storage_type")).toString(),
                malformed.storageType
                );
            const QString persistedClassId =
                before.value(QStringLiteral("class_id")).toString();

            const auto assignments = repository.loadTestingAssignments();
            QVERIFY2(!assignments, qPrintable(malformed.description));
            QVERIFY(assignments.error().contains(
                QStringLiteral("invalid class ID"), Qt::CaseInsensitive));

            const auto displayRecords =
                repository.loadTestingAssignmentDisplayRecords();
            QVERIFY2(!displayRecords, qPrintable(malformed.description));
            QVERIFY(displayRecords.error().contains(
                QStringLiteral("invalid class ID"), Qt::CaseInsensitive));

            const auto save = repository.saveTestingBlock(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                QStringLiteral("Replacement")
                );
            QVERIFY2(!save, qPrintable(malformed.description));

            QSqlQuery after(database);
            QVERIFY(after.exec(QStringLiteral(R"(
                SELECT room, class_id, typeof(class_id) AS storage_type
                FROM schedule_testing_blocks
                WHERE day='Monday' AND start_time='16:00'
            )")));
            QVERIFY(after.next());
            QCOMPARE(
                after.value(QStringLiteral("room")).toString(),
                QStringLiteral("Original")
                );
            QCOMPARE(
                after.value(QStringLiteral("storage_type")).toString(),
                malformed.storageType
                );
            QCOMPARE(
                after.value(QStringLiteral("class_id")).toString(),
                persistedClassId
                );
        }

        QVERIFY(query.exec(QStringLiteral(
            "DELETE FROM schedule_testing_blocks"
            )));
        QVERIFY(query.exec(QStringLiteral(R"(
            INSERT INTO schedule_testing_blocks (
                day, start_time, room, class_id
            ) VALUES ('Monday', '16:00', '', 2147483647)
        )")));
        const auto maximumIdAssignments = repository.loadTestingAssignments();
        QVERIFY(maximumIdAssignments);
        QCOMPARE(maximumIdAssignments->size(), 1);
        QCOMPARE(
            maximumIdAssignments->first().classId,
            std::numeric_limits<int>::max()
            );
        const auto maximumIdDisplay =
            repository.loadTestingAssignmentDisplayRecords();
        QVERIFY(maximumIdDisplay);
        QCOMPARE(maximumIdDisplay->size(), 1);
        QCOMPARE(
            maximumIdDisplay->first().classId,
            std::numeric_limits<int>::max()
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(TestingBlockRepositoryTests)

#include "testing_block_repository_tests.moc"
