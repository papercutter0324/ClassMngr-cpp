#pragma once

#include "core/enums/schedule_type.h"
#include "core/result.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/class_teacher_assignment.h"
#include "domain/models/sub_prep_class_summary_record.h"
#include "domain/models/sub_prep_class_details_record.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <vector>

struct ClassNavigationReadRecord final
{
    int classId = -1;
    bool hasClassInfo = false;
    QString grade;
    QString level;
    QString teacherEnglishName;
    QString teacherKoreanName;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
};

struct ClassSubtitleReadRecord final
{
    int classId = -1;
    int teacherId = -1;
    QString grade;
    QString level;
    QList<ClassTime> regularTimes;
};

struct ClassSubtitleBatchReadRecord final
{
    int classId = -1;
    int teacherId = -1;
    QString grade;
    QString level;
    QList<ClassTime> regularTimes;
};

struct MyClassesClassInformationReadRecord final
{
    int classId = -1;
    int teacherId = -1;
    QString classGrade;
    QString classLevel;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
    QString notes;
    QString timeFillerActivities;
};

struct MyClassesClassInformationBatchReadEntry final
{
    int classId = -1;
    Result<MyClassesClassInformationReadRecord> information;
};

struct RosterPrintClassInfoReadRecord final
{
    int classId = -1;
    QString classGrade;
    QString classLevel;
    QString teacherEnglishName;
    QString teacherKoreanName;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString zoomId;
    QString zoomPassword;
    QList<ClassTime> regularTimes;
};

struct SubPrepRosterOutputClassInfoReadRecord final
{
    int classId = -1;
    int teacherId = -1;
    QString classGrade;
    QString classLevel;
    QString teacherEnglishName;
    QString teacherKoreanName;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString zoomId;
    QString zoomPassword;
};

struct SubPrepRosterOutputClassInfoBatchReadMetrics final
{
    int callCount = 0;
    int requestedClassCount = 0;
    int statementCount = 0;
};

struct ClassesNavigationReadMetrics final
{
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
    int intensiveScheduleStatementCount = 0;
};

struct ScheduleClassInfoReadMetrics final
{
    int scheduleClassInfosCallCount = 0;
    int singleClassInfoReadCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
    int intensiveScheduleStatementCount = 0;
};

struct ClassSubtitleBatchReadMetrics final
{
    int callCount = 0;
    int requestedClassCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
};

struct MyClassesClassInformationBatchReadMetrics final
{
    int callCount = 0;
    int requestedClassCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
    int intensiveScheduleStatementCount = 0;
    int fallbackClassReadCount = 0;
};

class ClassInfoRepository
{
public:
    explicit ClassInfoRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Status saveClassInfo(
        const ClassInfo& info
        );

    [[nodiscard]] Status saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );

    [[nodiscard]] Result<ClassInfo> loadClassInfo(
        int classId
        );
    [[nodiscard]] Result<ClassSubtitleReadRecord> loadClassSubtitleRecord(
        int classId
        );
    [[nodiscard]] Result<QList<ClassSubtitleBatchReadRecord>>
        loadClassSubtitleRecords(const QList<int>& classIds);
    [[nodiscard]] Result<std::vector<MyClassesClassInformationBatchReadEntry>>
        loadMyClassesClassInformationRecords(const QList<int>& classIds);
    [[nodiscard]] Result<RosterPrintClassInfoReadRecord>
        loadRosterPrintClassInfoRecord(int classId);
    [[nodiscard]] Result<QList<SubPrepRosterOutputClassInfoReadRecord>>
        loadSubPrepRosterOutputClassInfoRecords(const QList<int>& classIds);

    [[nodiscard]] Result<QList<ClassNavigationReadRecord>>
        loadClassesNavigationRecords(const QList<int>& classIds);
    [[nodiscard]] const ClassesNavigationReadMetrics&
        classesNavigationReadMetrics() const noexcept;
    [[nodiscard]] const ScheduleClassInfoReadMetrics&
        scheduleClassInfoReadMetrics() const noexcept;
    [[nodiscard]] const ClassSubtitleBatchReadMetrics&
        classSubtitleBatchReadMetrics() const noexcept;
    [[nodiscard]] const MyClassesClassInformationBatchReadMetrics&
        myClassesClassInformationBatchReadMetrics() const noexcept;
    [[nodiscard]] const SubPrepRosterOutputClassInfoBatchReadMetrics&
        subPrepRosterOutputClassInfoBatchReadMetrics() const noexcept;

    [[nodiscard]] Result<SubPrepClassDetailsRecord>
        loadSubPrepClassDetails(int classId);

    [[nodiscard]] Result<QList<SubPrepClassSummaryRecord>>
        loadSubPrepClassSummaries(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );

    [[nodiscard]] Result<QList<ClassInfo>> loadClassInfosForScheduleScope(
        const QList<int>& classIds,
        const QStringList& selectedDays,
        ScheduleType type,
        int maxMeetingsPerClass,
        int maxTotalMeetings,
        bool includeUnassignedTeachers = false
        );

    [[nodiscard]] Result<QList<ClassTeacherAssignment>>
        loadClassTeacherAssignments();

    [[nodiscard]] Result<QList<ClassInfo>> loadScheduleClassInfos();

    [[nodiscard]] Result<QList<ClassConflict>> getClassTimeConflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        );

private:
    [[nodiscard]] Result<MyClassesClassInformationReadRecord>
        loadMyClassesClassInformationRecord(int classId);

    QSqlDatabase& m_database;
    ClassesNavigationReadMetrics m_classesNavigationReadMetrics;
    ScheduleClassInfoReadMetrics m_scheduleClassInfoReadMetrics;
    ClassSubtitleBatchReadMetrics m_classSubtitleBatchReadMetrics;
    MyClassesClassInformationBatchReadMetrics
        m_myClassesClassInformationBatchReadMetrics;
    SubPrepRosterOutputClassInfoBatchReadMetrics
        m_subPrepRosterOutputClassInfoBatchReadMetrics;
};
