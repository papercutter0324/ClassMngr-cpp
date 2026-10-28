#pragma once

#include "data/repositories/repository_error.h"
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
    qint64 teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
};

struct TeacherDirectoryReadRecord final
{
    qint64 teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;
};

struct TestingTeacherChoiceReadRecord final
{
    qint64 teacherId = -1;
    QString teacherKr;
    QString room;
};

struct ScheduleImportTeacherReadRecord final
{
    qint64 teacherId = -1;
    QString teacherKr;
    QString roomNumber;
};

struct ScheduleImportTeacherReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
    // Rows count only after the query completes and all row validation passes.
    int returnedRowCount = 0;
};

struct ClassCoTeacherTeacherChoiceReadRecord final
{
    qint64 teacherId = -1;
    QString teacherKr;
    QString teacherEn;
    QString roomNumber;
    QString internetType;
    QString wifiName;
    QString wifiPassword;
    QString projectionType;
    QString zoomId;
    QString zoomPassword;
    bool textExceedsBound = false;
};

struct ClassCoTeacherTeacherProfileReadRecord final
{
    qint64 teacherId = -1;
    QString roomNumber;
    QString internetType;
    QString wifiName;
    QString wifiPassword;
    QString projectionType;
    QString zoomId;
    QString zoomPassword;
    bool textExceedsBound = false;
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
    RepositoryResult<Teacher> profile;
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

struct ClassTransferExportTeacherProfileBatchReadRecord final
{
    qint64 teacherId = -1;
    RepositoryResult<TeacherProfileReadRecord> profile;
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
    RepositoryResult<SubPrepPrintSourceTeacherProfileReadFields> profile;
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
    RepositoryResult<SubPrepRosterOutputTeacherProfileReadFields> profile;
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
    RepositoryResult<MyClassesTeacherProfileReadFields> profile;
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

    [[nodiscard]] RepositoryResult<int> createTeacher(
        const Teacher& teacher
        );
    [[nodiscard]] RepositoryResult<qint64> createTeacherWithWideId(
        const Teacher& teacher
        );

    [[nodiscard]] RepositoryResult<int> saveTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] RepositoryStatus updateTeacher(
        const Teacher& teacher
        );
    [[nodiscard]] RepositoryStatus updateTeacherWithOutcome(
        const Teacher& teacher
        );

    [[nodiscard]] RepositoryStatus updateTeacherWithOutcome(
        const Teacher& fields, qint64 teacherId
        );

    [[nodiscard]] RepositoryResult<Teacher> getTeacher(
        int teacherId
        );
    [[nodiscard]] RepositoryResult<Teacher> lookupTeacherById(
        int teacherId
        );
    [[nodiscard]] RepositoryResult<TeacherDisplayNameReadRecord> lookupTeacherDisplayName(
        int teacherId
        );
    [[nodiscard]] RepositoryResult<TeacherDisplayNameReadRecord> lookupTeacherDisplayName(
        qint64 teacherId
        );
    [[nodiscard]] RepositoryResult<TeacherDisplayNameReadRecord>
        loadTeacherDisplayNameFields(int teacherId);
    [[nodiscard]] RepositoryResult<TeacherDisplayNameReadRecord>
        loadTeacherDisplayNameFields(qint64 teacherId);
    [[nodiscard]] RepositoryResult<QList<TeacherDisplayNameBatchReadRecord>>
        loadTeacherDisplayNameRecords(const QList<int>& teacherIds);
    [[nodiscard]] RepositoryResult<QList<MyClassesTeacherDisplayNameBatchReadRecord>>
        loadMyClassesTeacherDisplayNameRecords(
            const QList<qint64>& teacherIds
            );
    [[nodiscard]] const TeacherDisplayNameBatchReadMetrics&
        teacherDisplayNameBatchReadMetrics() const noexcept;
    [[nodiscard]] RepositoryResult<QList<TeacherProfileBatchReadRecord>>
        loadTeacherProfileRecords(const QList<int>& teacherIds);
    [[nodiscard]] RepositoryResult<QList<ClassTransferExportTeacherProfileBatchReadRecord>>
        loadClassTransferExportTeacherProfileRecords(const QList<qint64>& teacherIds);
    [[nodiscard]] const TeacherProfileBatchReadMetrics&
        teacherProfileBatchReadMetrics() const noexcept;
    [[nodiscard]] RepositoryResult<TeacherProfileReadRecord> lookupTeacherProfileById(
        qint64 teacherId
        );
    // Allows edit reloads to retain the legacy operation diagnostic.
    [[nodiscard]] RepositoryResult<TeacherProfileReadRecord> lookupTeacherProfileById(
        qint64 teacherId, const QString& action
        );
    [[nodiscard]] RepositoryResult<
        QList<SubPrepPrintSourceTeacherProfileBatchReadRecord>>
        loadSubPrepPrintSourceTeacherProfileRecords(
            const QList<qint64>& teacherIds
            );
    [[nodiscard]] const SubPrepPrintSourceTeacherProfileBatchReadMetrics&
        subPrepPrintSourceTeacherProfileBatchReadMetrics() const noexcept;
    [[nodiscard]] RepositoryResult<QList<SubPrepRosterOutputTeacherProfileBatchReadRecord>>
        loadSubPrepRosterOutputTeacherProfileRecords(
            const QList<qint64>& teacherIds
            );
    [[nodiscard]] const SubPrepRosterOutputTeacherProfileBatchReadMetrics&
        subPrepRosterOutputTeacherProfileBatchReadMetrics() const noexcept;
    [[nodiscard]] RepositoryResult<QList<MyClassesTeacherProfileBatchReadRecord>>
        loadMyClassesTeacherProfileRecords(const QList<qint64>& teacherIds);
    [[nodiscard]] const MyClassesTeacherProfileBatchReadMetrics&
        myClassesTeacherProfileBatchReadMetrics() const noexcept;

    [[nodiscard]] RepositoryResult<QList<Teacher>> getAllTeachers();
    [[nodiscard]] RepositoryResult<QList<TeacherDirectoryReadRecord>>
        loadTeacherDirectoryRecords();
    [[nodiscard]] RepositoryResult<QList<ScheduleImportTeacherReadRecord>>
        loadScheduleImportTeacherRecords();
    [[nodiscard]] const ScheduleImportTeacherReadMetrics&
        scheduleImportTeacherReadMetrics() const noexcept;
    [[nodiscard]] RepositoryResult<QList<InitialSetupTeacherChoiceReadRecord>>
        loadInitialSetupTeacherChoiceRecords();
    [[nodiscard]] RepositoryResult<QList<TestingTeacherChoiceReadRecord>>
        loadTestingTeacherChoiceRecords();
    [[nodiscard]] RepositoryResult<QList<ClassCoTeacherTeacherChoiceReadRecord>>
        loadClassCoTeacherTeacherChoiceRecords();
    [[nodiscard]] RepositoryResult<QList<ClassCoTeacherTeacherChoiceReadRecord>>
        loadBoundedClassCoTeacherTeacherChoiceRecords(
            int maximumRows,
            int maximumNameBytes,
            qint64 maximumTotalNameBytes
            );
    [[nodiscard]] RepositoryResult<ClassCoTeacherTeacherProfileReadRecord>
        loadBoundedClassCoTeacherTeacherProfile(
            qint64 teacherId,
            int maximumFieldBytes,
            qint64 maximumTotalBytes
            );
    [[nodiscard]] RepositoryResult<QList<KoreanTeacherBirthdayDirectoryReadRecord>>
        loadKoreanTeacherBirthdayDirectoryRecords();

    [[nodiscard]] RepositoryStatus deleteTeacher(
        int teacherId
        );

    [[nodiscard]] RepositoryStatus deleteTeacher(qint64 teacherId);

private:
    [[nodiscard]] RepositoryResult<SubPrepPrintSourceTeacherProfileReadFields>
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
