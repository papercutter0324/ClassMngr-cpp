#pragma once

#include "core/enums/schedule_type.h"
#include "domain/models/calendar_event.h"
#include "domain/models/campus.h"
#include "domain/models/class_info.h"
#include "domain/models/class_conflict.h"
#include "domain/models/classroom.h"
#include "domain/models/intensive_slot_state.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/teacher.h"
#include "core/result.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

#include <memory>

class CampusRecordRepository;
class CalendarEventRepository;
class ClassInfoRepository;
class ClassRepository;
class IntensiveSlotStateRepository;
class RosterRepository;
class SettingsRepository;
class SpeakingEvalRepository;
class TeacherRepository;

// =========================================================
// Data Service
// =========================================================

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

    void createTables();



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

    int createTeacher(
        const Teacher& teacher
        );

    int saveTeacher(
        const Teacher& teacher
        );

    void updateTeacher(
        const Teacher& teacher
        );

    QList<Teacher> getAllTeachers();

    Teacher getTeacher(
        int teacherId
        );

    void deleteTeacher(
        int teacherId
        );



    // =====================================================
    // Classes
    // =====================================================

    int createClass(
        const QString& name
        );

    QList<Classroom> getClasses();

    Classroom getClassById(
        int classId
        );

    void updateClassName(
        int classId,
        const QString& name
        );

    void deleteClass(
        int classId
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

    QString m_dbPath;

    QSqlDatabase m_db;

    std::unique_ptr<SettingsRepository> m_settingsRepository;
    std::unique_ptr<CampusRecordRepository> m_campusRecordRepository;
    std::unique_ptr<TeacherRepository> m_teacherRepository;
    std::unique_ptr<ClassRepository> m_classRepository;
    std::unique_ptr<ClassInfoRepository> m_classInfoRepository;
    std::unique_ptr<IntensiveSlotStateRepository> m_intensiveSlotStateRepository;
    std::unique_ptr<CalendarEventRepository> m_calendarEventRepository;
    std::unique_ptr<RosterRepository> m_rosterRepository;
    std::unique_ptr<SpeakingEvalRepository> m_speakingEvalRepository;
};
