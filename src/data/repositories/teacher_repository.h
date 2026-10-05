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

struct InitialSetupTeacherChoiceReadRecord final
{
    int teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
};

struct TestingTeacherChoiceReadRecord final
{
    int teacherId = -1;
    QString teacherKr;
    QString room;
};

struct ScheduleImportTeacherReadRecord final
{
    int teacherId = -1;
    QString teacherKr;
    QString roomNumber;
};

struct ScheduleImportTeacherReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
};

struct ClassCoTeacherTeacherChoiceReadRecord final
{
    int teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString roomNumber;
    QString internetType;
    QString wifiName;
    QString wifiPassword;
    QString projectionType;
    QString zoomId;
    QString zoomPassword;
};

struct KoreanTeacherBirthdayDirectoryReadRecord final
{
    QString birthday;
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

struct MyClassesTeacherProfileReadFields final
{
    int teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString internetType;
    QString zoomId;
    QString zoomPassword;
    QString projectionType;
    QString notes;
};

struct MyClassesTeacherProfileBatchReadRecord final
{
    int requestedTeacherId = -1;
    Result<MyClassesTeacherProfileReadFields> profile;
};

struct MyClassesTeacherProfileBatchReadMetrics final
{
    int callCount = 0;
    int requestedTeacherCount = 0;
    int statementCount = 0;
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
    [[nodiscard]] Result<QList<MyClassesTeacherProfileBatchReadRecord>>
        loadMyClassesTeacherProfileRecords(const QList<int>& teacherIds);
    [[nodiscard]] const MyClassesTeacherProfileBatchReadMetrics&
        myClassesTeacherProfileBatchReadMetrics() const noexcept;

    [[nodiscard]] Result<QList<Teacher>> getAllTeachers();
    [[nodiscard]] Result<QList<ScheduleImportTeacherReadRecord>>
        loadScheduleImportTeacherRecords();
    [[nodiscard]] const ScheduleImportTeacherReadMetrics&
        scheduleImportTeacherReadMetrics() const noexcept;
    [[nodiscard]] Result<QList<InitialSetupTeacherChoiceReadRecord>>
        loadInitialSetupTeacherChoiceRecords();
    [[nodiscard]] Result<QList<TestingTeacherChoiceReadRecord>>
        loadTestingTeacherChoiceRecords();
    [[nodiscard]] Result<QList<ClassCoTeacherTeacherChoiceReadRecord>>
        loadClassCoTeacherTeacherChoiceRecords();
    [[nodiscard]] Result<QList<KoreanTeacherBirthdayDirectoryReadRecord>>
        loadKoreanTeacherBirthdayDirectoryRecords();

    [[nodiscard]] Status deleteTeacher(
        int teacherId
        );

private:
    QSqlDatabase& m_database;
    ScheduleImportTeacherReadMetrics m_scheduleImportTeacherReadMetrics;
    TeacherDisplayNameBatchReadMetrics m_teacherDisplayNameBatchReadMetrics;
    TeacherProfileBatchReadMetrics m_teacherProfileBatchReadMetrics;
    MyClassesTeacherProfileBatchReadMetrics
        m_myClassesTeacherProfileBatchReadMetrics;
};
