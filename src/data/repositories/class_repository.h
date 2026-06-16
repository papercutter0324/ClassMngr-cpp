#pragma once

#include "domain/models/classroom.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

class ClassRepository
{
public:
    explicit ClassRepository(
        QSqlDatabase& database
        );

    int createClass(
        const QString& name
        );

    QList<Classroom> getClasses();

    Classroom getClassById(
        int classId
        );

    void updateClassName(
        int classId,
        const QString& name
        );

    void deleteClass(
        int classId
        );

private:
    QSqlDatabase& m_database;
};
