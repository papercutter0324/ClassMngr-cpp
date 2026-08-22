#include "data/database/database_schema_manager.h"

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
    void upgradesLegacyProfileAndRepairsUnassignedTeacher();
    void rejectsInvalidLegacyDataBeforeConstraintMigration();
    void rejectsInvalidLegacyRowIndexesBeforeConstraintMigration();
    void rollsBackConstraintMigrationAfterPartialTableRebuild();
    void upgradesVersionFiveProfilesWithRowIndexConstraints();
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

        QVERIFY(DatabaseSchemaManager::ensureSchema(database));
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

void DatabaseSchemaManagerTests
    ::upgradesLegacyProfileAndRepairsUnassignedTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("legacy-profile.db"));
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(path);
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE classes (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE class_info ("
            "class_id INTEGER PRIMARY KEY, teacher_id INTEGER, class_grade TEXT, "
            "class_level TEXT, reading_book TEXT, essay_book TEXT, "
            "class_color TEXT DEFAULT '#FFFFFF', font_color TEXT DEFAULT '#000000', "
            "notes TEXT, time_filler_activities TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (name) VALUES ('Legacy Class')"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id) VALUES (1, -1)"
            )));

        QVERIFY(DatabaseSchemaManager::ensureSchema(database));
        QCOMPARE(
            pragmaValue(database, QStringLiteral("PRAGMA user_version")),
            DatabaseSchemaManager::LatestSchemaVersion
            );
        QVERIFY(tableHasForeignKey(
            database,
            QStringLiteral("class_info"),
            QStringLiteral("classes")
            ));

        QVERIFY(query.exec(QStringLiteral(
            "SELECT teacher_id IS NULL FROM class_info WHERE class_id=1"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }
    QSqlDatabase::removeDatabase(name);

    QVERIFY(QFileInfo::exists(
        path + QStringLiteral(".pre-schema-v4-backup")
        ));
}

void DatabaseSchemaManagerTests
    ::rejectsInvalidLegacyDataBeforeConstraintMigration()
{
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE teachers ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, teacher_kr TEXT, teacher_en TEXT, "
            "preferred_romanization TEXT, preferred_name TEXT, room_number TEXT, "
            "birthday TEXT, phone_number TEXT, wifi_name TEXT, wifi_password TEXT, "
            "internet_type TEXT DEFAULT 'WiFi', zoom_id TEXT, zoom_password TEXT, "
            "projection_type TEXT DEFAULT 'HDMI', notes TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (teacher_en, internet_type, projection_type) "
            "VALUES ('Legacy Teacher', 'Satellite', 'HDMI')"
            )));

        const Status status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QVERIFY(status.error().contains(QStringLiteral("unsupported internet type")));
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), 2);
        QVERIFY(!tableHasForeignKey(
            database,
            QStringLiteral("class_info"),
            QStringLiteral("classes")
            ));
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests
    ::rejectsInvalidLegacyRowIndexesBeforeConstraintMigration()
{
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE classes (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE roster_data ("
            "class_id INTEGER, row_index INTEGER, col_index INTEGER, value TEXT, "
            "PRIMARY KEY (class_id, row_index, col_index))"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (name) VALUES ('Legacy Class')"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (1, -1, 0, 'Legacy Student')"
            )));

        const Status status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QVERIFY(status.error().contains(
            QStringLiteral("invalid row or column index")
            ));
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), 2);
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests
    ::rollsBackConstraintMigrationAfterPartialTableRebuild()
{
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE teachers ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, teacher_kr TEXT, teacher_en TEXT, "
            "preferred_romanization TEXT, preferred_name TEXT, room_number TEXT, "
            "birthday TEXT, phone_number TEXT, wifi_name TEXT, wifi_password TEXT, "
            "internet_type TEXT DEFAULT 'WiFi', zoom_id TEXT, zoom_password TEXT, "
            "projection_type TEXT DEFAULT 'HDMI', notes TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE classes (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE classmngr_legacy_classes (id INTEGER PRIMARY KEY)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (teacher_en) VALUES ('Legacy Teacher')"
            )));

        const Status status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QVERIFY(status.error().contains(QStringLiteral("Database migration 4")));

        // Migration 4 starts by rebuilding teachers.  A name collision while
        // rebuilding classes must roll back that earlier rebuild as well.
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), 3);
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA foreign_keys")), 1);
        QVERIFY(!tableHasForeignKey(
            database,
            QStringLiteral("teachers"),
            QStringLiteral("classes")
            ));
        QVERIFY(!tableExists(database, QStringLiteral("classmngr_legacy_teachers")));
        QVERIFY(tableExists(database, QStringLiteral("classmngr_legacy_classes")));
        QVERIFY(query.exec(QStringLiteral(
            "SELECT teacher_en FROM teachers WHERE id=1"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Legacy Teacher"));
    }
    QSqlDatabase::removeDatabase(name);
}

void DatabaseSchemaManagerTests
    ::upgradesVersionFiveProfilesWithRowIndexConstraints()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("version-five.db"));
    const QString name = connectionName();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database));

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO classes (name) VALUES ('Version Five Class')"
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

        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 5")));
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

    QVERIFY(QFileInfo::exists(
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
        QVERIFY(DatabaseSchemaManager::ensureSchema(database));

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
