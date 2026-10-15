#pragma once

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

namespace TeacherChoicesCursorErrorFixture
{

inline QString execute(
    QSqlDatabase& database,
    const QString& sql
    )
{
    QSqlQuery query(database);
    if (query.exec(sql))
    {
        return {};
    }
    return query.lastError().text();
}

inline QString install(QSqlDatabase& database)
{
    for (const QString& sql : {
             QStringLiteral("PRAGMA foreign_keys=OFF"),
             QStringLiteral("DROP TABLE teachers"),
             QStringLiteral(R"(
                 CREATE TABLE teacher_choices_cursor_error_backing (
                     id INTEGER PRIMARY KEY,
                     teacher_en TEXT NOT NULL,
                     payload TEXT NOT NULL
                 )
             )"),
             QStringLiteral(R"(
                 CREATE INDEX teacher_choices_cursor_teacher_en_idx
                 ON teacher_choices_cursor_error_backing (teacher_en)
             )")
         })
    {
        const QString error = execute(database, sql);
        if (!error.isEmpty())
        {
            return error;
        }
    }

    QSqlQuery insert(database);
    if (!insert.prepare(QStringLiteral(R"(
            INSERT INTO teacher_choices_cursor_error_backing
                (id, teacher_en, payload)
            VALUES (?, ?, ?)
        )")))
    {
        return insert.lastError().text();
    }

    insert.bindValue(0, 1);
    insert.bindValue(1, QStringLiteral("A Alpha"));
    insert.bindValue(2, QStringLiteral(R"({
        "teacher_kr":"A Alpha",
        "preferred_romanization":"A Alpha",
        "preferred_name":"A Alpha",
        "room_number":"A Alpha",
        "internet_type":"A Alpha",
        "wifi_name":"A Alpha",
        "wifi_password":"A Alpha",
        "projection_type":"A Alpha",
        "zoom_id":"A Alpha",
        "zoom_password":"A Alpha"
    })"));
    if (!insert.exec())
    {
        return insert.lastError().text();
    }

    insert.bindValue(0, 2);
    insert.bindValue(1, QStringLiteral("B Broken"));
    insert.bindValue(2, QStringLiteral("{"));
    if (!insert.exec())
    {
        return insert.lastError().text();
    }

    return execute(database, QStringLiteral(R"(
        CREATE VIEW teachers AS
        SELECT
            id,
            teacher_en,
            json_extract(payload, '$.teacher_kr') AS teacher_kr,
            json_extract(payload, '$.preferred_romanization')
                AS preferred_romanization,
            json_extract(payload, '$.preferred_name') AS preferred_name,
            json_extract(payload, '$.room_number') AS room_number,
            json_extract(payload, '$.internet_type') AS internet_type,
            json_extract(payload, '$.wifi_name') AS wifi_name,
            json_extract(payload, '$.wifi_password') AS wifi_password,
            json_extract(payload, '$.projection_type') AS projection_type,
            json_extract(payload, '$.zoom_id') AS zoom_id,
            json_extract(payload, '$.zoom_password') AS zoom_password
        FROM teacher_choices_cursor_error_backing
    )"));
}

struct QueryPlan final
{
    bool executed = false;
    QStringList details;
    QString error;
};

inline QueryPlan explain(
    QSqlDatabase& database,
    const QString& selectSql
    )
{
    QueryPlan result;
    QSqlQuery query(database);
    result.executed = query.exec(
        QStringLiteral("EXPLAIN QUERY PLAN ") + selectSql
        );
    if (!result.executed)
    {
        result.error = query.lastError().text();
        return result;
    }

    while (query.next())
    {
        result.details.append(query.value(3).toString());
    }
    if (query.lastError().isValid())
    {
        result.error = query.lastError().text();
    }
    return result;
}

struct CursorProbe final
{
    bool executed = false;
    bool firstRowFound = false;
    qint64 firstTeacherId = 0;
    QString firstTeacherKr;
    bool secondRowFound = false;
    QSqlError terminalError;
};

inline CursorProbe probe(
    QSqlDatabase& database,
    const QString& selectSql
    )
{
    CursorProbe result;
    QSqlQuery query(database);
    result.executed = query.exec(selectSql);
    if (!result.executed)
    {
        result.terminalError = query.lastError();
        return result;
    }

    result.firstRowFound = query.next();
    if (!result.firstRowFound)
    {
        result.terminalError = query.lastError();
        return result;
    }

    result.firstTeacherId = query.value(QStringLiteral("id")).toLongLong();
    result.firstTeacherKr = query.value(QStringLiteral("teacher_kr")).toString();
    result.secondRowFound = query.next();
    result.terminalError = query.lastError();
    return result;
}

} // namespace TeacherChoicesCursorErrorFixture
