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

struct TeacherDisplayNameBatchReadRecord final
{
    int teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
};

struct TeacherDisplayNameBatchReadMetrics final
{
    int callCount = 0;
    int requestedTeacherCount = 0;
    int statementCount = 0;
};

struct TeacherProfileBatchReadRecord final
{
    int teacherId = -1;
    Result<Teacher> profile;
};

struct TeacherProfileBatchReadMetrics final
{
    int callCount = 0;
    int requestedTeacherCount = 0;
    int statementCount = 0;
    int fallbackSingleReadCount = 0;
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
    [[nodiscard]] Result<QList<TeacherDisplayNameBatchReadRecord>>
        loadTeacherDisplayNameRecords(const QList<int>& teacherIds);
    [[nodiscard]] const TeacherDisplayNameBatchReadMetrics&
        teacherDisplayNameBatchReadMetrics() const noexcept;
    [[nodiscard]] Result<QList<TeacherProfileBatchReadRecord>>
        loadTeacherProfileRecords(const QList<int>& teacherIds);
    [[nodiscard]] const TeacherProfileBatchReadMetrics&
        teacherProfileBatchReadMetrics() const noexcept;

    [[nodiscard]] Result<QList<Teacher>> getAllTeachers();

    [[nodiscard]] Status deleteTeacher(
        int teacherId
        );

private:
    QSqlDatabase& m_database;
    TeacherDisplayNameBatchReadMetrics m_teacherDisplayNameBatchReadMetrics;
    TeacherProfileBatchReadMetrics m_teacherProfileBatchReadMetrics;
};
