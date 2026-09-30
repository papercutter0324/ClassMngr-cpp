#pragma once

#include "core/result.h"
#include "domain/models/teacher.h"

#include <QList>
#include <QSqlDatabase>

struct TeacherDisplayNameReadRecord final
{
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
};

class TeacherRepository
{
public:
    explicit TeacherRepository(
        QSqlDatabase& database
        );

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
    [[nodiscard]] Result<TeacherDisplayNameReadRecord>
        loadTeacherDisplayNameFields(int teacherId);

    [[nodiscard]] Result<QList<Teacher>> getAllTeachers();

    [[nodiscard]] Status deleteTeacher(
        int teacherId
        );

private:
    QSqlDatabase& m_database;
};
