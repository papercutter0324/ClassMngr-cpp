#include "data/repositories/testing_block_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class TestingBlockRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void savesUpdatesDeletesAndClearsBlocks();
    void assignsTestingClassesAndRequiresExplicitReplacement();
    void reportsInvalidKeysAndDatabaseFailures();
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
    QTemporaryDir profileDirectory;
    QVERIFY(profileDirectory.isValid());

    const QString connectionName =
        QStringLiteral("testing_assignment_repository_crud");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            profileDirectory.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(createTable(database));

        QSqlQuery schemaVersionQuery(database);
        QVERIFY(schemaVersionQuery.exec(
            QStringLiteral("PRAGMA user_version = 6")
            ));

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
                teacher_id INTEGER,
                class_grade TEXT,
                class_level TEXT,
                class_color TEXT,
                font_color TEXT,
                notes TEXT
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

        QVERIFY(
            !repository.saveTestingBlock(
                QStringLiteral("Monday"),
                QStringLiteral("16:00"),
                QStringLiteral("405")
                )
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
        QVERIFY(
            !repository.assignTestingClass(
                QStringLiteral("Wednesday"),
                QStringLiteral("16:00"),
                44
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

void TestingBlockRepositoryTests::savesUpdatesDeletesAndClearsBlocks()
{
    QTemporaryDir profileDirectory;
    QVERIFY(profileDirectory.isValid());

    const QString connectionName =
        QStringLiteral("testing_block_repository_crud");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            profileDirectory.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(createTable(database));

        QSqlQuery schemaVersionQuery(database);
        QVERIFY(schemaVersionQuery.exec(
            QStringLiteral("PRAGMA user_version = 6")
            ));

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
    QTemporaryDir profileDirectory;
    QVERIFY(profileDirectory.isValid());

    const QString connectionName =
        QStringLiteral("testing_block_repository_errors");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            profileDirectory.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(
            QStringLiteral(
                "CREATE VIEW schedule_testing_blocks AS SELECT 1 AS id"
                )
            ));

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

QTEST_MAIN(TestingBlockRepositoryTests)

#include "testing_block_repository_tests.moc"
