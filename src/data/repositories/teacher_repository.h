#pragma once

#include "domain/models/teacher.h"

#include <QList>
#include <QSqlDatabase>

class TeacherRepository
{
public:
    explicit TeacherRepository(
        QSqlDatabase& database
        );

    int createTeacher(
        const Teacher& teacher
        );

    int saveTeacher(
        const Teacher& teacher
        );

    void updateTeacher(
        const Teacher& teacher
        );

    Teacher getTeacher(
        int teacherId
        );

    QList<Teacher> getAllTeachers();

    void deleteTeacher(
        int teacherId
        );

private:
    QSqlDatabase& m_database;
};
