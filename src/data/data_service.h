#pragma once

#include "core/enums/schedule_type.h"
#include "domain/models/calendar_event.h"
#include "domain/models/campus.h"
#include "domain/models/class_info.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_transfer.h"
#include "domain/models/classroom.h"
#include "domain/models/intensive_slot_state.h"
#include "domain/models/testing_block.h"
#include "domain/models/testing_class.h"
#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/schedule_import.h"
#include "domain/models/teacher.h"
#include "domain/models/teacher_import.h"
#include "core/result.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QVariantMap>

#include <memory>

class CampusRecordRepository;
class CalendarEventRepository;
class DatabaseSession;
class ClassInfoRepository;
class ClassRepository;
class ClassTransferRepository;
class IntensiveSlotStateRepository;
class GsTeamRepository;
class NativeEnglishTeacherRepository;
class RosterRepository;
class ScheduleImportRepository;
class SettingsRepository;
class SpeakingEvalRepository;
class TeacherRepository;
class TeacherImportRepository;
class TestingBlockRepository;
class TestingClassRepository;

// =========================================================
// Data Service
// =========================================================

// Compatibility facade for legacy callers while they migrate to the feature
// services exposed by ApplicationServices. Operations are forwarded to the
// engine-backed repositories; new UI and controller code should depend on
// those narrow services instead of adding operations here.
class DataService
{
public:

    explicit DataService(
        const QString &dbPath = QString()
        );

    // Explicit compatibility adapter for legacy callers that must share an
    // ApplicationServices-owned session during the transition.
    explicit DataService(
        DatabaseSession& session
        );

    ~DataService();



    // =====================================================
    // Setup
    // =====================================================

    [[nodiscard]] bool open();

    [[nodiscard]] Status openDatabase(
        const QString& dbPath
        );

    void closeDatabase();

    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] QString currentDatabasePath() const;

    // Transitional access used to construct the narrow application services
    // and support legacy migration callers. UI and controllers must not use
    // the session or its repositories directly.
    [[nodiscard]] DatabaseSession* databaseSession() const;

    // Compatibility-only lifecycle hook for an adapter borrowing an
    // ApplicationServices-owned session. New production code must not call it.
    void synchronizeCompatibilityAdapters();

    // =====================================================
    // Settings
    // =====================================================

    Status saveSetting(
        const QString &key,
        const QVariant &value
        );

    [[nodiscard]] Status saveSettings(
        const QVariantMap& values
        );

    [[nodiscard]] Result<QVariant> loadSetting(
        const QString &key
        );



    // =====================================================
    // Teachers
    // =====================================================

    [[nodiscard]] Result<int> createTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] Result<int> saveTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] Status updateTeacher(
        const Teacher& teacher
        );

    [[nodiscard]] Result<QList<Teacher>> getAllTeachers();

    [[nodiscard]] Result<Teacher> getTeacher(
        int teacherId
        );

    [[nodiscard]] Status deleteTeacher(
        int teacherId
        );

    [[nodiscard]] Result<QList<NativeEnglishTeacher>> getNativeEnglishTeachers();

    [[nodiscard]] Status saveNativeEnglishTeacherDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        );

    [[nodiscard]] Result<QList<GsTeamMember>> getGsTeamMembers();

    [[nodiscard]] Status saveGsTeamDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

    [[nodiscard]] Result<QDate> latestTeacherImportDate();



    // =====================================================
    // Classes
    // =====================================================

    [[nodiscard]] Result<int> createClass(
        const QString& name
        );

    [[nodiscard]] Result<QList<Classroom>> getClasses();

    [[nodiscard]] Result<Classroom> getClassById(
        int classId
        );

    [[nodiscard]] Status updateClassName(
        int classId,
        const QString& name
        );

    [[nodiscard]] Status deleteClass(
        int classId
        );

    [[nodiscard]] Result<ClassTransferPackage> buildClassTransferPackage(
        const QList<int>& classIds
        );

    [[nodiscard]] Result<ClassImportPreview> previewClassImport(
        const ClassTransferPackage& package
        );

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        );

    [[nodiscard]] Result<ScheduleImportPreview> previewScheduleImport(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind
        );

    [[nodiscard]] Status validateScheduleImport(
        const ScheduleImportPlan& plan
        );

    [[nodiscard]] Result<ScheduleImportSummary> importSchedule(
        const ScheduleImportPlan& plan
        );



    // =====================================================
    // Class Info
    // =====================================================

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

    // Class details and regular/intensive class times are delegated through
    // ClassInfoRepository to the engine ClassInfoService.



    // =====================================================
    // Intensive Slot States
    // =====================================================

    // Grid-wide intensive-slot state persistence is delegated through the
    // engine-backed IntensiveSlotStateRepository.

    [[nodiscard]] Result<QList<IntensiveSlotState>> loadIntensiveSlotStates();

    [[nodiscard]] Status saveIntensiveSlotState(
        const QString& day,
        const QString& startTime,
        const QString& state,
        const QString& defaultState = QStringLiteral("essay")
        );

    // =====================================================
    // Testing Blocks
    // =====================================================

    [[nodiscard]] Result<QList<TestingAssignment>>
    loadTestingAssignments();

    [[nodiscard]] Result<QList<TestingBlock>> loadTestingBlocks();

    [[nodiscard]] Status saveTestingBlock(
        const QString& day,
        const QString& startTime,
        const QString& room,
        bool replaceExisting = false
        );

    [[nodiscard]] Status assignTestingClass(
        const QString& day,
        const QString& startTime,
        int classId,
        bool replaceExisting = false
        );

    [[nodiscard]] Status deleteTestingAssignment(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] Status deleteTestingBlock(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] Status clearTestingAssignments();

    [[nodiscard]] Status clearTestingBlocks();

    // =====================================================
    // Testing Classes
    // =====================================================

    [[nodiscard]] Result<int> createTestingClass(
        const TestingClass& testingClass,
        const QString& assignmentDay = {},
        const QString& assignmentStartTime = {}
        );

    [[nodiscard]] Status updateTestingClass(
        const TestingClass& testingClass
        );

    [[nodiscard]] Result<TestingClass> loadTestingClass(
        int classId
        );

    [[nodiscard]] Result<QList<TestingClass>> loadTestingClasses();

    [[nodiscard]] Status deleteTestingClass(
        int classId
        );

    [[nodiscard]] Result<bool> isTestingClass(
        int classId
        );



    // =====================================================
    // Calendar Events
    // =====================================================

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsForDate(
        const QDate& date
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadUpcomingCalendarEvents(
        const QDate& fromDate,
        int limit
        );

    [[nodiscard]] Result<CalendarEvent> getCalendarEvent(
        int eventId
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] Result<int> saveCalendarEvent(
        const CalendarEvent& event
        );

    [[nodiscard]] Result<QList<int>> saveCalendarEvents(
        const QList<CalendarEvent>& events
        );

    [[nodiscard]] Result<CalendarEventImportSummary> importCalendarEvents(
        const QList<CalendarEvent>& events,
        int parserSkippedCount
        );

    [[nodiscard]] Status deleteCalendarEvent(
        int eventId
        );

    [[nodiscard]] Status deleteCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] Status deleteAllCalendarEvents();



    // =====================================================
    // Conflict Detection
    // =====================================================

    [[nodiscard]] Result<QList<ClassConflict>> getClassTimeConflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        );

    // Class-time conversion and conflict evaluation are delegated through
    // ClassInfoRepository to the engine ClassScheduleService.



    // =====================================================
    // Roster
    // =====================================================

    [[nodiscard]] Status saveRoster(
        int classId,
        const Roster& roster
        );

    [[nodiscard]] Status saveRosters(
        const QList<QPair<int, Roster>>& rosters
        );

    [[nodiscard]] Result<Roster> loadRoster(
        int classId
        );

    [[nodiscard]] Result<int> getRosterStudentCount(
        int classId
        );

    // Roster score-import construction is delegated through
    // SpeakingEvalRepository to the engine
    // SpeakingEvaluationPersistenceService.



    // =====================================================
    // Speaking Evaluations
    // =====================================================

    [[nodiscard]] Status saveSpeakingEval(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    [[nodiscard]] Result<SpeakingEvalRows> loadSpeakingEval(
        int classId,
        const QString& evaluationName
        );

    [[nodiscard]] Result<QList<SpeakingEvalScore>> buildRosterScoreImport(
        int classId,
        const QString& evaluationName
        );



    // =====================================================
    // Campuses
    // =====================================================

    [[nodiscard]] Result<int> saveCampus(
        const CampusRecord &campus
        );

    [[nodiscard]] Result<CampusRecord> getCampus(
        int campusId
        );

    [[nodiscard]] Result<QList<CampusRecord>> getAllCampuses();

    [[nodiscard]] Status deleteCampus(
        int campusId
        );



    // =====================================================
    // Manual Saving
    // =====================================================

    // Compatibility no-op. Engine repository writes are transactional.
    void save();

    [[nodiscard]] Status saveAs(
        const QString &destinationPath
        );

    [[nodiscard]] Status exportAs(
        const QString &destinationPath
        );



private:

    void refreshRepositoryAdapters();

    QString m_initialDatabasePath;
    std::unique_ptr<DatabaseSession> m_ownedSession;
    DatabaseSession* m_session = nullptr;
    bool m_ownsSession = false;

    SettingsRepository* m_settingsRepository = nullptr;
    CampusRecordRepository* m_campusRecordRepository = nullptr;
    TeacherRepository* m_teacherRepository = nullptr;
    NativeEnglishTeacherRepository* m_nativeEnglishTeacherRepository = nullptr;
    GsTeamRepository* m_gsTeamRepository = nullptr;
    TeacherImportRepository* m_teacherImportRepository = nullptr;
    ClassRepository* m_classRepository = nullptr;
    ClassTransferRepository* m_classTransferRepository = nullptr;
    ScheduleImportRepository* m_scheduleImportRepository = nullptr;
    ClassInfoRepository* m_classInfoRepository = nullptr;
    IntensiveSlotStateRepository* m_intensiveSlotStateRepository = nullptr;
    TestingBlockRepository* m_testingBlockRepository = nullptr;
    TestingClassRepository* m_testingClassRepository = nullptr;
    CalendarEventRepository* m_calendarEventRepository = nullptr;
    RosterRepository* m_rosterRepository = nullptr;
    SpeakingEvalRepository* m_speakingEvalRepository = nullptr;
};
