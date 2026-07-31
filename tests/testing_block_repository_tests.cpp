#include "data/repositories/testing_block_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class TestingBlockRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void savesUpdatesDeletesAndClearsBlocks();
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
            UNIQUE(day, start_time)
        )
    )");
}
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
        QVERIFY(!repository.loadTestingBlocks());
        QVERIFY(!repository.clearTestingBlocks());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(TestingBlockRepositoryTests)

#include "testing_block_repository_tests.moc"
