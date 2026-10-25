#include "data/repositories/intensive_slot_state_repository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

class IntensiveSlotStateRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultStateControlsStoredRows();
    void readFailuresAreReturned();
    void terminalCursorFailuresAreReturnedWithoutPartialStates();
    void writeFailuresAreReturned();
};

void IntensiveSlotStateRepositoryTests::defaultStateControlsStoredRows()
{
    const QString connectionName =
        QStringLiteral("intensive_slot_state_repository_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );

        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(
            query.exec(R"(
                CREATE TABLE intensive_slot_states (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    day TEXT,
                    start_time TEXT,
                    state TEXT,
                    UNIQUE(day, start_time)
                )
            )")
            );

        IntensiveSlotStateRepository repository(database);

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay")
            ).has_value());

        const RepositoryResult<QList<IntensiveSlotState>> noStoredStates =
            repository.loadIntensiveSlotStates();
        QVERIFY(noStoredStates);
        QVERIFY(noStoredStates->isEmpty());

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("lunch")
            ).has_value());

        RepositoryResult<QList<IntensiveSlotState>> loadedStates =
            repository.loadIntensiveSlotStates();
        QVERIFY(loadedStates);
        QList<IntensiveSlotState> states = *loadedStates;

        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().state, QStringLiteral("lunch"));

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay")
            ).has_value());

        loadedStates = repository.loadIntensiveSlotStates();
        QVERIFY(loadedStates);
        QVERIFY(loadedStates->isEmpty());

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay"),
            QStringLiteral("empty")
            ).has_value());

        loadedStates = repository.loadIntensiveSlotStates();
        QVERIFY(loadedStates);
        states = *loadedStates;

        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().state, QStringLiteral("essay"));

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("empty"),
            QStringLiteral("empty")
            ).has_value());

        loadedStates = repository.loadIntensiveSlotStates();
        QVERIFY(loadedStates);
        QVERIFY(loadedStates->isEmpty());
    }

    QSqlDatabase::removeDatabase(
        connectionName
        );
}

void IntensiveSlotStateRepositoryTests::readFailuresAreReturned()
{
    const QString connectionName =
        QStringLiteral("intensive_slot_state_repository_read_failure_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        IntensiveSlotStateRepository repository(database);
        const RepositoryResult<QList<IntensiveSlotState>> states =
            repository.loadIntensiveSlotStates();
        QVERIFY(!states);
        QCOMPARE(states.error().code, RepositoryErrorCode::Technical);
        QVERIFY(states.error().diagnostic.contains(
            QStringLiteral("Loading intensive slot states")
            ));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void IntensiveSlotStateRepositoryTests
    ::terminalCursorFailuresAreReturnedWithoutPartialStates()
{
    const QString connectionName =
        QStringLiteral("intensive_slot_state_repository_cursor_failure_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery setup(database);
        QVERIFY(setup.exec(R"(
            CREATE TABLE intensive_slot_states_json_backing (
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                state_json TEXT NOT NULL
            )
        )"));
        QVERIFY(setup.exec(R"(
            CREATE INDEX intensive_slot_states_json_backing_order
            ON intensive_slot_states_json_backing (day, start_time)
        )"));
        QVERIFY(setup.exec(R"(
            INSERT INTO intensive_slot_states_json_backing (
                day, start_time, state_json
            ) VALUES
                (
                    'Monday',
                    '09:00',
                    '{"day":"Monday","start_time":"09:00","state":"essay"}'
                ),
                ('Monday', '10:00', '{')
        )"));
        QVERIFY(setup.exec(R"(
            CREATE VIEW intensive_slot_states AS
            SELECT
                day,
                start_time,
                json_extract(state_json, '$.state') AS state
            FROM intensive_slot_states_json_backing
            INDEXED BY intensive_slot_states_json_backing_order
        )"));

        QString cursorDetail;
        {
            QSqlQuery productionSelect(database);
            QVERIFY(productionSelect.exec(QStringLiteral(R"(
                SELECT
                    day,
                    start_time,
                    state
                FROM intensive_slot_states
                ORDER BY day, start_time
            )")));
            QVERIFY(productionSelect.next());
            QCOMPARE(
                productionSelect.value(QStringLiteral("day")).toString(),
                QStringLiteral("Monday")
                );
            QCOMPARE(
                productionSelect.value(QStringLiteral("start_time")).toString(),
                QStringLiteral("09:00")
                );
            QCOMPARE(
                productionSelect.value(QStringLiteral("state")).toString(),
                QStringLiteral("essay")
                );
            QVERIFY(!productionSelect.next());

            const QSqlError cursorError = productionSelect.lastError();
            QVERIFY(cursorError.isValid());
            cursorDetail = cursorError.text().trimmed();
            QVERIFY(!cursorDetail.isEmpty());
        }

        IntensiveSlotStateRepository repository(database);
        const RepositoryResult<QList<IntensiveSlotState>> states =
            repository.loadIntensiveSlotStates();
        QVERIFY(!states);
        QCOMPARE(states.error().code, RepositoryErrorCode::Technical);
        QVERIFY(states.error().diagnostic.contains(
            QStringLiteral("Loading intensive slot states")
            ));
        QVERIFY2(
            states.error().diagnostic.contains(cursorDetail),
            qPrintable(states.error().diagnostic)
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void IntensiveSlotStateRepositoryTests::writeFailuresAreReturned()
{
    const QString connectionName =
        QStringLiteral("intensive_slot_state_repository_failure_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(R"(
            CREATE TABLE intensive_slot_states (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                day TEXT,
                start_time TEXT,
                state TEXT,
                UNIQUE(day, start_time)
            )
        )"));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER reject_slot_insert "
            "BEFORE INSERT ON intensive_slot_states "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected insert failure'); "
            "END"
            )));

        IntensiveSlotStateRepository repository(database);
        const RepositoryStatus inserted = repository.saveIntensiveSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            QStringLiteral("lunch")
            );
        QVERIFY(!inserted);
        QCOMPARE(inserted.error().code, RepositoryErrorCode::Technical);
        QVERIFY(inserted.error().diagnostic.contains(
            QStringLiteral("Saving intensive slot state")
            ));
        QVERIFY(inserted.error().diagnostic.contains(
            QStringLiteral("Monday at 09:00")
            ));

        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_slot_insert")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO intensive_slot_states (day, start_time, state) "
            "VALUES ('Monday', '09:00', 'lunch')"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER reject_slot_delete "
            "BEFORE DELETE ON intensive_slot_states "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected delete failure'); "
            "END"
            )));

        const RepositoryStatus deleted = repository.saveIntensiveSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            QStringLiteral("essay")
            );
        QVERIFY(!deleted);
        QCOMPARE(deleted.error().code, RepositoryErrorCode::Technical);
        QVERIFY(deleted.error().diagnostic.contains(
            QStringLiteral("Deleting intensive slot state")
            ));
        const RepositoryResult<QList<IntensiveSlotState>> states =
            repository.loadIntensiveSlotStates();
        QVERIFY(states);
        QCOMPARE(states->size(), 1);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(IntensiveSlotStateRepositoryTests)

#include "intensive_slot_state_repository_tests.moc"
