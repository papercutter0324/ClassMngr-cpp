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

    [[nodiscard]] Result<QList<ClassNavigationReadRecord>>
        loadClassesNavigationRecords(const QList<int>& classIds);
    [[nodiscard]] const ClassesNavigationReadMetrics&
        classesNavigationReadMetrics() const noexcept;
    [[nodiscard]] const ScheduleClassInfoReadMetrics&
        scheduleClassInfoReadMetrics() const noexcept;

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
    QSqlDatabase& m_database;
    ClassesNavigationReadMetrics m_classesNavigationReadMetrics;
    ScheduleClassInfoReadMetrics m_scheduleClassInfoReadMetrics;
};
