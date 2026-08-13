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
#include <QSqlDatabase>
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

// Compatibility facade retained while callers migrate to the feature services
// exposed by ApplicationServices. New UI and controller code should depend on
// those narrow services instead of adding operations here.
class DataService
{
public:

    explicit DataService(
        const QString &dbPath = QString()
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

    // Transitional access for constructing the narrow application services.
    // UI and controllers must not use the session or its repositories directly.
    [[nodiscard]] DatabaseSession* databaseSession() const;

    // =====================================================
    // Settings
    // =====================================================

    void saveSetting(
        const QString &key,
        const QVariant &value
        );

    QVariant loadSetting(
        const QString &key,
        const QVariant &defaultValue = QVariant()
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

    QList<Teacher> getAllTeachers();

    Teacher getTeacher(
        int teacherId
        );

    [[nodiscard]] Status deleteTeacher(
        int teacherId
        );

    QList<NativeEnglishTeacher> getNativeEnglishTeachers();

    [[nodiscard]] Status saveNativeEnglishTeacherDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        );

    QList<GsTeamMember> getGsTeamMembers();

    [[nodiscard]] Status saveGsTeamDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

    [[nodiscard]] QDate latestTeacherImportDate();



    // =====================================================
    // Classes
    // =====================================================

    [[nodiscard]] Result<int> createClass(
        const QString& name
        );

    QList<Classroom> getClasses();

    Classroom getClassById(
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

    [[nodiscard]] Result<ScheduleImportSummary> importSchedule(
        const ScheduleImportPlan& plan
        );



    // =====================================================
    // Class Info
    // =====================================================

    [[nodiscard]] bool saveClassInfo(
        const ClassInfo& info
        );

    [[nodiscard]] bool saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );

    ClassInfo loadClassInfo(
        int classId
        );

    // TODO:
    // Port class time management
    // Port intensive time management



    // =====================================================
    // Intensive Slot States
    // =====================================================

    QList<IntensiveSlotState> loadIntensiveSlotStates();

    void saveIntensiveSlotState(
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

    QList<CalendarEvent> loadCalendarEventsForDate(
        const QDate& date
        );

    QList<CalendarEvent> loadCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    QList<CalendarEvent> loadUpcomingCalendarEvents(
        const QDate& fromDate,
        int limit
        );

    CalendarEvent getCalendarEvent(
        int eventId
        );

    QList<CalendarEvent> loadCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    int saveCalendarEvent(
        const CalendarEvent& event
        );

    void deleteCalendarEvent(
        int eventId
        );

    void deleteCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    void deleteAllCalendarEvents();



    // =====================================================
    // Conflict Detection
    // =====================================================

    QList<ClassConflict> getClassTimeConflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        );

    // TODO:
    // Port time parsing helpers



    // =====================================================
    // Roster
    // =====================================================

    void saveRoster(
        int classId,
        const Roster& roster
        );

    [[nodiscard]] bool saveRosters(
        const QList<QPair<int, Roster>>& rosters
        );

    Roster loadRoster(
        int classId
        );

    int getRosterStudentCount(
        int classId
        );

    // Port buildRosterScoreImport()



    // =====================================================
    // Speaking Evaluations
    // =====================================================

    [[nodiscard]] bool saveSpeakingEval(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    SpeakingEvalRows loadSpeakingEval(
        int classId,
        const QString& evaluationName
        );

    QList<SpeakingEvalScore> buildRosterScoreImport(
        int classId,
        const QString& evaluationName
        );



    // =====================================================
    // Campuses
    // =====================================================

    int saveCampus(
        const CampusRecord &campus
        );

    CampusRecord getCampus(
        int campusId
        );

    QList<CampusRecord> getAllCampuses();

    void deleteCampus(
        int campusId
        );



    // =====================================================
    // Manual Saving
    // =====================================================

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
    std::unique_ptr<DatabaseSession> m_session;

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
