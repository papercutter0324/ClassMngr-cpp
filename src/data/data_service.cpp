#include "data_service.h"

#include "data/database/database_file_operations.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/campus_record_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/schedule_import_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"

#include <QObject>
#include <QVariant>

DataService::DataService(
    const QString &dbPath
    )
    : m_initialDatabasePath(dbPath)
    , m_ownedSession(std::make_unique<DatabaseSession>())
    , m_session(m_ownedSession.get())
{
}

DataService::DataService(DatabaseSession& session)
    : m_session(&session)
{
}

DataService::~DataService()
{
    if (m_ownedSession)
    {
        closeDatabase();
    }
}

bool DataService::open()
{
    if (m_initialDatabasePath.trimmed().isEmpty())
    {
        return false;
    }

    return openDatabase(m_initialDatabasePath).has_value();
}

Status DataService::openDatabase(
    const QString& dbPath
    )
{
    const Status status = m_session->open(dbPath);
    return status;
}

void DataService::closeDatabase()
{
    m_session->close();
}

bool DataService::isOpen() const
{
    return m_session->isOpen();
}

QString DataService::currentDatabasePath() const
{
    return m_session->databasePath();
}

DatabaseSession* DataService::databaseSession() const
{
    return m_session;
}

Status DataService::saveSetting(
    const QString &key,
    const QVariant &value
    )
{
    if (!m_session->settingsRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->settingsRepository()->saveSetting(key, value);
}

Status DataService::saveSettings(
    const QVariantMap& values
    )
{
    if (!m_session->settingsRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->settingsRepository()->saveSettings(values);
}

Result<QVariant> DataService::loadSetting(
    const QString &key
    )
{
    if (!m_session->settingsRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->settingsRepository()->loadSetting(key);
}

Result<int> DataService::createTeacher(
    const Teacher& teacher
    )
{
    if (!m_session->teacherRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->teacherRepository()->createTeacher(
        teacher
        );
}

Result<int> DataService::saveTeacher(
    const Teacher& teacher
    )
{
    if (!m_session->teacherRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->teacherRepository()->saveTeacher(
        teacher
        );
}

Status DataService::updateTeacher(
    const Teacher& teacher
    )
{
    if (!m_session->teacherRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->teacherRepository()->updateTeacher(teacher);
}

Result<Teacher> DataService::getTeacher(
    int teacherId
    )
{
    if (!m_session->teacherRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->teacherRepository()->getTeacher(
        teacherId
        );
}

Result<QList<Teacher>> DataService::getAllTeachers()
{
    if (!m_session->teacherRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->teacherRepository()->getAllTeachers();
}

Status DataService::deleteTeacher(
    int teacherId
    )
{
    if (!m_session->teacherRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->teacherRepository()->deleteTeacher(teacherId);
}

Result<QList<NativeEnglishTeacher>> DataService::getNativeEnglishTeachers()
{
    if (!m_session->nativeEnglishTeacherRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->nativeEnglishTeacherRepository()->getAll();
}

Status DataService::saveNativeEnglishTeacherDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    )
{
    if (!m_session->nativeEnglishTeacherRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }
    return m_session->nativeEnglishTeacherRepository()->saveDirectory(teachers, deletedIds);
}

Result<QList<GsTeamMember>> DataService::getGsTeamMembers()
{
    if (!m_session->gsTeamRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->gsTeamRepository()->getAll();
}

Status DataService::saveGsTeamDirectory(
    const QList<GsTeamMember>& members,
    const QList<int>& deletedIds
    )
{
    if (!m_session->gsTeamRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }
    return m_session->gsTeamRepository()->saveDirectory(members, deletedIds);
}

Result<TeacherImportSummary> DataService::importTeachers(
    const TeacherImportPlan& plan
    )
{
    if (!m_session->teacherImportRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }
    return m_session->teacherImportRepository()->importTeachers(plan);
}

Result<QDate> DataService::latestTeacherImportDate()
{
    const Result<QVariant> value = loadSetting(
        QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting)
        );
    if (!value)
    {
        return std::unexpected(value.error());
    }

    return QDate::fromString(value->toString(), Qt::ISODate);
}

Result<int> DataService::createClass(
    const QString &name
    )
{
    if (!m_session->classRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->classRepository()->createClass(
        name
        );
}

Result<QList<Classroom>> DataService::getClasses()
{
    if (!m_session->classRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->classRepository()->getClasses();
}

Result<Classroom> DataService::getClassById(
    int classId
    )
{
    if (!m_session->classRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->classRepository()->getClassById(
        classId
        );
}

Status DataService::updateClassName(
    int classId,
    const QString &name
    )
{
    if (!m_session->classRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->classRepository()->updateClassName(classId, name);
}

Status DataService::deleteClass(
    int classId
    )
{
    if (!m_session->classRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->classRepository()->deleteClass(classId);
}

Result<ClassTransferPackage> DataService::buildClassTransferPackage(
    const QList<int>& classIds
    )
{
    if (!m_session->classTransferRepository())
    {
        return std::unexpected(
            QStringLiteral("Class transfer is unavailable.")
            );
    }

    return m_session->classTransferRepository()->buildPackage(classIds);
}

Result<ClassImportPreview> DataService::previewClassImport(
    const ClassTransferPackage& package
    )
{
    if (!m_session->classTransferRepository())
    {
        return std::unexpected(
            QStringLiteral("Class transfer is unavailable.")
            );
    }

    return m_session->classTransferRepository()->previewImport(package);
}

Result<ClassImportSummary> DataService::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    if (!m_session->classTransferRepository())
    {
        return std::unexpected(
            QStringLiteral("Class transfer is unavailable.")
            );
    }

    return m_session->classTransferRepository()->importClasses(package, plan);
}

Result<ScheduleImportPreview> DataService::previewScheduleImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    if (!m_session->scheduleImportRepository())
    {
        return std::unexpected(
            QObject::tr("Schedule import is unavailable.")
            );
    }

    return m_session->scheduleImportRepository()->preview(
        user,
        kind
        );
}

Result<ScheduleImportSummary> DataService::importSchedule(
    const ScheduleImportPlan& plan
    )
{
    if (!m_session->scheduleImportRepository())
    {
        return std::unexpected(
            QObject::tr("Schedule import is unavailable.")
            );
    }

    return m_session->scheduleImportRepository()->apply(plan);
}

Status DataService::saveClassInfo(
    const ClassInfo& info
    )
{
    if (!m_session->classInfoRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->classInfoRepository()->saveClassInfo(
        info
        );
}

Status DataService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (!m_session->classInfoRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->classInfoRepository()->saveClassNotes(
        classId,
        notes,
        timeFillerActivities
        );
}

Result<ClassInfo> DataService::loadClassInfo(
    int classId
    )
{
    if (!m_session->classInfoRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->classInfoRepository()->loadClassInfo(
        classId
        );
}

Result<QList<IntensiveSlotState>> DataService::loadIntensiveSlotStates()
{
    if (!m_session->intensiveSlotStateRepository())
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_session->intensiveSlotStateRepository()->loadIntensiveSlotStates();
}

Status DataService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    )
{
    if (!m_session->intensiveSlotStateRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->intensiveSlotStateRepository()->saveIntensiveSlotState(
        day,
        startTime,
        state,
        defaultState
        );
}

Result<QList<TestingBlock>> DataService::loadTestingBlocks()
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->loadTestingBlocks();
}

Result<QList<TestingAssignment>>
DataService::loadTestingAssignments()
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->loadTestingAssignments();
}

Status DataService::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool replaceExisting
    )
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->saveTestingBlock(
        day,
        startTime,
        room,
        replaceExisting
        );
}

Status DataService::assignTestingClass(
    const QString& day,
    const QString& startTime,
    int classId,
    bool replaceExisting
    )
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->assignTestingClass(
        day,
        startTime,
        classId,
        replaceExisting
        );
}

Status DataService::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    )
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->deleteTestingAssignment(
        day,
        startTime
        );
}

Status DataService::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    )
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->deleteTestingBlock(
        day,
        startTime
        );
}

Status DataService::clearTestingBlocks()
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->clearTestingBlocks();
}

Status DataService::clearTestingAssignments()
{
    if (!m_session->testingBlockRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingBlockRepository()->clearTestingAssignments();
}

Result<int> DataService::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    )
{
    if (!m_session->testingClassRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingClassRepository()->createTestingClass(
        testingClass,
        assignmentDay,
        assignmentStartTime
        );
}

Status DataService::updateTestingClass(
    const TestingClass& testingClass
    )
{
    if (!m_session->testingClassRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingClassRepository()->updateTestingClass(testingClass);
}

Result<TestingClass> DataService::loadTestingClass(
    int classId
    )
{
    if (!m_session->testingClassRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingClassRepository()->loadTestingClass(classId);
}

Result<QList<TestingClass>> DataService::loadTestingClasses()
{
    if (!m_session->testingClassRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingClassRepository()->loadTestingClasses();
}

Status DataService::deleteTestingClass(
    int classId
    )
{
    if (!m_session->testingClassRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingClassRepository()->deleteTestingClass(classId);
}

Result<bool> DataService::isTestingClass(
    int classId
    )
{
    if (!m_session->testingClassRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->testingClassRepository()->isTestingClass(classId);
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsForDate(
    const QDate& date
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->loadCalendarEventsForDate(
        date
        );
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsInRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->loadCalendarEventsInRange(
        startDate,
        endDate
        );
}

Result<QList<CalendarEventDateInterval>>
DataService::loadCalendarEventDateIntervalsInRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->loadCalendarEventDateIntervalsInRange(
        startDate,
        endDate
        );
}

Result<QList<CalendarEvent>> DataService::loadUpcomingCalendarEvents(
    const QDate& fromDate,
    int limit
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->loadUpcomingCalendarEvents(
        fromDate,
        limit
        );
}

Result<CalendarEvent> DataService::getCalendarEvent(
    int eventId
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->getCalendarEvent(
        eventId
        );
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        startDate
        );
}

Result<int> DataService::saveCalendarEvent(
    const CalendarEvent& event
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->saveCalendarEvent(
        event
        );
}

Result<QList<int>> DataService::saveCalendarEvents(
    const QList<CalendarEvent>& events
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->saveCalendarEvents(events);
}

Status DataService::deleteCalendarEvent(
    int eventId
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->deleteCalendarEvent(eventId);
}

Status DataService::deleteCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()
        ->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId,
            startDate
            );
}

Status DataService::deleteAllCalendarEvents()
{
    if (!m_session->calendarEventRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->calendarEventRepository()->deleteAllCalendarEvents();
}

Result<QList<ClassConflict>> DataService::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    if (!m_session->classInfoRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->classInfoRepository()->getClassTimeConflicts(
        classId,
        times,
        type
        );
}

Status DataService::saveRoster(
    int classId,
    const Roster& roster
    )
{
    if (!m_session->rosterRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->rosterRepository()->saveRoster(classId, roster);
}

Status DataService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    if (!m_session->rosterRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->rosterRepository()->saveRosters(
        rosters
        );
}

Result<Roster> DataService::loadRoster(
    int classId
    )
{
    if (!m_session->rosterRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->rosterRepository()->loadRoster(
        classId
        );
}

Result<Roster> DataService::loadRosterForOutput(
    const int classId,
    const QStringList& requestedColumns,
    const std::size_t maxRows,
    const std::size_t maxCells,
    const std::size_t maxTextBytes
    )
{
    if (!m_session->rosterRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->rosterRepository()->loadRosterForOutput(
        classId,
        requestedColumns,
        maxRows,
        maxCells,
        maxTextBytes
        );
}

Result<int> DataService::getRosterStudentCount(
    int classId
    )
{
    if (!m_session->rosterRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->rosterRepository()->getRosterStudentCount(
        classId
        );
}

Status DataService::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    if (!m_session->speakingEvalRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->speakingEvalRepository()->saveSpeakingEval(
        classId,
        evaluationName,
        rows,
        dirtyCells
        );
}

Result<SpeakingEvalRows> DataService::loadSpeakingEval(
    int classId,
    const QString& evaluationName
    )
{
    if (!m_session->speakingEvalRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->speakingEvalRepository()->loadSpeakingEval(
        classId,
        evaluationName
        );
}

Result<QList<SpeakingEvalScore>> DataService::buildRosterScoreImport(
    int classId,
    const QString& evaluationName
    )
{
    if (!m_session->speakingEvalRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->speakingEvalRepository()->buildRosterScoreImport(
        classId,
        evaluationName
        );
}

Result<int> DataService::saveCampus(
    const CampusRecord &campus
    )
{
    if (!m_session->campusRecordRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->campusRecordRepository()->saveCampus(
        campus
        );
}

Result<CampusRecord> DataService::getCampus(
    int campusId
    )
{
    if (!m_session->campusRecordRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->campusRecordRepository()->getCampus(
        campusId
        );
}

Result<QList<CampusRecord>> DataService::getAllCampuses()
{
    if (!m_session->campusRecordRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->campusRecordRepository()->getAllCampuses();
}

Status DataService::deleteCampus(
    int campusId
    )
{
    if (!m_session->campusRecordRepository())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_session->campusRecordRepository()->deleteCampus(campusId);
}

void DataService::save()
{
    if (!isOpen())
    {
        return;
    }

    m_session->database().commit();
}

Status DataService::saveAs(
    const QString &destinationPath
    )
{
    if (!isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    return DatabaseFileOperations::copyDatabaseFile(
        m_session->databasePath(),
        destinationPath
        );
}

Status DataService::exportAs(
    const QString &destinationPath
    )
{
    if (!isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    return DatabaseFileOperations::copyDatabaseFile(
        m_session->databasePath(),
        destinationPath
        );
}
