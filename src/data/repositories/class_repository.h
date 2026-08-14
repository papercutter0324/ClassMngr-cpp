#pragma once

#include "core/result.h"
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

    [[nodiscard]] Result<int> createClass(
        const QString& name
        );

    [[nodiscard]] Result<QList<Classroom>> getClasses();

    [[nodiscard]] Result<Classroom> getClassById(
        int classId
        );

    [[nodiscard]] Status updateClassName(
        int classId,
        const QString& name
        );

    [[nodiscard]] Status deleteClass(
        int classId
        );

private:
    QSqlDatabase& m_database;
};
