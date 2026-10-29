#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <QUuid>

class Phase7NextMemoryFixtureTests final : public QObject
{
    Q_OBJECT

private slots:
    void schemaOnlyBlankWorkspaceIsValidAndDeterministic();
};

void Phase7NextMemoryFixtureTests
    ::schemaOnlyBlankWorkspaceIsValidAndDeterministic()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString configuredPath =
        qEnvironmentVariable(
            "CLASSMNGR_PHASE7_SCHEMA_ONLY_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString fixturePath = configuredPath.isEmpty()
        ? temporaryDirectory.filePath(QStringLiteral("schema-only.tps"))
        : QFileInfo(configuredPath).absoluteFilePath();

    if (!configuredPath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(fixturePath).absolutePath()),
            qPrintable(QStringLiteral("Could not create fixture output directory for %1")
                .arg(fixturePath))
            );
        QVERIFY2(
            !QFileInfo::exists(fixturePath),
            qPrintable(QStringLiteral("Refusing to overwrite fixture %1")
                .arg(fixturePath))
            );
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.open(fixturePath);
    QVERIFY2(
        opened.has_value(),
        qPrintable(opened.has_value()
            ? QString()
            : opened.error().diagnostic)
        );
    QVERIFY(store.isOpen());
    store.close();

    const QFileInfo fixtureInfo(fixturePath);
    QVERIFY(fixtureInfo.exists());
    QVERIFY(fixtureInfo.isFile());
    QVERIFY(fixtureInfo.size() > 0);

    const QString connectionName =
        QStringLiteral("phase7-schema-only-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        const RepositoryResult<int> schemaVersion =
            DatabaseSchemaManager::schemaVersion(database);
        QVERIFY2(
            schemaVersion.has_value(),
            qPrintable(schemaVersion.has_value()
                ? QString()
                : schemaVersion.error().diagnostic)
            );
        QCOMPARE(*schemaVersion, DatabaseSchemaManager::LatestSchemaVersion);

        QSqlQuery integrityQuery(database);
        QVERIFY2(
            integrityQuery.exec(QStringLiteral("PRAGMA integrity_check")),
            qPrintable(integrityQuery.lastError().text())
            );
        QVERIFY(integrityQuery.next());
        QCOMPARE(integrityQuery.value(0).toString(), QStringLiteral("ok"));

        for (const QString& tableName : {
                 QStringLiteral("classes"),
                 QStringLiteral("teachers")
             })
        {
            QSqlQuery countQuery(database);
            QVERIFY2(
                countQuery.exec(
                    QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName)
                    ),
                qPrintable(countQuery.lastError().text())
                );
            QVERIFY(countQuery.next());
            QCOMPARE(countQuery.value(0).toInt(), 0);
        }

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_GUILESS_MAIN(Phase7NextMemoryFixtureTests)

#include "phase7_next_memory_fixture_tests.moc"
