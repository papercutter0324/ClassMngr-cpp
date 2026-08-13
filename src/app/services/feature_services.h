#pragma once

#include "core/enums/schedule_type.h"
#include "core/result.h"
#include "domain/models/calendar_event.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
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

#include <QPair>
#include <QVariant>

class DataService;
class DatabaseSession;

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
    void save(const QString& key, const QVariant& value) const;
    QVariant load(const QString& key, const QVariant& defaultValue = {}) const;
};

class TeacherService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Result<int> create(const Teacher& teacher) const;
    [[nodiscard]] Result<int> save(const Teacher& teacher) const;
    [[nodiscard]] Status update(const Teacher& teacher) const;
    Teacher teacher(int teacherId) const;
    QList<Teacher> teachers() const;
    [[nodiscard]] Status remove(int teacherId) const;
    QList<NativeEnglishTeacher> nativeEnglishTeachers() const;
    Status saveNativeEnglishTeacherDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        ) const;
    QList<GsTeamMember> gsTeamMembers() const;
    Status saveGsTeamDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        ) const;
    Result<TeacherImportSummary> importTeachers(const TeacherImportPlan& plan) const;
    QDate latestImportDate() const;
};

class ClassService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    [[nodiscard]] Result<int> create(const QString& name) const;
    QList<Classroom> classes() const;
    Classroom classroom(int classId) const;
    [[nodiscard]] Status rename(int classId, const QString& name) const;
    [[nodiscard]] Status remove(int classId) const;
    ClassInfo classInfo(int classId) const;
    bool saveClassInfo(const ClassInfo& info) const;
    bool saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        ) const;
    QList<ClassConflict> conflicts(
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
    Result<ScheduleImportSummary> importSchedule(const ScheduleImportPlan& plan) const;
    QList<IntensiveSlotState> intensiveSlotStates() const;
    void saveIntensiveSlotState(
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
    QList<CalendarEvent> eventsForDate(const QDate& date) const;
    QList<CalendarEvent> eventsInRange(const QDate& startDate, const QDate& endDate) const;
    QList<CalendarEvent> upcomingEvents(const QDate& fromDate, int limit) const;
    CalendarEvent event(int eventId) const;
    QList<CalendarEvent> repeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        ) const;
    int saveEvent(const CalendarEvent& event) const;
    void deleteEvent(int eventId) const;
    void deleteRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        ) const;
    void deleteAllEvents() const;
};

class RosterService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    void saveRoster(int classId, const Roster& roster) const;
    bool saveRosters(const QList<QPair<int, Roster>>& rosters) const;
    Roster roster(int classId) const;
    int studentCount(int classId) const;
};

class SpeakingEvaluationService final : public FeatureService
{
public:
    using FeatureService::FeatureService;
    bool saveEvaluation(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        ) const;
    SpeakingEvalRows evaluation(int classId, const QString& evaluationName) const;
    QList<SpeakingEvalScore> rosterScoreImport(
        int classId,
        const QString& evaluationName
        ) const;
};
