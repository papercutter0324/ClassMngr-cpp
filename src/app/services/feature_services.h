#pragma once

#include "core/enums/schedule_type.h"
#include "core/result.h"
#include "domain/models/calendar_event.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/class_teacher_assignment.h"
#include "domain/models/class_transfer.h"
#include "domain/models/classroom.h"
#include "domain/models/gs_team_member.h"
#include "domain/models/intensive_slot_state.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/roster.h"
#include "domain/models/schedule_import.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/teacher.h"
#include "domain/models/teacher_import.h"
#include "domain/models/testing_block.h"
#include "domain/models/testing_class.h"
#include "features/classes/services/speaking_analytics.h"

#include <QPair>
#include <QVariant>

class DataService;
class DatabaseSession;

// The distinct views required by the Class Analytics dashboard.  The selected
// snapshot keeps the existing current-roster policy; the Class Shape snapshot
// may instead be the latest completed named evaluation when All is selected.
struct SpeakingEvaluationDashboard
{
    SpeakingAnalytics::Snapshot selectedSnapshot;
    QString classShapeEvaluationName;
    SpeakingAnalytics::Snapshot classShapeSnapshot;
    QList<SpeakingAnalytics::YearToDatePoint> yearToDatePoints;
};

class FeatureService
{
public:
    explicit FeatureService(DataService* dataService);
    FeatureService(DatabaseSession* session, DataService* legacyDataService);
    [[nodiscard]] bool isAvailable() const;

protected:
    DatabaseSession* session() const;
    DataService* dataService() const;

private:
    DatabaseSession* m_session = nullptr;
    DataService* m_legacyDataService = nullptr;
};

class SettingsService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] DatabaseSession* databaseSession() const;
    [[nodiscard]] Status save(
        const QString& key,
        const QVariant& value
        ) const;
    [[nodiscard]] Status saveAll(
        const QVariantMap& values
        ) const;
    [[nodiscard]] Result<QVariant> load(const QString& key) const;
    [[nodiscard]] QVariant loadOrDefault(
        const QString& key,
        const QVariant& defaultValue
        ) const;
};

class TeacherService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Result<int> create(const Teacher& teacher) const;
    [[nodiscard]] Result<int> save(const Teacher& teacher) const;
    [[nodiscard]] Status update(const Teacher& teacher) const;
    [[nodiscard]] Result<Teacher> teacher(int teacherId) const;
    [[nodiscard]] Result<QList<Teacher>> teachers() const;
    [[nodiscard]] Status remove(int teacherId) const;
    [[nodiscard]] Result<QList<NativeEnglishTeacher>> nativeEnglishTeachers() const;
    Status saveNativeEnglishTeacherDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        ) const;
    [[nodiscard]] Result<QList<GsTeamMember>> gsTeamMembers() const;
    Status saveGsTeamDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        ) const;
    Result<TeacherImportSummary> importTeachers(const TeacherImportPlan& plan) const;
    [[nodiscard]] Result<QDate> latestImportDate() const;
};

class ClassService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Result<int> create(const QString& name) const;
    [[nodiscard]] Result<QList<Classroom>> classes() const;
    [[nodiscard]] Result<QList<ClassTeacherAssignment>>
        classTeacherAssignments() const;
    [[nodiscard]] Result<QList<ClassInfo>> scheduleClassInfos() const;
    [[nodiscard]] Result<Classroom> classroom(int classId) const;
    [[nodiscard]] Status rename(int classId, const QString& name) const;
    [[nodiscard]] Status remove(int classId) const;
    [[nodiscard]] Result<ClassInfo> classInfo(int classId) const;
    [[nodiscard]] Status saveClassInfo(const ClassInfo& info) const;
    [[nodiscard]] Status saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        ) const;
    [[nodiscard]] Result<QList<ClassConflict>> conflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        ) const;
    Result<ClassTransferPackage> buildTransferPackage(const QList<int>& classIds) const;
    Result<ClassImportPreview> previewImport(const ClassTransferPackage& package) const;
    Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        ) const;
};

class ScheduleService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    Result<ScheduleImportPreview> previewImport(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind
        ) const;
    [[nodiscard]] Status validateImport(const ScheduleImportPlan& plan) const;
    Result<ScheduleImportSummary> importSchedule(const ScheduleImportPlan& plan) const;
    [[nodiscard]] Result<QList<IntensiveSlotState>> intensiveSlotStates() const;
    [[nodiscard]] Status saveIntensiveSlotState(
        const QString& day,
        const QString& startTime,
        const QString& state,
        const QString& defaultState = QStringLiteral("essay")
        ) const;
    Result<QList<TestingAssignment>> testingAssignments() const;
    Result<QList<TestingBlock>> testingBlocks() const;
    Status saveTestingBlock(
        const QString& day,
        const QString& startTime,
        const QString& room,
        bool replaceExisting = false
        ) const;
    Status assignTestingClass(
        const QString& day,
        const QString& startTime,
        int classId,
        bool replaceExisting = false
        ) const;
    Status deleteTestingAssignment(const QString& day, const QString& startTime) const;
    Status deleteTestingBlock(const QString& day, const QString& startTime) const;
    Status clearTestingAssignments() const;
    Status clearTestingBlocks() const;
    Result<int> createTestingClass(
        const TestingClass& testingClass,
        const QString& assignmentDay = {},
        const QString& assignmentStartTime = {}
        ) const;
    Status updateTestingClass(const TestingClass& testingClass) const;
    Result<TestingClass> testingClass(int classId) const;
    Result<QList<TestingClass>> testingClasses() const;
    Status deleteTestingClass(int classId) const;
    Result<bool> isTestingClass(int classId) const;
};

class CalendarService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Result<QList<CalendarEvent>> eventsForDate(const QDate& date) const;
    [[nodiscard]] Result<QList<CalendarEvent>> eventsInRange(const QDate& startDate, const QDate& endDate) const;
    [[nodiscard]] Result<QList<CalendarEvent>> upcomingEvents(const QDate& fromDate, int limit) const;
    [[nodiscard]] Result<CalendarEvent> event(int eventId) const;
    [[nodiscard]] Result<QList<CalendarEvent>> repeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        ) const;
    [[nodiscard]] Result<int> saveEvent(const CalendarEvent& event) const;
    [[nodiscard]] Result<QList<int>> saveEvents(
        const QList<CalendarEvent>& events
        ) const;
    [[nodiscard]] Status deleteEvent(int eventId) const;
    [[nodiscard]] Status deleteRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        ) const;
    [[nodiscard]] Status deleteAllEvents() const;
};

class RosterService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Status saveRoster(
        int classId,
        const Roster& roster,
        bool allowQuestionableKoreanNameLengths = false
        ) const;
    [[nodiscard]] Status saveRosters(
        const QList<QPair<int, Roster>>& rosters
        ) const;
    [[nodiscard]] Result<Roster> roster(int classId) const;
    [[nodiscard]] Result<int> studentCount(int classId) const;
};

class SpeakingEvaluationService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Status saveEvaluation(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {},
        bool allowQuestionableKoreanNameLengths = false
        ) const;
    [[nodiscard]] Result<SpeakingEvalRows> evaluation(int classId, const QString& evaluationName) const;
    [[nodiscard]] Result<SpeakingAnalytics::Snapshot> analytics(
        int classId,
        const QString& evaluationName
    ) const;
    [[nodiscard]] Result<SpeakingEvaluationDashboard> analyticsDashboard(
        int classId,
        const QString& evaluationName
    ) const;
    [[nodiscard]] Result<QList<SpeakingEvalScore>> rosterScoreImport(
        int classId,
        const QString& evaluationName
        ) const;
};
