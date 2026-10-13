#pragma once

#include "core/result.h"
#include "domain/models/teacher.h"

#include <QList>
#include <QSqlDatabase>
#include <QtGlobal>

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

struct MyClassesTeacherDisplayNameBatchReadRecord final
{
    qint64 teacherId = -1;
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

struct TeacherProfileReadRecord final
{
    qint64 teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
    QString roomNumber;
    QString birthday;
    QString phoneNumber;
    QString wifiName;
    QString wifiPassword;
    QString internetType;
    QString zoomId;
    QString zoomPassword;
    QString projectionType;
    QString notes;
};

enum class TeacherProfileLookupCode
{
    Found,
    Missing,
    Failure
};

struct TeacherProfileLookupResult final
{
    TeacherProfileLookupCode code = TeacherProfileLookupCode::Failure;
    TeacherProfileReadRecord profile;
    QString diagnostic;
};

struct SubPrepPrintSourceTeacherProfileReadFields final
{
    qint64 teacherId = -1;
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

struct SubPrepPrintSourceTeacherProfileBatchReadRecord final
{
    qint64 requestedTeacherId = -1;
    Result<SubPrepPrintSourceTeacherProfileReadFields> profile;
};

struct SubPrepPrintSourceTeacherProfileBatchReadMetrics final
{
    int callCount = 0;
    int requestedTeacherCount = 0;
    int statementCount = 0;
    int fallbackSingleReadCount = 0;
};

struct SubPrepRosterOutputTeacherProfileReadFields final
{
    qint64 teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
};

struct SubPrepRosterOutputTeacherProfileBatchReadRecord final
{
    qint64 requestedTeacherId = -1;
    Result<SubPrepRosterOutputTeacherProfileReadFields> profile;
};

struct SubPrepRosterOutputTeacherProfileBatchReadMetrics final
{
    int callCount = 0;
    int requestedTeacherCount = 0;
    int statementCount = 0;
};

struct MyClassesTeacherProfileReadFields final
{
    qint64 teacherId = -1;
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
    qint64 requestedTeacherId = -1;
    Result<MyClassesTeacherProfileReadFields> profile;
};

struct MyClassesTeacherProfileBatchReadMetrics final
{
    int callCount = 0;
    int requestedTeacherCount = 0;
    int statementCount = 0;
};

enum class TeacherByIdLookupCode
{
    Found,
    Missing,
    Failure
};

struct TeacherByIdLookupResult final
{
    TeacherByIdLookupCode code = TeacherByIdLookupCode::Failure;
    Teacher teacher;
    QString diagnostic;
};

enum class TeacherUpdateOutcomeCode
{
    Updated,
    Missing,
    Failure
};

struct TeacherUpdateOutcome final
{
    TeacherUpdateOutcomeCode code = TeacherUpdateOutcomeCode::Failure;
    QString diagnostic;
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
    [[nodiscard]] TeacherUpdateOutcome updateTeacherWithOutcome(
        const Teacher& teacher
        );

    [[nodiscard]] Result<Teacher> getTeacher(
        int teacherId
        );
    [[nodiscard]] TeacherByIdLookupResult lookupTeacherById(
        int teacherId
        );
    [[nodiscard]] Result<TeacherDisplayNameReadRecord>
        loadTeacherDisplayNameFields(int teacherId);
    [[nodiscard]] Result<TeacherDisplayNameReadRecord>
        loadTeacherDisplayNameFields(qint64 teacherId);
    [[nodiscard]] Result<QList<TeacherDisplayNameBatchReadRecord>>
        loadTeacherDisplayNameRecords(const QList<int>& teacherIds);
    [[nodiscard]] Result<QList<MyClassesTeacherDisplayNameBatchReadRecord>>
        loadMyClassesTeacherDisplayNameRecords(
            const QList<qint64>& teacherIds
            );
    [[nodiscard]] const TeacherDisplayNameBatchReadMetrics&
        teacherDisplayNameBatchReadMetrics() const noexcept;
    [[nodiscard]] Result<QList<TeacherProfileBatchReadRecord>>
        loadTeacherProfileRecords(const QList<int>& teacherIds);
    [[nodiscard]] const TeacherProfileBatchReadMetrics&
        teacherProfileBatchReadMetrics() const noexcept;
    [[nodiscard]] TeacherProfileLookupResult lookupTeacherProfileById(
        qint64 teacherId
        );
    [[nodiscard]] Result<
        QList<SubPrepPrintSourceTeacherProfileBatchReadRecord>>
        loadSubPrepPrintSourceTeacherProfileRecords(
            const QList<qint64>& teacherIds
            );
    [[nodiscard]] const SubPrepPrintSourceTeacherProfileBatchReadMetrics&
        subPrepPrintSourceTeacherProfileBatchReadMetrics() const noexcept;
    [[nodiscard]] Result<QList<SubPrepRosterOutputTeacherProfileBatchReadRecord>>
        loadSubPrepRosterOutputTeacherProfileRecords(
            const QList<qint64>& teacherIds
            );
    [[nodiscard]] const SubPrepRosterOutputTeacherProfileBatchReadMetrics&
        subPrepRosterOutputTeacherProfileBatchReadMetrics() const noexcept;
    [[nodiscard]] Result<QList<MyClassesTeacherProfileBatchReadRecord>>
        loadMyClassesTeacherProfileRecords(const QList<qint64>& teacherIds);
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
    [[nodiscard]] Result<SubPrepPrintSourceTeacherProfileReadFields>
        loadSubPrepPrintSourceTeacherProfileRecord(qint64 teacherId);

    QSqlDatabase& m_database;
    ScheduleImportTeacherReadMetrics m_scheduleImportTeacherReadMetrics;
    TeacherDisplayNameBatchReadMetrics m_teacherDisplayNameBatchReadMetrics;
    TeacherProfileBatchReadMetrics m_teacherProfileBatchReadMetrics;
    SubPrepPrintSourceTeacherProfileBatchReadMetrics
        m_subPrepPrintSourceTeacherProfileBatchReadMetrics;
    SubPrepRosterOutputTeacherProfileBatchReadMetrics
        m_subPrepRosterOutputTeacherProfileBatchReadMetrics;
    MyClassesTeacherProfileBatchReadMetrics
        m_myClassesTeacherProfileBatchReadMetrics;
};
