#include "data/database/database_schema_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

namespace
{
struct ColumnContract
{
    const char* name;
    const char* type;
    int notNull;
    const char* defaultSql;
    int primaryKeyOrdinal;
};

struct TableContract
{
    const char* name;
    QList<ColumnContract> columns;
};

struct ForeignKeyContract
{
    const char* table;
    const char* column;
    const char* referencedTable;
    const char* referencedColumn;
    const char* onUpdate;
    const char* onDelete;
};

struct IndexContract
{
    const char* name;
    const char* table;
    QStringList columns;
};

int pragmaValue(QSqlDatabase& database, const QString& pragma);

ColumnContract column(
    const char* name,
    const char* type,
    int notNull = 0,
    const char* defaultSql = nullptr,
    int primaryKeyOrdinal = 0
    )
{
    return {name, type, notNull, defaultSql, primaryKeyOrdinal};
}

QString quotedIdentifier(const QString& identifier)
{
    QString escaped = identifier;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QStringList indexColumns(QSqlDatabase& database, const QString& indexName)
{
    QStringList columns;
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA index_info(%1)").arg(
            quotedIdentifier(indexName)
            )))
    {
        return columns;
    }

    while (query.next())
    {
        columns.append(query.value(QStringLiteral("name")).toString());
    }
    return columns;
}

bool containsSqlToken(const QString& sql, const QString& token)
{
    const QRegularExpression tokenExpression(
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(token)),
        QRegularExpression::CaseInsensitiveOption
        );
    return tokenExpression.match(sql).hasMatch();
}

QList<TableContract> schemaV6TableContracts()
{
    return {
        {"app_settings", {
            column("key", "TEXT", 0, nullptr, 1),
            column("value", "TEXT")
        }},
        {"calendar_events", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("title", "TEXT", 1),
            column("event_type", "TEXT", 1, "'Other'"),
            column("time_status", "TEXT", 1, "'Timed'"),
            column("repeat_series_id", "TEXT"),
            column("all_day", "INTEGER", 1, "0"),
            column("start_date", "TEXT"),
            column("start_time", "TEXT"),
            column("end_date", "TEXT"),
            column("end_time", "TEXT")
        }},
        {"campuses", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("name", "TEXT", 1),
            column("building_name", "TEXT"),
            column("address", "TEXT"),
            column("phone_number", "TEXT"),
            column("office_number", "TEXT"),
            column("transit_steps", "TEXT"),
            column("arrival_info", "TEXT"),
            column("image_path", "TEXT"),
            column("office_wifi", "TEXT"),
            column("office_wifi_password", "TEXT"),
            column("printer_name", "TEXT"),
            column("printer_steps", "TEXT"),
            column("photocopier_code", "TEXT"),
            column("housing_locations", "TEXT")
        }},
        {"class_info", {
            column("class_id", "INTEGER", 0, nullptr, 1),
            column("teacher_id", "INTEGER"),
            column("class_grade", "TEXT"),
            column("class_level", "TEXT"),
            column("reading_book", "TEXT"),
            column("essay_book", "TEXT"),
            column("class_color", "TEXT", 0, "'#FFFFFF'"),
            column("font_color", "TEXT", 0, "'#000000'"),
            column("notes", "TEXT"),
            column("time_filler_activities", "TEXT")
        }},
        {"class_intensive_times", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("class_id", "INTEGER", 1),
            column("day", "TEXT"),
            column("start_time", "TEXT"),
            column("end_time", "TEXT")
        }},
        {"class_times", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("class_id", "INTEGER", 1),
            column("day", "TEXT"),
            column("start_time", "TEXT"),
            column("end_time", "TEXT")
        }},
        {"classes", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("name", "TEXT")
        }},
        {"gs_team", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("name", "TEXT"),
            column("korean_name", "TEXT"),
            column("position", "TEXT"),
            column("phone_number", "TEXT"),
            column("birthday", "TEXT")
        }},
        {"intensive_slot_states", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("day", "TEXT"),
            column("start_time", "TEXT"),
            column("state", "TEXT")
        }},
        {"native_english_teachers", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("name", "TEXT"),
            column("position", "TEXT"),
            column("phone_number", "TEXT"),
            column("birthday", "TEXT"),
            column("nationality", "TEXT"),
            column("email", "TEXT")
        }},
        {"roster_columns", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("class_id", "INTEGER", 1),
            column("name", "TEXT"),
            column("position", "INTEGER", 1),
            column("width", "INTEGER")
        }},
        {"roster_data", {
            column("class_id", "INTEGER", 1, nullptr, 1),
            column("row_index", "INTEGER", 1, nullptr, 2),
            column("col_index", "INTEGER", 1, nullptr, 3),
            column("value", "TEXT")
        }},
        {"schedule_testing_blocks", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("day", "TEXT", 1),
            column("start_time", "TEXT", 1),
            column("room", "TEXT", 1, "''"),
            column("class_id", "INTEGER")
        }},
        {"speaking_eval_data", {
            column("evaluation_id", "INTEGER", 1, nullptr, 1),
            column("row_index", "INTEGER", 1, nullptr, 2),
            column("col_0", "TEXT"),
            column("col_1", "TEXT"),
            column("col_2", "TEXT"),
            column("col_3", "TEXT"),
            column("col_4", "TEXT"),
            column("col_5", "TEXT"),
            column("col_6", "TEXT"),
            column("col_7", "TEXT"),
            column("col_8", "TEXT"),
            column("col_9", "TEXT"),
            column("col_10", "TEXT")
        }},
        {"speaking_evaluations", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("class_id", "INTEGER", 1),
            column("evaluation_name", "TEXT")
        }},
        {"teachers", {
            column("id", "INTEGER", 0, nullptr, 1),
            column("teacher_kr", "TEXT"),
            column("teacher_en", "TEXT"),
            column("preferred_romanization", "TEXT"),
            column("preferred_name", "TEXT"),
            column("room_number", "TEXT"),
            column("birthday", "TEXT"),
            column("phone_number", "TEXT"),
            column("wifi_name", "TEXT"),
            column("wifi_password", "TEXT"),
            column("internet_type", "TEXT", 1, "'WiFi'"),
            column("zoom_id", "TEXT"),
            column("zoom_password", "TEXT"),
            column("projection_type", "TEXT", 1, "'HDMI'"),
            column("notes", "TEXT")
        }},
        {"testing_classes", {
            column("class_id", "INTEGER", 0, nullptr, 1),
            column("room", "TEXT", 1)
        }}
    };
}

void verifyRuntimeSchemaV6Contract(QSqlDatabase& database)
{
    const QList<TableContract> tables = schemaV6TableContracts();
    QStringList expectedTableNames;
    for (const TableContract& table : tables)
    {
        expectedTableNames.append(QString::fromLatin1(table.name));
    }
    expectedTableNames.sort();

    QStringList actualTableNames;
    QSqlQuery tableQuery(database);
    QVERIFY(tableQuery.exec(QStringLiteral(
        "SELECT name FROM sqlite_master "
        "WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
        )));
    while (tableQuery.next())
    {
        actualTableNames.append(tableQuery.value(0).toString());
    }
    QCOMPARE(actualTableNames, expectedTableNames);
    QCOMPARE(actualTableNames.size(), 17);

    for (const TableContract& table : tables)
    {
        QSqlQuery columnsQuery(database);
        QVERIFY(columnsQuery.exec(QStringLiteral("PRAGMA table_info(%1)").arg(
            quotedIdentifier(QString::fromLatin1(table.name))
            )));

        int columnOrdinal = 0;
        while (columnsQuery.next())
        {
            QVERIFY(columnOrdinal < table.columns.size());
            const ColumnContract& expected = table.columns.at(columnOrdinal);
            QCOMPARE(columnsQuery.value(QStringLiteral("cid")).toInt(), columnOrdinal);
            QCOMPARE(columnsQuery.value(QStringLiteral("name")).toString(),
                QString::fromLatin1(expected.name));
            QCOMPARE(columnsQuery.value(QStringLiteral("type")).toString(),
                QString::fromLatin1(expected.type));
            QCOMPARE(columnsQuery.value(QStringLiteral("notnull")).toInt(), expected.notNull);
            QCOMPARE(columnsQuery.value(QStringLiteral("pk")).toInt(), expected.primaryKeyOrdinal);
            if (expected.defaultSql == nullptr)
            {
                QVERIFY(columnsQuery.value(QStringLiteral("dflt_value")).isNull());
            }
            else
            {
                QVERIFY(!columnsQuery.value(QStringLiteral("dflt_value")).isNull());
                QCOMPARE(columnsQuery.value(QStringLiteral("dflt_value")).toString(),
                    QString::fromLatin1(expected.defaultSql));
            }
            ++columnOrdinal;
        }
        QCOMPARE(columnOrdinal, table.columns.size());
    }

    const QList<ForeignKeyContract> expectedForeignKeys = {
        {"class_info", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"class_info", "teacher_id", "teachers", "id", "NO ACTION", "SET NULL"},
        {"testing_classes", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"class_times", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"class_intensive_times", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"roster_columns", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"roster_data", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"speaking_evaluations", "class_id", "classes", "id", "NO ACTION", "CASCADE"},
        {"speaking_eval_data", "evaluation_id", "speaking_evaluations", "id", "NO ACTION", "CASCADE"},
        {"schedule_testing_blocks", "class_id", "classes", "id", "NO ACTION", "SET NULL"}
    };
    QStringList expectedForeignKeyEdges;
    for (const ForeignKeyContract& foreignKey : expectedForeignKeys)
    {
        expectedForeignKeyEdges.append(QStringLiteral("%1.%2->%3.%4|%5|%6")
            .arg(QString::fromLatin1(foreignKey.table),
                QString::fromLatin1(foreignKey.column),
                QString::fromLatin1(foreignKey.referencedTable),
                QString::fromLatin1(foreignKey.referencedColumn),
                QString::fromLatin1(foreignKey.onUpdate),
                QString::fromLatin1(foreignKey.onDelete)));
    }
    expectedForeignKeyEdges.sort();

    QStringList actualForeignKeyEdges;
    for (const TableContract& table : tables)
    {
        QSqlQuery foreignKeysQuery(database);
        QVERIFY(foreignKeysQuery.exec(QStringLiteral("PRAGMA foreign_key_list(%1)").arg(
            quotedIdentifier(QString::fromLatin1(table.name))
            )));
        while (foreignKeysQuery.next())
        {
            actualForeignKeyEdges.append(QStringLiteral("%1.%2->%3.%4|%5|%6")
                .arg(QString::fromLatin1(table.name),
                    foreignKeysQuery.value(QStringLiteral("from")).toString(),
                    foreignKeysQuery.value(QStringLiteral("table")).toString(),
                    foreignKeysQuery.value(QStringLiteral("to")).toString(),
                    foreignKeysQuery.value(QStringLiteral("on_update")).toString(),
                    foreignKeysQuery.value(QStringLiteral("on_delete")).toString()));
        }
    }
    actualForeignKeyEdges.sort();
    QCOMPARE(actualForeignKeyEdges, expectedForeignKeyEdges);
    QCOMPARE(actualForeignKeyEdges.size(), 10);

    const QList<IndexContract> explicitIndexes = {
        {"idx_schedule_testing_blocks_class_id", "schedule_testing_blocks",
            {QStringLiteral("class_id")}},
        {"idx_calendar_events_dates", "calendar_events",
            {QStringLiteral("start_date"), QStringLiteral("end_date"),
                QStringLiteral("start_time"), QStringLiteral("title")}},
        {"idx_calendar_events_end_dates", "calendar_events",
            {QStringLiteral("end_date"), QStringLiteral("start_date"),
                QStringLiteral("start_time"), QStringLiteral("title")}},
        {"idx_calendar_events_repeat_series", "calendar_events",
            {QStringLiteral("repeat_series_id"), QStringLiteral("start_date"),
                QStringLiteral("id")}}
    };
    QStringList expectedExplicitIndexNames;
    for (const IndexContract& index : explicitIndexes)
    {
        expectedExplicitIndexNames.append(QString::fromLatin1(index.name));
    }
    expectedExplicitIndexNames.sort();

    QStringList actualExplicitIndexNames;
    QSqlQuery explicitIndexQuery(database);
    QVERIFY(explicitIndexQuery.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='index' AND sql IS NOT NULL"
        )));
    while (explicitIndexQuery.next())
    {
        actualExplicitIndexNames.append(explicitIndexQuery.value(0).toString());
    }
    actualExplicitIndexNames.sort();
    QCOMPARE(actualExplicitIndexNames, expectedExplicitIndexNames);
    QCOMPARE(actualExplicitIndexNames.size(), 4);

    for (const IndexContract& expected : explicitIndexes)
    {
        QSqlQuery indexOwnerQuery(database);
        QVERIFY(indexOwnerQuery.prepare(QStringLiteral(
            "SELECT tbl_name FROM sqlite_master WHERE type='index' AND name=?"
            )));
        indexOwnerQuery.addBindValue(QString::fromLatin1(expected.name));
        QVERIFY(indexOwnerQuery.exec());
        QVERIFY(indexOwnerQuery.next());
        QCOMPARE(indexOwnerQuery.value(0).toString(), QString::fromLatin1(expected.table));
        QCOMPARE(indexColumns(database, QString::fromLatin1(expected.name)), expected.columns);

        QSqlQuery indexListQuery(database);
        QVERIFY(indexListQuery.exec(QStringLiteral("PRAGMA index_list(%1)").arg(
            quotedIdentifier(QString::fromLatin1(expected.table))
            )));
        bool foundInTableIndexList = false;
        while (indexListQuery.next())
        {
            if (indexListQuery.value(QStringLiteral("name")).toString()
                == QString::fromLatin1(expected.name))
            {
                foundInTableIndexList = true;
                QCOMPARE(indexListQuery.value(QStringLiteral("unique")).toInt(), 0);
                QCOMPARE(indexListQuery.value(QStringLiteral("origin")).toString(),
                    QStringLiteral("c"));
            }
        }
        QVERIFY(foundInTableIndexList);
    }

    QStringList expectedUniqueFamilies = {
        QStringLiteral("speaking_evaluations(class_id,evaluation_name)"),
        QStringLiteral("class_intensive_times(day,start_time)"),
        QStringLiteral("intensive_slot_states(day,start_time)"),
        QStringLiteral("schedule_testing_blocks(day,start_time)")
    };
    expectedUniqueFamilies.sort();
    QStringList actualUniqueFamilies;
    for (const TableContract& table : tables)
    {
        QSqlQuery indexListQuery(database);
        QVERIFY(indexListQuery.exec(QStringLiteral("PRAGMA index_list(%1)").arg(
            quotedIdentifier(QString::fromLatin1(table.name))
            )));
        while (indexListQuery.next())
        {
            if (indexListQuery.value(QStringLiteral("unique")).toInt() != 1
                || indexListQuery.value(QStringLiteral("origin")).toString()
                    != QStringLiteral("u"))
            {
                continue;
            }

            const QString indexName = indexListQuery.value(QStringLiteral("name")).toString();
            actualUniqueFamilies.append(QStringLiteral("%1(%2)")
                .arg(QString::fromLatin1(table.name),
                    indexColumns(database, indexName).join(QLatin1Char(','))));
        }
    }
    actualUniqueFamilies.sort();
    QCOMPARE(actualUniqueFamilies, expectedUniqueFamilies);

    QStringList expectedAutoIncrementTables = {
        QStringLiteral("roster_columns"),
        QStringLiteral("speaking_evaluations"),
        QStringLiteral("campuses"),
        QStringLiteral("teachers"),
        QStringLiteral("native_english_teachers"),
        QStringLiteral("gs_team"),
        QStringLiteral("classes"),
        QStringLiteral("class_times"),
        QStringLiteral("class_intensive_times"),
        QStringLiteral("intensive_slot_states"),
        QStringLiteral("schedule_testing_blocks"),
        QStringLiteral("calendar_events")
    };
    expectedAutoIncrementTables.sort();
    QStringList actualAutoIncrementTables;
    for (const TableContract& table : tables)
    {
        QSqlQuery createSqlQuery(database);
        QVERIFY(createSqlQuery.prepare(QStringLiteral(
            "SELECT sql FROM sqlite_master WHERE type='table' AND name=?"
            )));
        createSqlQuery.addBindValue(QString::fromLatin1(table.name));
        QVERIFY(createSqlQuery.exec());
        QVERIFY(createSqlQuery.next());
        if (containsSqlToken(createSqlQuery.value(0).toString(), QStringLiteral("AUTOINCREMENT")))
        {
            actualAutoIncrementTables.append(QString::fromLatin1(table.name));
        }
    }
    actualAutoIncrementTables.sort();
    QCOMPARE(actualAutoIncrementTables, expectedAutoIncrementTables);
    QCOMPARE(actualAutoIncrementTables.size(), 12);

    QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), 6);
    QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA foreign_keys")), 1);
    QSqlQuery integrityQuery(database);
    QVERIFY(integrityQuery.exec(QStringLiteral("PRAGMA foreign_key_check")));
    QVERIFY(!integrityQuery.next());

    // Exercise every CHECK family with accepted values and an out-of-domain value.
    const QStringList internetTypes = {
        QStringLiteral("WiFi"), QStringLiteral("LAN"),
        QStringLiteral("Both"), QStringLiteral("N/A")
    };
    for (const QString& type : internetTypes)
    {
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (internet_type) VALUES ('%1')"
            ).arg(type)));
    }
    {
        QSqlQuery query(database);
        QVERIFY(!query.exec(QStringLiteral(
            "INSERT INTO teachers (internet_type) VALUES ('Invalid')"
            )));
    }

    const QStringList projectionTypes = {
        QStringLiteral("HDMI"), QStringLiteral("Zoom"),
        QStringLiteral("Any"), QStringLiteral("N/A")
    };
    for (const QString& type : projectionTypes)
    {
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (projection_type) VALUES ('%1')"
            ).arg(type)));
    }
    {
        QSqlQuery query(database);
        QVERIFY(!query.exec(QStringLiteral(
            "INSERT INTO teachers (projection_type) VALUES ('Invalid')"
            )));
    }

    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO classes (name) VALUES ('Schema contract checks')"
        )));
    const int checkClassId = query.lastInsertId().toInt();
    QVERIFY(checkClassId > 0);
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO roster_columns (class_id, position) VALUES (%1, -1)"
        ).arg(checkClassId)));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO roster_columns (class_id, position) VALUES (%1, 0)"
        ).arg(checkClassId)));

    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index) VALUES (%1, -1, 0)"
        ).arg(checkClassId)));
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index) VALUES (%1, 0, -1)"
        ).arg(checkClassId)));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index) VALUES (%1, 0, 0)"
        ).arg(checkClassId)));

    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (%1, 'Schema contract checks')"
        ).arg(checkClassId)));
    const int checkEvaluationId = query.lastInsertId().toInt();
    QVERIFY(checkEvaluationId > 0);
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO speaking_eval_data (evaluation_id, row_index) VALUES (%1, -1)"
        ).arg(checkEvaluationId)));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO speaking_eval_data (evaluation_id, row_index) VALUES (%1, 0)"
        ).arg(checkEvaluationId)));

    const QStringList eventTypes = {
        QStringLiteral("Vacation"), QStringLiteral("Holiday"),
        QStringLiteral("Workshop"), QStringLiteral("CM"),
        QStringLiteral("Meeting"), QStringLiteral("Other")
    };
    for (const QString& type : eventTypes)
    {
        QSqlQuery eventQuery(database);
        QVERIFY(eventQuery.exec(QStringLiteral(
            "INSERT INTO calendar_events (title, event_type) VALUES ('Check', '%1')"
            ).arg(type)));
    }
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO calendar_events (title, event_type) VALUES ('Check', 'Invalid')"
        )));

    const QStringList eventTimeStatuses = {
        QStringLiteral("Timed"), QStringLiteral("Unknown"), QStringLiteral("Unconfirmed")
    };
    for (const QString& status : eventTimeStatuses)
    {
        QSqlQuery eventQuery(database);
        QVERIFY(eventQuery.exec(QStringLiteral(
            "INSERT INTO calendar_events (title, time_status) VALUES ('Check', '%1')"
            ).arg(status)));
    }
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO calendar_events (title, time_status) VALUES ('Check', 'Invalid')"
        )));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO calendar_events (title, all_day) VALUES ('Check', 0)"
        )));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO calendar_events (title, all_day) VALUES ('Check', 1)"
        )));
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO calendar_events (title, all_day) VALUES ('Check', 2)"
        )));

    // PRAGMA index_list reports table UNIQUE constraints as origin "u".
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (%1, 'Schema contract checks')"
        ).arg(checkClassId)));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO class_intensive_times (class_id, day, start_time) "
        "VALUES (%1, 'Monday', '09:00')"
        ).arg(checkClassId)));
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO class_intensive_times (class_id, day, start_time) "
        "VALUES (%1, 'Monday', '09:00')"
        ).arg(checkClassId)));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO intensive_slot_states (day, start_time, state) "
        "VALUES ('Monday', '09:00', 'available')"
        )));
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO intensive_slot_states (day, start_time, state) "
        "VALUES ('Monday', '09:00', 'blocked')"
        )));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO schedule_testing_blocks (day, start_time) "
        "VALUES ('Monday', '09:00')"
        )));
    QVERIFY(!query.exec(QStringLiteral(
        "INSERT INTO schedule_testing_blocks (day, start_time) "
        "VALUES ('Monday', '09:00')"
        )));
    QVERIFY(query.exec(QStringLiteral("DELETE FROM classes WHERE id=%1").arg(checkClassId)));
}

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
        const RepositoryResult<int> version = DatabaseSchemaManager::schemaVersion(database);
        QVERIFY(version);
        QCOMPARE(*version, DatabaseSchemaManager::LatestSchemaVersion);
        verifyRuntimeSchemaV6Contract(database);
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

        const RepositoryStatus convenience = DatabaseSchemaManager::ensureSchema(database);
        const RepositoryStatus defaultPath = DatabaseSchemaManager::ensureSchema(database, path);

        QVERIFY(!convenience);
        QVERIFY(!defaultPath);
        QVERIFY(convenience.error().diagnostic.contains(QStringLiteral("unsupported schema version 0")));
        QCOMPARE(defaultPath.error().diagnostic, convenience.error().diagnostic);
        QCOMPARE(convenience.error().code, RepositoryErrorCode::Validation);
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
        const RepositoryStatus status = explicitPath
            ? DatabaseSchemaManager::ensureSchema(database, path)
            : DatabaseSchemaManager::ensureSchema(database);

        QVERIFY(!status);
        QCOMPARE(status.error().code, RepositoryErrorCode::Validation);
        QVERIFY(status.error().diagnostic.contains(QStringLiteral("unsupported schema version %1").arg(version)));
        QVERIFY(status.error().diagnostic.contains(QStringLiteral("schema version 6 or later")));
        QVERIFY(status.error().diagnostic.contains(QStringLiteral("Migration from older Teacher Profiles is not available")));
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

        const RepositoryStatus status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QCOMPARE(status.error().code, RepositoryErrorCode::Validation);
        QVERIFY(status.error().diagnostic.contains(
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

        const RepositoryStatus status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QCOMPARE(status.error().code, RepositoryErrorCode::Validation);
        QVERIFY(status.error().diagnostic.contains(QString::number(unsupportedVersion)));
        QVERIFY(status.error().diagnostic.contains(QString::number(
            DatabaseSchemaManager::LatestSchemaVersion
            )));
        QCOMPARE(pragmaValue(database, QStringLiteral("PRAGMA user_version")), unsupportedVersion);
        QVERIFY(!tableExists(database, QStringLiteral("classes")));
    }
    QSqlDatabase::removeDatabase(name);
}

QTEST_MAIN(DatabaseSchemaManagerTests)

#include "database_schema_manager_tests.moc"
