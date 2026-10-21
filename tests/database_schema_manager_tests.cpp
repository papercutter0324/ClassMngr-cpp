#include "data/database/database_schema_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

namespace
{
QString connectionName()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int pragmaValue(QSqlDatabase& database, const QString& pragma)
{
    QSqlQuery query(database);
    if (!query.exec(pragma) || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

bool tableHasForeignKey(
    QSqlDatabase& database,
    const QString& tableName,
    const QString& referencedTable
    )
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_key_list(%1)").arg(tableName)))
    {
        return false;
    }

    while (query.next())
    {
        if (query.value(QStringLiteral("table")).toString() == referencedTable)
        {
            return true;
        }
    }

    return false;
}

bool tableExists(QSqlDatabase& database, const QString& tableName)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT EXISTS("
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"
        ")"
        ));
    query.addBindValue(tableName);
    if (!query.exec() || !query.next())
    {
        return false;
    }

    return query.value(0).toBool();
}
} // namespace

class DatabaseSchemaManagerTests final : public QObject
{
    Q_OBJECT

private slots:
    void createsLatestVersionedSchemaWithForeignKeys();
    void emptyFileRequiresExplicitCreationIntent();
    void rejectsEveryOlderSchemaWithoutWritesOrBackup_data();
    void rejectsEveryOlderSchemaWithoutWritesOrBackup();
    void currentProfilesRetainRowIndexConstraints();
    void reportsForeignKeyIntegrityFailure();
    void rejectsProfilesFromNewerSchemaVersions();
};

void DatabaseSchemaManagerTests::createsLatestVersionedSchemaWithForeignKeys()
{
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QVERIFY(DatabaseSchemaManager::createSchema(database));
        const Result<int> version = DatabaseSchemaManager::schemaVersion(database);
        QVERIFY(version);
        QCOMPARE(*version, DatabaseSchemaManager::LatestSchemaVersion);
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA foreign_keys")), 1);
        QVERIFY(tableHasForeignKey(
            database,
            QStringLiteral("class_info"),
            QStringLiteral("classes")
            ));
        QVERIFY(tableHasForeignKey(
            database,
            QStringLiteral("class_info"),
            QStringLiteral("teachers")
            ));

        QSqlQuery query(database);
        QVERIFY(!query.exec(QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (999, 'Monday', '09:00', '10:00')"
            )));

        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (name) VALUES ('Migration Test')"
            )));
        const int classId = query.lastInsertId().toInt();
        QVERIFY(classId > 0);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (%1, 0, 0, 'Student')"
            ).arg(classId)));
        QVERIFY(query.exec(QStringLiteral(
            "DELETE FROM classes WHERE id=%1"
            ).arg(classId)));
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM roster_data")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests::emptyFileRequiresExplicitCreationIntent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("empty-existing.db"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QCOMPARE(QFileInfo(path).size(), qint64(0));
    const QString name = connectionName();
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        QVERIFY(database.open());

        const Status convenience = DatabaseSchemaManager::ensureSchema(database);
        const Status defaultPath = DatabaseSchemaManager::ensureSchema(database, path);

        QVERIFY(!convenience);
        QVERIFY(!defaultPath);
        QVERIFY(convenience.error().contains(QStringLiteral("unsupported schema version 0")));
        QCOMPARE(defaultPath.error(), convenience.error());
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), 0);
        QVERIFY(!tableExists(database, QStringLiteral("classes")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray{});
        file.close();
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files),
            QStringList{QStringLiteral("empty-existing.db")});

        QVERIFY(DatabaseSchemaManager::createSchema(database));
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), 6);
        QVERIFY(tableExists(database, QStringLiteral("classes")));
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests::rejectsEveryOlderSchemaWithoutWritesOrBackup_data()
{
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("hasSchemaObjects");
    QTest::addColumn<bool>("explicitPath");
    for (int version = 0; version < 6; ++version)
    {
        const QByteArray row = QByteArray::number(version);
        QTest::newRow((row + "-populated-convenience").constData()) << version << true << false;
        QTest::newRow((row + "-empty-existing-convenience").constData()) << version << false << false;
        QTest::newRow((row + "-populated-default").constData()) << version << true << true;
        QTest::newRow((row + "-empty-existing-default").constData()) << version << false << true;
    }
}

void DatabaseSchemaManagerTests::rejectsEveryOlderSchemaWithoutWritesOrBackup()
{
    QFETCH(int, version);
    QFETCH(bool, hasSchemaObjects);
    QFETCH(bool, explicitPath);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("older-profile.db"));
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QSqlQuery query(database);
        if (hasSchemaObjects)
        {
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE class_info (class_id INTEGER PRIMARY KEY, teacher_id INTEGER)")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO class_info VALUES (1, -1)")));
        }
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)));
        const int schemaBefore = pragmaValue(database, QStringLiteral("PRAGMA schema_version"));
        const int foreignKeysBefore = pragmaValue(database, QStringLiteral("PRAGMA foreign_keys"));
        QFile beforeFile(path);
        QVERIFY(beforeFile.open(QIODevice::ReadOnly));
        const QByteArray bytesBefore = beforeFile.readAll();
        beforeFile.close();
        const QStringList filesBefore = QDir(directory.path()).entryList(QDir::Files);

        // Both public validation forms must reject old databases by default.
        const Status status = explicitPath
            ? DatabaseSchemaManager::ensureSchema(database, path)
            : DatabaseSchemaManager::ensureSchema(database);

        QVERIFY(!status);
        QVERIFY(status.error().contains(QStringLiteral("unsupported schema version %1").arg(version)));
        QVERIFY(status.error().contains(QStringLiteral("schema version 6 or later")));
        QVERIFY(status.error().contains(QStringLiteral("Migration from older Teacher Profiles is not available")));
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), version);
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA schema_version")), schemaBefore);
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA foreign_keys")), foreignKeysBefore);
        if (hasSchemaObjects)
        {
            QVERIFY(query.exec(QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id=1")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), -1);
        }
        QVERIFY(!tableExists(database, QStringLiteral("teachers")));
        database.close();
        QFile afterFile(path);
        QVERIFY(afterFile.open(QIODevice::ReadOnly));
        QCOMPARE(afterFile.readAll(), bytesBefore);
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files), filesBefore);
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests
    ::currentProfilesRetainRowIndexConstraints()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("current-profile.db"));
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::createSchema(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (name) VALUES ('Current Class')"
            )));
        const int classId = query.lastInsertId().toInt();
        QVERIFY(classId > 0);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (%1, 'English Name', 0, 180)"
            ).arg(classId)));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (%1, 0, 0, 'Student')"
            ).arg(classId)));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
            "VALUES (%1, 'Week 1')"
            ).arg(classId)));
        const int evaluationId = query.lastInsertId().toInt();
        QVERIFY(evaluationId > 0);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO speaking_eval_data (evaluation_id, row_index) "
            "VALUES (%1, 0)"
            ).arg(evaluationId)));

        QVERIFY(DatabaseSchemaManager::ensureSchema(database));
        QCOMPARE(
            pragmaValue(database, QStringLiteral("PRAGMA user_version")),
            DatabaseSchemaManager::LatestSchemaVersion
            );

        QVERIFY(!query.exec(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (%1, -1, 0, 'Invalid')"
            ).arg(classId)));
        QVERIFY(!query.exec(QStringLiteral(
            "INSERT INTO speaking_eval_data (evaluation_id, row_index) "
            "VALUES (%1, -1)"
            ).arg(evaluationId)));
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM roster_data")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }
    QSqlDatabase::removeDatabase(name);

    QVERIFY(!QFileInfo::exists(
        path + QStringLiteral(".pre-schema-v6-backup")
        ));
}

void DatabaseSchemaManagerTests::reportsForeignKeyIntegrityFailure()
{
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::createSchema(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (999, 'Monday', '09:00', '10:00')"
            )));

        const Status status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QVERIFY(status.error().contains(
            QStringLiteral("Foreign-key integrity check failed")
            ));
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests::rejectsProfilesFromNewerSchemaVersions()
{
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        const int unsupportedVersion = DatabaseSchemaManager::LatestSchemaVersion + 1;
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = %1").arg(
            unsupportedVersion
            )));

        const Status status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QVERIFY(status.error().contains(QString::number(unsupportedVersion)));
        QVERIFY(status.error().contains(QString::number(
            DatabaseSchemaManager::LatestSchemaVersion
            )));
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), unsupportedVersion);
        QVERIFY(!tableExists(database, QStringLiteral("classes")));
    }
    QSqlDatabase::removeDatabase(name);
}

QTEST_MAIN(DatabaseSchemaManagerTests)

#include "database_schema_manager_tests.moc"
