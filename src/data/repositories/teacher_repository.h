#pragma once

#include "core/result.h"
#include "domain/models/teacher.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class TeacherRepository
{
public:
    explicit TeacherRepository(
        QSqlDatabase& database
        );
    ~TeacherRepository();

    [[nodiscard]] Result<int> createTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] Result<int> saveTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] Status updateTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] Result<Teacher> getTeacher(
        int teacherId
        );

    [[nodiscard]] Result<QList<Teacher>> getAllTeachers();

    [[nodiscard]] Status deleteTeacher(
        int teacherId
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation,
        const QString& teacherContext = {}
        );

    QSqlDatabase& m_database;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
