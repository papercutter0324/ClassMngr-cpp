#include "data/repositories/class_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/testing_class_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class TestingClassRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void createsUpdatesFiltersAndDeletesTestingClasses();
    void rejectsIncompleteTestingClasses();
    void persistsEverySupportedMixedLevel();
    void exposesTestingWorkspaceGradeAndLevelChoices();
    void createsClassAndAssignmentAtomically();
    void loadsRegularClassTeacherAssignmentsInOneSnapshot();
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
        const Result<int> regularClass =
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

        const Result<int> created =
            repository.createTestingClass(testingClass);
        QVERIFY(created);
        QVERIFY(*created > regularClassId);

        const Result<QList<Classroom>> regularClasses =
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
        QVERIFY(!repository.createTestingClass(testingClass));

        testingClass.name = QStringLiteral("Support");
        testingClass.grade = QStringLiteral("M1");
        testingClass.level = QStringLiteral("Mixed (High)");
        QVERIFY(!repository.createTestingClass(testingClass));

        testingClass.room = QStringLiteral("305");
        const Result<int> created =
            repository.createTestingClass(testingClass);
        QVERIFY(created);

        testingClass.classId = *created;
        testingClass.level.clear();
        QVERIFY(!repository.updateTestingClass(testingClass));
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

            const Result<int> created =
                repository.createTestingClass(testingClass);
            QVERIFY(created);

            const Result<TestingClass> loaded =
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

        const Result<int> created =
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

        testingClass.name = QStringLiteral("Must Roll Back");
        const Result<int> rejected =
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

QTEST_MAIN(TestingClassRepositoryTests)

#include "testing_class_repository_tests.moc"
