#pragma once

#include "core/enums/schedule_type.h"
#include "data/repositories/repository_error.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/class_teacher_assignment.h"
#include "domain/models/sub_prep_class_summary_record.h"
#include "domain/models/sub_prep_class_details_record.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>
#include <optional>
#include <vector>

struct ClassTeacherAssignmentReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
};

struct ClassDetailsSaveReadRecord final
{
    ClassInfo info;
    qint64 teacherId = -1;
};

// Full export metadata, kept separate from the integer-based edit model.
struct ClassTransferExportClassInfoReadRecord final
{
    int classId{-1};

    qint64 teacherId{-1};

    QString teacherKr;
    QString teacherEn;
    QString teacherPreferredName;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString internetType;
    QString zoomId;
    QString zoomPassword;
    QString projectionType;

    QString classGrade;
    QString classLevel;

    QString readingBook;
    QString essayBook;

    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};

    QList<ClassTime> classTimes;
    QList<ClassTime> intensiveTimes;

    QString notes;
    QString timeFillerActivities;
};

struct ClassNavigationReadRecord final
{
    int classId = -1;
    bool hasClassInfo = false;
    qint64 teacherId = -1;
    QString grade;
    QString level;
    QString teacherEnglishName;
    QString teacherKoreanName;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
};

struct ClassNavigationParentAwareReadRecord final
{
    bool parentExists = false;
    ClassNavigationReadRecord details;
};

struct ClassSubtitleReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString grade;
    QString level;
    QList<ClassTime> regularTimes;
};

struct ClassSubtitleReadOutcome final
{
    ClassSubtitleReadRecord record;
    bool parentMissing = false;
};

struct ClassPageDetailsRegularTime final
{
    QString day;
    QString startTime;
};

struct ClassNotesPageRegularTime final
{
    QString day;
    QString startTime;
};

struct ClassPageDetailsReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QList<ClassPageDetailsRegularTime> regularTimes;
    QString notes;
    QString timeFillerActivities;
};

struct ClassPageDetailsReadOutcome final
{
    ClassPageDetailsReadRecord record;
    bool parentMissing = false;
};

struct ClassCoTeacherPageDetailsReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QList<ClassPageDetailsRegularTime> regularTimes;
    QString notes;
    QString timeFillerActivities;
};

struct ClassCoTeacherPageDetailsReadOutcome final
{
    ClassCoTeacherPageDetailsReadRecord record;
    bool parentMissing = false;
};

struct ClassNotesPageReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QList<ClassNotesPageRegularTime> regularTimes;
    QString notes;
    QString timeFillerActivities;
};

struct ClassNotesPageReadOutcome final
{
    ClassNotesPageReadRecord record;
    bool parentMissing = false;
};

struct ClassDetailsPageReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QString readingBook;
    QString essayBook;
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
};

struct ClassDetailsPageReadOutcome final
{
    ClassDetailsPageReadRecord record;
    bool parentMissing = false;
};

struct ClassCoTeacherAssignmentValidationSnapshotReadRecord final
{
    int classId = -1;
    QString classGrade;
    QString classLevel;
    QString readingBook;
    QString essayBook;
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
    QString notes;
    QString timeFillerActivities;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
};

struct ScheduleEditorClassInfoReadRecord final
{
    int classId = -1;
    QString classGrade;
    QString classLevel;
    QString readingBook;
    QString essayBook;
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
    QString teacherKoreanName;
    QString roomNumber;
};

struct ScheduleEditorClassInfoReadOutcome final
{
    ScheduleEditorClassInfoReadRecord record;
    bool parentMissing = false;
};

struct SelectedClassGradeReadRecord final
{
    int classId = -1;
    QString classGrade;
};

struct SelectedClassGradeReadOutcome final
{
    SelectedClassGradeReadRecord record;
    bool parentMissing = false;
};

struct ClassSubtitleBatchReadRecord final
{
    int classId = -1;
    bool parentMissing = false;
    qint64 teacherId = -1;
    QString grade;
    QString level;
    QList<ClassTime> regularTimes;
};

struct MyClassesClassInformationReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
    QString notes;
    QString timeFillerActivities;
};

struct MyClassesClassSummaryReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
};

struct MyClassesClassSummaryBatchReadEntry final
{
    int classId = -1;
    RepositoryResult<MyClassesClassSummaryReadRecord> summary;
    bool parentMissing = false;
};

struct MyClassesClassInformationBatchReadEntry final
{
    int classId = -1;
    RepositoryResult<MyClassesClassInformationReadRecord> information;
    bool parentMissing = false;
};

struct RosterPrintClassInfoReadRecord final
{
    int classId = -1;
    bool parentMissing = false;
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

struct RosterPrintClassInfoReadOutcome final
{
    RosterPrintClassInfoReadRecord record;
    bool parentMissing = false;
};

struct RosterPrintClassInfoBatchReadMetrics final
{
    int callCount = 0;
    int requestedClassCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
};

struct SubPrepRosterOutputClassInfoReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
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

struct SubPrepRosterOutputScheduleReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QList<ClassTime> meetings;
};

struct SubPrepPrintSourceClassInfoReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
    QString notes;
    QList<ClassTime> classTimes;
    QList<ClassTime> intensiveTimes;
};

struct SubPrepClassDetailsReadOutcome final
{
    SubPrepClassDetailsRecord record;
    bool parentMissing = false;
};

struct SubPrepRosterOutputScheduleBatchReadMetrics final
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
    qint64 completedClassCount = 0;
    qint64 regularScheduleRowCount = 0;
    qint64 intensiveScheduleRowCount = 0;
};

struct ScheduleImportStateSnapshotClassInfoReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QString classColor{"#FFFFFF"};
    QString roomNumber;
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;
};

struct ScheduleImportStateSnapshotClassInfoReadMetrics final
{
    int callCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
    int intensiveScheduleStatementCount = 0;
    // Each count advances only after its query completes and row validation passes.
    int metadataReturnedRowCount = 0;
    int regularScheduleReturnedRowCount = 0;
    int intensiveScheduleReturnedRowCount = 0;
};

struct ClassPageDetailsReadMetrics final
{
    int callCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
};

struct ClassNotesPageReadMetrics final
{
    int callCount = 0;
    int metadataStatementCount = 0;
    int regularScheduleStatementCount = 0;
};

struct ClassDetailsPageReadMetrics final
{
    int callCount = 0;
    int metadataStatementCount = 0;
    int scheduleStatementCount = 0;
};

struct ScheduleEditorClassInfoReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
};

struct SelectedClassGradeReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
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

struct MyClassesClassSummaryBatchReadMetrics final
{
    int callCount = 0;
    int requestedClassCount = 0;
    int metadataStatementCount = 0;
    int scheduleStatementCount = 0;
    int fallbackClassReadCount = 0;
};

// Navigation summaries contain only the fields used to build schedule
// navigation metadata. Student counts are explicitly absent on this read path.
struct SubPrepClassSummaryNavigationReadRecord final
{
    int classId = -1;
    qint64 teacherId = -1;
    QString classGrade;
    QString classLevel;
    QString teacherKr;
    QString teacherEn;
    QString teacherPreferredRomanization;
    QString teacherPreferredName;
    std::optional<qint64> studentCount;
    QList<SubPrepScheduleMeetingRecord> meetings;
};

class ClassInfoRepository
{
public:
    explicit ClassInfoRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryStatus saveClassInfo(
        const ClassInfo& info
        );
    // The typed class-details path retains exact teacher identity separately
    // from the legacy int field, including an existing assignment.
    [[nodiscard]] RepositoryStatus saveClassDetails(
        const ClassInfo& info,
        qint64 teacherId
        );
    // Typed callers can identify a missing parent before loading or checking
    // editable details, and retain that provenance if the parent disappears
    // before the write transaction begins.
    [[nodiscard]] RepositoryStatus
        checkClassDetailsParentDetailed(int classId);
    [[nodiscard]] RepositoryStatus saveClassDetailsDetailed(
        const ClassInfo& info,
        qint64 teacherId
        );
    [[nodiscard]] RepositoryResult<ClassDetailsSaveReadRecord>
        loadClassDetailsSaveInfo(
        int classId
        );

    [[nodiscard]] RepositoryStatus saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
    // Typed callers can distinguish a missing parent class from other save
    // failures while preserving the legacy diagnostic above.
    [[nodiscard]] RepositoryStatus saveClassNotesDetailed(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
    [[nodiscard]] RepositoryStatus saveClassCoTeacherAssignment(
        int classId,
        std::optional<qint64> teacherId
        );
    [[nodiscard]] RepositoryStatus
        saveClassCoTeacherAssignmentDetailed(
            int classId,
            std::optional<qint64> teacherId
            );

    [[nodiscard]] RepositoryResult<ClassInfo> loadClassInfo(
        int classId
        );
    [[nodiscard]] RepositoryResult<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
        loadClassCoTeacherAssignmentValidationSnapshot(int classId);
    [[nodiscard]] RepositoryResult<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
        loadClassCoTeacherAssignmentValidationSnapshotDetailed(int classId);
    [[nodiscard]] RepositoryResult<ClassPageDetailsReadRecord>
        loadClassPageDetails(
        int classId
        );
    // Typed callers can distinguish an absent parent while the legacy page
    // reader keeps its successful default/orphan behavior for unknown IDs.
    [[nodiscard]] RepositoryResult<ClassPageDetailsReadOutcome>
        loadClassPageDetailsDetailed(int classId);
    [[nodiscard]] RepositoryResult<ClassCoTeacherPageDetailsReadRecord>
        loadClassCoTeacherPageDetails(int classId);
    // Typed callers can distinguish a missing parent class while the legacy
    // record-returning API keeps its successful defaults for unknown IDs.
    [[nodiscard]] RepositoryResult<ClassCoTeacherPageDetailsReadOutcome>
        loadClassCoTeacherPageDetailsDetailed(int classId);
    [[nodiscard]] RepositoryResult<ClassNotesPageReadRecord> loadClassNotesPageRecord(
        int classId
        );
    // Typed callers can distinguish an absent parent without changing the
    // legacy record's default-success behavior.
    [[nodiscard]] RepositoryResult<ClassNotesPageReadOutcome>
        loadClassNotesPageRecordDetailed(int classId);
    [[nodiscard]] RepositoryResult<ClassDetailsPageReadRecord>
        loadClassDetailsPageRecord(int classId);
    [[nodiscard]] RepositoryResult<ClassDetailsPageReadOutcome>
        loadClassDetailsPageRecordDetailed(int classId);
    [[nodiscard]] RepositoryResult<ScheduleEditorClassInfoReadRecord>
        loadScheduleEditorClassInfoRecord(int classId);
    // Typed callers can distinguish an absent class parent while the legacy
    // wrapper above retains its successful default record for unknown IDs.
    [[nodiscard]] RepositoryResult<ScheduleEditorClassInfoReadOutcome>
        loadScheduleEditorClassInfoRecordDetailed(int classId);
    [[nodiscard]] RepositoryResult<SelectedClassGradeReadRecord>
        loadSelectedClassGradeRecord(int classId);
    // Typed callers can distinguish an absent parent class from a class that
    // exists without grade metadata while the legacy wrapper keeps its empty
    // successful result for either case.
    [[nodiscard]] RepositoryResult<SelectedClassGradeReadOutcome>
        loadSelectedClassGradeRecordDetailed(int classId);
    [[nodiscard]] RepositoryResult<ClassTransferExportClassInfoReadRecord>
        loadClassTransferExportClassInfoRecord(int classId);
    [[nodiscard]] RepositoryResult<QList<ClassTransferExportClassInfoReadRecord>>
        loadClassTransferExportClassInfoRecords(const QList<int>& classIds);
    [[nodiscard]] RepositoryResult<QList<ClassInfo>> loadClassInfoRecords(
        const QList<int>& classIds
        );
    [[nodiscard]] RepositoryResult<ClassSubtitleReadRecord> loadClassSubtitleRecord(
        int classId
        );
    // Typed callers can distinguish an absent parent while the legacy
    // record-returning wrapper keeps its successful defaults for unknown IDs.
    [[nodiscard]] RepositoryResult<ClassSubtitleReadOutcome>
        loadClassSubtitleRecordDetailed(int classId);
    [[nodiscard]] RepositoryResult<QList<ClassSubtitleBatchReadRecord>>
        loadClassSubtitleRecords(const QList<int>& classIds);
    [[nodiscard]] RepositoryResult<std::vector<MyClassesClassInformationBatchReadEntry>>
        loadMyClassesClassInformationRecords(const QList<int>& classIds);
    [[nodiscard]] RepositoryResult<std::vector<MyClassesClassSummaryBatchReadEntry>>
        loadMyClassesClassSummaryRecords(const QList<int>& classIds);
    [[nodiscard]] RepositoryResult<RosterPrintClassInfoReadRecord>
        loadRosterPrintClassInfoRecord(int classId);
    [[nodiscard]] RepositoryResult<RosterPrintClassInfoReadOutcome>
        loadRosterPrintClassInfoRecordDetailed(int classId);
    [[nodiscard]] RepositoryResult<QList<RosterPrintClassInfoReadRecord>>
        loadRosterPrintClassInfoRecords(const QList<int>& classIds);
    [[nodiscard]] RepositoryResult<QList<SubPrepRosterOutputClassInfoReadRecord>>
        loadSubPrepRosterOutputClassInfoRecords(const QList<int>& classIds);
    [[nodiscard]] RepositoryResult<QList<SubPrepRosterOutputScheduleReadRecord>>
        loadSubPrepRosterOutputScheduleRecords(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );

    [[nodiscard]] RepositoryResult<QList<ClassNavigationReadRecord>>
        loadClassesNavigationRecords(const QList<int>& classIds);
    // Parent-aware typed route for Next navigation. The legacy reader above
    // intentionally retains its orphan/default behavior for Class Transfer.
    [[nodiscard]] RepositoryResult<QList<ClassNavigationParentAwareReadRecord>>
        loadClassesNavigationRecordsWithParentProvenance(
            const QList<int>& classIds
            );
    [[nodiscard]] const ClassesNavigationReadMetrics&
        classesNavigationReadMetrics() const noexcept;
    [[nodiscard]] const ScheduleClassInfoReadMetrics&
        scheduleClassInfoReadMetrics() const noexcept;
    [[nodiscard]] const ClassPageDetailsReadMetrics&
        classPageDetailsReadMetrics() const noexcept;
    [[nodiscard]] const ClassNotesPageReadMetrics&
        classNotesPageReadMetrics() const noexcept;
    [[nodiscard]] const ClassDetailsPageReadMetrics&
        classDetailsPageReadMetrics() const noexcept;
    [[nodiscard]] const ScheduleEditorClassInfoReadMetrics&
        scheduleEditorClassInfoReadMetrics() const noexcept;
    [[nodiscard]] const SelectedClassGradeReadMetrics&
        selectedClassGradeReadMetrics() const noexcept;
    [[nodiscard]] const ClassSubtitleBatchReadMetrics&
        classSubtitleBatchReadMetrics() const noexcept;
    [[nodiscard]] const MyClassesClassInformationBatchReadMetrics&
        myClassesClassInformationBatchReadMetrics() const noexcept;
    [[nodiscard]] const MyClassesClassSummaryBatchReadMetrics&
        myClassesClassSummaryBatchReadMetrics() const noexcept;
    [[nodiscard]] const SubPrepRosterOutputClassInfoBatchReadMetrics&
        subPrepRosterOutputClassInfoBatchReadMetrics() const noexcept;
    [[nodiscard]] const SubPrepRosterOutputScheduleBatchReadMetrics&
        subPrepRosterOutputScheduleBatchReadMetrics() const noexcept;
    [[nodiscard]] const RosterPrintClassInfoBatchReadMetrics&
        rosterPrintClassInfoBatchReadMetrics() const noexcept;

    [[nodiscard]] RepositoryResult<SubPrepClassDetailsRecord>
        loadSubPrepClassDetails(int classId);
    [[nodiscard]] RepositoryResult<SubPrepClassDetailsReadOutcome>
        loadSubPrepClassDetailsDetailed(int classId);

    [[nodiscard]] RepositoryResult<QList<SubPrepClassSummaryRecord>>
        loadSubPrepClassSummaries(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );
    [[nodiscard]] RepositoryResult<QList<SubPrepClassSummaryNavigationReadRecord>>
        loadSubPrepClassNavigationSummaries(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );

    [[nodiscard]] RepositoryResult<QList<ClassInfo>> loadClassInfosForScheduleScope(
        const QList<int>& classIds,
        const QStringList& selectedDays,
        ScheduleType type,
        int maxMeetingsPerClass,
        int maxTotalMeetings,
        bool includeUnassignedTeachers = false
        );
    [[nodiscard]] RepositoryResult<QList<SubPrepPrintSourceClassInfoReadRecord>>
        loadSubPrepPrintSourceClassInfosForScheduleScope(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );

    [[nodiscard]] RepositoryResult<QList<ClassTeacherAssignment>>
        loadClassTeacherAssignments();
    [[nodiscard]] RepositoryResult<QList<ClassTeacherAssignmentReadRecord>>
        loadClassTeacherAssignmentRecords();

    [[nodiscard]] RepositoryResult<QList<ClassInfo>> loadScheduleClassInfos();
    using ScheduleClassInfoVisitor =
        std::function<RepositoryStatus(const ClassInfo&)>;
    using ScheduleClassTimeVisitor = std::function<RepositoryStatus(
        int,
        const ClassTime&,
        bool
        )>;
    [[nodiscard]] RepositoryStatus visitScheduleClassInfoRows(
        const ScheduleClassInfoVisitor& classInfoVisitor,
        const ScheduleClassTimeVisitor& scheduleTimeVisitor
        );
    [[nodiscard]] RepositoryResult<
        QList<ScheduleImportStateSnapshotClassInfoReadRecord>>
        loadScheduleImportStateSnapshotClassInfos();
    [[nodiscard]] const ScheduleImportStateSnapshotClassInfoReadMetrics&
        scheduleImportStateSnapshotClassInfoReadMetrics() const noexcept;

    [[nodiscard]] RepositoryResult<QList<ClassConflict>> getClassTimeConflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        );

private:
    enum class ClassInfoSavePath
    {
        Legacy,
        Typed
    };

    [[nodiscard]] RepositoryResult<QList<SubPrepClassSummaryRecord>>
        loadSubPrepClassSummariesImpl(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings,
            bool includeStudentCounts
            );

    [[nodiscard]] RepositoryStatus writeClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
    [[nodiscard]] RepositoryStatus saveClassInfoWithTeacherId(
        const ClassInfo& info,
        qint64 teacherId
        );
    [[nodiscard]] RepositoryResult<bool>
        classDetailsParentExists(int classId);
    [[nodiscard]] RepositoryStatus
        saveClassInfoWithTeacherIdDetailed(
        const ClassInfo& info,
        qint64 teacherId,
        ClassInfoSavePath savePath
        );
    struct MyClassesClassSummaryReadOutcome final
    {
        MyClassesClassSummaryReadRecord record;
        bool parentMissing = false;
    };
    [[nodiscard]] RepositoryResult<MyClassesClassSummaryReadOutcome>
        loadMyClassesClassSummaryRecord(int classId);
    struct MyClassesClassInformationReadOutcome final
    {
        MyClassesClassInformationReadRecord record;
        bool parentMissing = false;
    };
    [[nodiscard]] RepositoryResult<MyClassesClassInformationReadOutcome>
        loadMyClassesClassInformationRecord(int classId);

    QSqlDatabase& m_database;
    ClassesNavigationReadMetrics m_classesNavigationReadMetrics;
    ScheduleClassInfoReadMetrics m_scheduleClassInfoReadMetrics;
    ScheduleImportStateSnapshotClassInfoReadMetrics
        m_scheduleImportStateSnapshotClassInfoReadMetrics;
    ClassPageDetailsReadMetrics m_classPageDetailsReadMetrics;
    ClassNotesPageReadMetrics m_classNotesPageReadMetrics;
    ClassDetailsPageReadMetrics m_classDetailsPageReadMetrics;
    ScheduleEditorClassInfoReadMetrics m_scheduleEditorClassInfoReadMetrics;
    SelectedClassGradeReadMetrics m_selectedClassGradeReadMetrics;
    ClassSubtitleBatchReadMetrics m_classSubtitleBatchReadMetrics;
    MyClassesClassInformationBatchReadMetrics
        m_myClassesClassInformationBatchReadMetrics;
    MyClassesClassSummaryBatchReadMetrics
        m_myClassesClassSummaryBatchReadMetrics;
    SubPrepRosterOutputClassInfoBatchReadMetrics
        m_subPrepRosterOutputClassInfoBatchReadMetrics;
    SubPrepRosterOutputScheduleBatchReadMetrics
        m_subPrepRosterOutputScheduleBatchReadMetrics;
    RosterPrintClassInfoBatchReadMetrics
        m_rosterPrintClassInfoBatchReadMetrics;
};
