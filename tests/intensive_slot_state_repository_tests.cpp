#include "data/repositories/intensive_slot_state_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class IntensiveSlotStateRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultStateControlsStoredRows();
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

        repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay")
            );

        QVERIFY(repository.loadIntensiveSlotStates().isEmpty());

        repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("lunch")
            );

        QList<IntensiveSlotState> states =
            repository.loadIntensiveSlotStates();

        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().state, QStringLiteral("lunch"));

        repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay")
            );

        QVERIFY(repository.loadIntensiveSlotStates().isEmpty());

        repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("essay"),
            QStringLiteral("empty")
            );

        states =
            repository.loadIntensiveSlotStates();

        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().state, QStringLiteral("essay"));

        repository.saveIntensiveSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            QStringLiteral("empty"),
            QStringLiteral("empty")
            );

        QVERIFY(repository.loadIntensiveSlotStates().isEmpty());
    }

    QSqlDatabase::removeDatabase(
        connectionName
        );
}

QTEST_MAIN(IntensiveSlotStateRepositoryTests)

#include "intensive_slot_state_repository_tests.moc"
