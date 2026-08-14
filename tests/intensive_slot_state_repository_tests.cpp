#include "data/repositories/intensive_slot_state_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class IntensiveSlotStateRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultStateControlsStoredRows();
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

        QVERIFY(repository.loadIntensiveSlotStates().isEmpty());

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("lunch")
            ).has_value());

        QList<IntensiveSlotState> states =
            repository.loadIntensiveSlotStates();

        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().state, QStringLiteral("lunch"));

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay")
            ).has_value());

        QVERIFY(repository.loadIntensiveSlotStates().isEmpty());

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay"),
            QStringLiteral("empty")
            ).has_value());

        states =
            repository.loadIntensiveSlotStates();

        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().state, QStringLiteral("essay"));

        QVERIFY(repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("empty"),
            QStringLiteral("empty")
            ).has_value());

        QVERIFY(repository.loadIntensiveSlotStates().isEmpty());
    }

    QSqlDatabase::removeDatabase(
        connectionName
        );
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
        const Status inserted = repository.saveIntensiveSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            QStringLiteral("lunch")
            );
        QVERIFY(!inserted);
        QVERIFY(inserted.error().contains(
            QStringLiteral("Saving intensive slot state")
            ));
        QVERIFY(inserted.error().contains(
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

        const Status deleted = repository.saveIntensiveSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            QStringLiteral("essay")
            );
        QVERIFY(!deleted);
        QVERIFY(deleted.error().contains(
            QStringLiteral("Deleting intensive slot state")
            ));
        QCOMPARE(repository.loadIntensiveSlotStates().size(), 1);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(IntensiveSlotStateRepositoryTests)

#include "intensive_slot_state_repository_tests.moc"
