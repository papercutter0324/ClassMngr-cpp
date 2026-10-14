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
#include <QtGlobal>

#include <expected>
#include <optional>
#include <vector>

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
    int teacherId = -1;
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

enum class ClassCoTeacherAssignmentValidationSnapshotFailureKind
{
    Technical,
    ClassNotFound
};

struct ClassCoTeacherAssignmentValidationSnapshotReadFailure final
{
    ClassCoTeacherAssignmentValidationSnapshotFailureKind kind =
        ClassCoTeacherAssignmentValidationSnapshotFailureKind::Technical;
    QString message;
};

using ClassCoTeacherAssignmentValidationSnapshotDetailedResult =
    std::expected<
        ClassCoTeacherAssignmentValidationSnapshotReadRecord,
        ClassCoTeacherAssignmentValidationSnapshotReadFailure
        >;

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
    Result<MyClassesClassSummaryReadRecord> summary;
    bool parentMissing = false;
};

struct MyClassesClassInformationBatchReadEntry final
{
    int classId = -1;
    Result<MyClassesClassInformationReadRecord> information;
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

class ClassInfoRepository
{
public:
    struct ClassNotesSaveFailure final
    {
        enum class Kind
        {
            ParentClassMissing,
            Technical
        };

        Kind kind = Kind::Technical;
        QString diagnostic;
    };

    using ClassNotesSaveDetailedResult =
        std::expected<void, ClassNotesSaveFailure>;

    struct ClassDetailsSaveFailure final
    {
        enum class Kind
        {
            ParentClassMissing,
            Technical
        };

        Kind kind = Kind::Technical;
        QString diagnostic;
    };

    using ClassDetailsSaveDetailedResult =
        std::expected<void, ClassDetailsSaveFailure>;

    using ClassDetailsParentCheckDetailedResult =
        std::expected<void, ClassDetailsSaveFailure>;

    explicit ClassInfoRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Status saveClassInfo(
        const ClassInfo& info
        );
    // The typed class-details path retains exact teacher identity separately
    // from the legacy int field, including an existing assignment.
    [[nodiscard]] Status saveClassDetails(
        const ClassInfo& info,
        qint64 teacherId
        );
    // Typed callers can identify a missing parent before loading or checking
    // editable details, and retain that provenance if the parent disappears
    // before the write transaction begins.
    [[nodiscard]] ClassDetailsParentCheckDetailedResult
        checkClassDetailsParentDetailed(int classId);
    [[nodiscard]] ClassDetailsSaveDetailedResult saveClassDetailsDetailed(
        const ClassInfo& info,
        qint64 teacherId
        );
    [[nodiscard]] Result<ClassDetailsSaveReadRecord> loadClassDetailsSaveInfo(
        int classId
        );

    [[nodiscard]] Status saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
    // Typed callers can distinguish a missing parent class from other save
    // failures without changing the legacy Status/QString API above.
    [[nodiscard]] ClassNotesSaveDetailedResult saveClassNotesDetailed(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
    [[nodiscard]] Status saveClassCoTeacherAssignment(
        int classId,
        std::optional<qint64> teacherId
        );

    [[nodiscard]] Result<ClassInfo> loadClassInfo(
        int classId
        );
    [[nodiscard]] Result<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
        loadClassCoTeacherAssignmentValidationSnapshot(int classId);
    [[nodiscard]] ClassCoTeacherAssignmentValidationSnapshotDetailedResult
        loadClassCoTeacherAssignmentValidationSnapshotDetailed(int classId);
    [[nodiscard]] Result<ClassPageDetailsReadRecord> loadClassPageDetails(
        int classId
        );
    // Typed callers can distinguish an absent parent while the legacy page
    // reader keeps its successful default/orphan behavior for unknown IDs.
    [[nodiscard]] Result<ClassPageDetailsReadOutcome>
        loadClassPageDetailsDetailed(int classId);
    [[nodiscard]] Result<ClassCoTeacherPageDetailsReadRecord>
        loadClassCoTeacherPageDetails(int classId);
    // Typed callers can distinguish a missing parent class while the legacy
    // record-returning API keeps its successful defaults for unknown IDs.
    [[nodiscard]] Result<ClassCoTeacherPageDetailsReadOutcome>
        loadClassCoTeacherPageDetailsDetailed(int classId);
    [[nodiscard]] Result<ClassNotesPageReadRecord> loadClassNotesPageRecord(
        int classId
        );
    // Typed callers can distinguish an absent parent without changing the
    // legacy record's default-success behavior.
    [[nodiscard]] Result<ClassNotesPageReadOutcome>
        loadClassNotesPageRecordDetailed(int classId);
    [[nodiscard]] Result<ClassDetailsPageReadRecord>
        loadClassDetailsPageRecord(int classId);
    [[nodiscard]] Result<ClassDetailsPageReadOutcome>
        loadClassDetailsPageRecordDetailed(int classId);
    [[nodiscard]] Result<ScheduleEditorClassInfoReadRecord>
        loadScheduleEditorClassInfoRecord(int classId);
    // Typed callers can distinguish an absent class parent while the legacy
    // wrapper above retains its successful default record for unknown IDs.
    [[nodiscard]] Result<ScheduleEditorClassInfoReadOutcome>
        loadScheduleEditorClassInfoRecordDetailed(int classId);
    [[nodiscard]] Result<SelectedClassGradeReadRecord>
        loadSelectedClassGradeRecord(int classId);
    // Typed callers can distinguish an absent parent class from a class that
    // exists without grade metadata while the legacy wrapper keeps its empty
    // successful result for either case.
    [[nodiscard]] Result<SelectedClassGradeReadOutcome>
        loadSelectedClassGradeRecordDetailed(int classId);
    [[nodiscard]] Result<ClassTransferExportClassInfoReadRecord>
        loadClassTransferExportClassInfoRecord(int classId);
    [[nodiscard]] Result<QList<ClassTransferExportClassInfoReadRecord>>
        loadClassTransferExportClassInfoRecords(const QList<int>& classIds);
    [[nodiscard]] Result<QList<ClassInfo>> loadClassInfoRecords(
        const QList<int>& classIds
        );
    [[nodiscard]] Result<ClassSubtitleReadRecord> loadClassSubtitleRecord(
        int classId
        );
    // Typed callers can distinguish an absent parent while the legacy
    // record-returning wrapper keeps its successful defaults for unknown IDs.
    [[nodiscard]] Result<ClassSubtitleReadOutcome>
        loadClassSubtitleRecordDetailed(int classId);
    [[nodiscard]] Result<QList<ClassSubtitleBatchReadRecord>>
        loadClassSubtitleRecords(const QList<int>& classIds);
    [[nodiscard]] Result<std::vector<MyClassesClassInformationBatchReadEntry>>
        loadMyClassesClassInformationRecords(const QList<int>& classIds);
    [[nodiscard]] Result<std::vector<MyClassesClassSummaryBatchReadEntry>>
        loadMyClassesClassSummaryRecords(const QList<int>& classIds);
    [[nodiscard]] Result<RosterPrintClassInfoReadRecord>
        loadRosterPrintClassInfoRecord(int classId);
    [[nodiscard]] Result<RosterPrintClassInfoReadOutcome>
        loadRosterPrintClassInfoRecordDetailed(int classId);
    [[nodiscard]] Result<QList<RosterPrintClassInfoReadRecord>>
        loadRosterPrintClassInfoRecords(const QList<int>& classIds);
    [[nodiscard]] Result<QList<SubPrepRosterOutputClassInfoReadRecord>>
        loadSubPrepRosterOutputClassInfoRecords(const QList<int>& classIds);
    [[nodiscard]] Result<QList<SubPrepRosterOutputScheduleReadRecord>>
        loadSubPrepRosterOutputScheduleRecords(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );

    [[nodiscard]] Result<QList<ClassNavigationReadRecord>>
        loadClassesNavigationRecords(const QList<int>& classIds);
    // Parent-aware typed route for Next navigation. The legacy reader above
    // intentionally retains its orphan/default behavior for Class Transfer.
    [[nodiscard]] Result<QList<ClassNavigationParentAwareReadRecord>>
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
    [[nodiscard]] Result<QList<SubPrepPrintSourceClassInfoReadRecord>>
        loadSubPrepPrintSourceClassInfosForScheduleScope(
            const QList<int>& classIds,
            const QStringList& selectedDays,
            ScheduleType type,
            int maxMeetingsPerClass,
            int maxTotalMeetings
            );

    [[nodiscard]] Result<QList<ClassTeacherAssignment>>
        loadClassTeacherAssignments();

    [[nodiscard]] Result<QList<ClassInfo>> loadScheduleClassInfos();
    [[nodiscard]] Result<
        QList<ScheduleImportStateSnapshotClassInfoReadRecord>>
        loadScheduleImportStateSnapshotClassInfos();
    [[nodiscard]] const ScheduleImportStateSnapshotClassInfoReadMetrics&
        scheduleImportStateSnapshotClassInfoReadMetrics() const noexcept;

    [[nodiscard]] Result<QList<ClassConflict>> getClassTimeConflicts(
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

    [[nodiscard]] Status writeClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
    [[nodiscard]] Status saveClassInfoWithTeacherId(
        const ClassInfo& info,
        qint64 teacherId
        );
    [[nodiscard]] std::expected<bool, ClassDetailsSaveFailure>
        classDetailsParentExists(int classId);
    [[nodiscard]] ClassDetailsSaveDetailedResult
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
    [[nodiscard]] Result<MyClassesClassSummaryReadOutcome>
        loadMyClassesClassSummaryRecord(int classId);
    struct MyClassesClassInformationReadOutcome final
    {
        MyClassesClassInformationReadRecord record;
        bool parentMissing = false;
    };
    [[nodiscard]] Result<MyClassesClassInformationReadOutcome>
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
