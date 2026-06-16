#include "class_repository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

ClassRepository::ClassRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

int ClassRepository::createClass(
    const QString& name
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        INSERT INTO classes (
            name
        )
        VALUES (?)
    )");

    query.addBindValue(name);

    query.exec();

    return query.lastInsertId().toInt();
}

QList<Classroom> ClassRepository::getClasses()
{
    QList<Classroom> classes;

    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT *
        FROM classes
        ORDER BY name
    )"))
    {
        qWarning()
            << "Failed to load classes:"
            << query.lastError().text();

        return classes;
    }

    while (query.next())
    {
        Classroom classroom;

        classroom.id =
            query.value("id").toInt();

        classroom.name =
            query.value("name").toString();

        classes.append(classroom);
    }

    return classes;
}

Classroom ClassRepository::getClassById(
    int classId
    )
{
    Classroom classroom;

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT *
        FROM classes
        WHERE id=?
    )");

    query.addBindValue(classId);

    query.exec();

    if (!query.next())
    {
        return classroom;
    }

    classroom.id =
        query.value("id").toInt();

    classroom.name =
        query.value("name").toString();

    return classroom;
}

void ClassRepository::updateClassName(
    int classId,
    const QString& name
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE classes
        SET name=?
        WHERE id=?
    )");

    query.addBindValue(name);
    query.addBindValue(classId);

    query.exec();
}

void ClassRepository::deleteClass(
    int classId
    )
{
    QSqlQuery query(m_database);

    query.prepare("DELETE FROM classes WHERE id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM roster_columns WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM roster_data WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM class_info WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM class_times WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM class_intensive_times WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("SELECT id FROM speaking_evaluations WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    QList<int> evaluationIds;

    while (query.next())
    {
        evaluationIds.append(
            query.value("id").toInt()
            );
    }

    for (int evaluationId : evaluationIds)
    {
        query.prepare("DELETE FROM speaking_eval_data WHERE evaluation_id=?");
        query.addBindValue(evaluationId);
        query.exec();
    }

    query.prepare("DELETE FROM speaking_evaluations WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();
}
