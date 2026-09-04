#include "data_service.h"

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

#include "classmngr/engine/file_system.h"

#include <QByteArray>
#include <QObject>
#include <QVariant>

#include <cstddef>
#include <string>
#include <string_view>

namespace
{

std::string utf8Path(
    const QString& path
    )
{
    const QByteArray encoded = path.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

QString displayPath(
    const std::string& normalizedPath
    )
{
    return QString::fromUtf8(
        normalizedPath.data(),
        static_cast<qsizetype>(normalizedPath.size())
        );
}

Status copyDatabaseFile(
    const QString& sourcePath,
    const QString& destinationPath
    )
{
    classmngr::engine::StandardFileSystem fileSystem;
    const std::string sourceUtf8 = utf8Path(sourcePath);
    const std::string destinationUtf8 = utf8Path(destinationPath);

    const classmngr::engine::Result<std::string> normalizedDestination =
        fileSystem.normalizePath(destinationUtf8);
    const QString targetPath = normalizedDestination
        ? displayPath(*normalizedDestination)
        : destinationPath;

    const classmngr::engine::Result<std::string> normalizedSource =
        fileSystem.normalizePath(sourceUtf8);
    if (!normalizedSource || !normalizedDestination)
    {
        return std::unexpected(
            QStringLiteral("Unable to copy Teacher Profile to:\n%1")
                .arg(targetPath)
            );
    }

    if (*normalizedSource == *normalizedDestination)
    {
        return {};
    }

    const classmngr::engine::Status copied = fileSystem.copyFile(
        *normalizedSource,
        *normalizedDestination,
        true
        );
    if (copied)
    {
        return {};
    }

    const std::string_view errorToken = copied.error().message;
    if (errorToken
        == classmngr::engine::FileSystemErrorToken::DirectoryCreationFailed)
    {
        return std::unexpected(
            QStringLiteral("Unable to create destination directory:\n%1")
                .arg(targetPath)
            );
    }

    if (errorToken
        == classmngr::engine::FileSystemErrorToken::AtomicReplacementFailed)
    {
        return std::unexpected(
            QStringLiteral(
                "Unable to replace existing Teacher Profile file:\n%1"
                )
                .arg(targetPath)
            );
    }

    return std::unexpected(
        QStringLiteral("Unable to copy Teacher Profile to:\n%1")
            .arg(targetPath)
        );
}

} // namespace

DataService::DataService(
    const QString &dbPath
    )
    : m_initialDatabasePath(dbPath)
    , m_ownedSession(std::make_unique<DatabaseSession>())
    , m_session(m_ownedSession.get())
    , m_ownsSession(true)
{
}

DataService::DataService(
    DatabaseSession& session
    )
    : m_session(&session)
{
    refreshRepositoryAdapters();
}

DataService::~DataService()
{
    if (m_ownsSession)
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
    refreshRepositoryAdapters();
    return status;
}

void DataService::closeDatabase()
{
    m_session->close();
    refreshRepositoryAdapters();
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

void DataService::synchronizeCompatibilityAdapters()
{
    refreshRepositoryAdapters();
}

void DataService::refreshRepositoryAdapters()
{
    m_settingsRepository = m_session->settingsRepository();
    m_campusRecordRepository = m_session->campusRecordRepository();
    m_teacherRepository = m_session->teacherRepository();
    m_nativeEnglishTeacherRepository = m_session->nativeEnglishTeacherRepository();
    m_gsTeamRepository = m_session->gsTeamRepository();
    m_teacherImportRepository = m_session->teacherImportRepository();
    m_classRepository = m_session->classRepository();
    m_classTransferRepository = m_session->classTransferRepository();
    m_scheduleImportRepository = m_session->scheduleImportRepository();
    m_classInfoRepository = m_session->classInfoRepository();
    m_intensiveSlotStateRepository = m_session->intensiveSlotStateRepository();
    m_testingBlockRepository = m_session->testingBlockRepository();
    m_testingClassRepository = m_session->testingClassRepository();
    m_calendarEventRepository = m_session->calendarEventRepository();
    m_rosterRepository = m_session->rosterRepository();
    m_speakingEvalRepository = m_session->speakingEvalRepository();
}

Status DataService::saveSetting(
    const QString &key,
    const QVariant &value
    )
{
    if (!m_settingsRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_settingsRepository->saveSetting(key, value);
}

Status DataService::saveSettings(
    const QVariantMap& values
    )
{
    if (!m_settingsRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_settingsRepository->saveSettings(values);
}

Result<QVariant> DataService::loadSetting(
    const QString &key
    )
{
    if (!m_settingsRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_settingsRepository->loadSetting(key);
}

Result<int> DataService::createTeacher(
    const Teacher& teacher
    )
{
    if (!m_teacherRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_teacherRepository->createTeacher(
        teacher
        );
}

Result<int> DataService::saveTeacher(
    const Teacher& teacher
    )
{
    if (!m_teacherRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_teacherRepository->saveTeacher(
        teacher
        );
}

Status DataService::updateTeacher(
    const Teacher& teacher
    )
{
    if (!m_teacherRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_teacherRepository->updateTeacher(teacher);
}

Result<Teacher> DataService::getTeacher(
    int teacherId
    )
{
    if (!m_teacherRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_teacherRepository->getTeacher(
        teacherId
        );
}

Result<QList<Teacher>> DataService::getAllTeachers()
{
    if (!m_teacherRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_teacherRepository->getAllTeachers();
}

Status DataService::deleteTeacher(
    int teacherId
    )
{
    if (!m_teacherRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_teacherRepository->deleteTeacher(teacherId);
}

Result<QList<NativeEnglishTeacher>> DataService::getNativeEnglishTeachers()
{
    if (!m_nativeEnglishTeacherRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_nativeEnglishTeacherRepository->getAll();
}

Status DataService::saveNativeEnglishTeacherDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    )
{
    if (!m_nativeEnglishTeacherRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }
    return m_nativeEnglishTeacherRepository->saveDirectory(teachers, deletedIds);
}

Result<QList<GsTeamMember>> DataService::getGsTeamMembers()
{
    if (!m_gsTeamRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_gsTeamRepository->getAll();
}

Status DataService::saveGsTeamDirectory(
    const QList<GsTeamMember>& members,
    const QList<int>& deletedIds
    )
{
    if (!m_gsTeamRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }
    return m_gsTeamRepository->saveDirectory(members, deletedIds);
}

Result<TeacherImportSummary> DataService::importTeachers(
    const TeacherImportPlan& plan
    )
{
    if (!m_teacherImportRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }
    return m_teacherImportRepository->importTeachers(plan);
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
    if (!m_classRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_classRepository->createClass(
        name
        );
}

Result<QList<Classroom>> DataService::getClasses()
{
    if (!m_classRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_classRepository->getClasses();
}

Result<Classroom> DataService::getClassById(
    int classId
    )
{
    if (!m_classRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_classRepository->getClassById(
        classId
        );
}

Status DataService::updateClassName(
    int classId,
    const QString &name
    )
{
    if (!m_classRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_classRepository->updateClassName(classId, name);
}

Status DataService::deleteClass(
    int classId
    )
{
    if (!m_classRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_classRepository->deleteClass(classId);
}

Result<ClassTransferPackage> DataService::buildClassTransferPackage(
    const QList<int>& classIds
    )
{
    if (!m_classTransferRepository)
    {
        return std::unexpected(
            QStringLiteral("Class transfer is unavailable.")
            );
    }

    return m_classTransferRepository->buildPackage(classIds);
}

Result<ClassImportPreview> DataService::previewClassImport(
    const ClassTransferPackage& package
    )
{
    if (!m_classTransferRepository)
    {
        return std::unexpected(
            QStringLiteral("Class transfer is unavailable.")
            );
    }

    return m_classTransferRepository->previewImport(package);
}

Result<ClassImportSummary> DataService::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    if (!m_classTransferRepository)
    {
        return std::unexpected(
            QStringLiteral("Class transfer is unavailable.")
            );
    }

    return m_classTransferRepository->importClasses(package, plan);
}

Result<ScheduleImportPreview> DataService::previewScheduleImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    if (!m_scheduleImportRepository)
    {
        return std::unexpected(
            QObject::tr("Schedule import is unavailable.")
            );
    }

    return m_scheduleImportRepository->preview(
        user,
        kind
        );
}

Status DataService::validateScheduleImport(
    const ScheduleImportPlan& plan
    )
{
    if (!m_scheduleImportRepository)
    {
        return std::unexpected(
            QObject::tr("Schedule import is unavailable.")
            );
    }

    return m_scheduleImportRepository->validateImport(plan);
}

Result<ScheduleImportSummary> DataService::importSchedule(
    const ScheduleImportPlan& plan
    )
{
    if (!m_scheduleImportRepository)
    {
        return std::unexpected(
            QObject::tr("Schedule import is unavailable.")
            );
    }

    return m_scheduleImportRepository->apply(plan);
}

Status DataService::saveClassInfo(
    const ClassInfo& info
    )
{
    if (!m_classInfoRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_classInfoRepository->saveClassInfo(
        info
        );
}

Status DataService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (!m_classInfoRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_classInfoRepository->saveClassNotes(
        classId,
        notes,
        timeFillerActivities
        );
}

Result<ClassInfo> DataService::loadClassInfo(
    int classId
    )
{
    if (!m_classInfoRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_classInfoRepository->loadClassInfo(
        classId
        );
}

Result<QList<IntensiveSlotState>> DataService::loadIntensiveSlotStates()
{
    if (!m_intensiveSlotStateRepository)
    {
        return std::unexpected(QStringLiteral("No Teacher Profile is open."));
    }

    return m_intensiveSlotStateRepository->loadIntensiveSlotStates();
}

Status DataService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    )
{
    if (!m_intensiveSlotStateRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_intensiveSlotStateRepository->saveIntensiveSlotState(
        day,
        startTime,
        state,
        defaultState
        );
}

Result<QList<TestingBlock>> DataService::loadTestingBlocks()
{
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->loadTestingBlocks();
}

Result<QList<TestingAssignment>>
DataService::loadTestingAssignments()
{
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->loadTestingAssignments();
}

Status DataService::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool replaceExisting
    )
{
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->saveTestingBlock(
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
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->assignTestingClass(
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
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->deleteTestingAssignment(
        day,
        startTime
        );
}

Status DataService::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    )
{
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->deleteTestingBlock(
        day,
        startTime
        );
}

Status DataService::clearTestingBlocks()
{
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->clearTestingBlocks();
}

Status DataService::clearTestingAssignments()
{
    if (!m_testingBlockRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingBlockRepository->clearTestingAssignments();
}

Result<int> DataService::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    )
{
    if (!m_testingClassRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingClassRepository->createTestingClass(
        testingClass,
        assignmentDay,
        assignmentStartTime
        );
}

Status DataService::updateTestingClass(
    const TestingClass& testingClass
    )
{
    if (!m_testingClassRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingClassRepository->updateTestingClass(testingClass);
}

Result<TestingClass> DataService::loadTestingClass(
    int classId
    )
{
    if (!m_testingClassRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingClassRepository->loadTestingClass(classId);
}

Result<QList<TestingClass>> DataService::loadTestingClasses()
{
    if (!m_testingClassRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingClassRepository->loadTestingClasses();
}

Status DataService::deleteTestingClass(
    int classId
    )
{
    if (!m_testingClassRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingClassRepository->deleteTestingClass(classId);
}

Result<bool> DataService::isTestingClass(
    int classId
    )
{
    if (!m_testingClassRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_testingClassRepository->isTestingClass(classId);
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsForDate(
    const QDate& date
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->loadCalendarEventsForDate(
        date
        );
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsInRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->loadCalendarEventsInRange(
        startDate,
        endDate
        );
}

Result<QList<CalendarEvent>> DataService::loadUpcomingCalendarEvents(
    const QDate& fromDate,
    int limit
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->loadUpcomingCalendarEvents(
        fromDate,
        limit
        );
}

Result<CalendarEvent> DataService::getCalendarEvent(
    int eventId
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->getCalendarEvent(
        eventId
        );
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        startDate
        );
}

Result<int> DataService::saveCalendarEvent(
    const CalendarEvent& event
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->saveCalendarEvent(
        event
        );
}

Result<QList<int>> DataService::saveCalendarEvents(
    const QList<CalendarEvent>& events
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->saveCalendarEvents(events);
}

Result<CalendarEventImportSummary> DataService::importCalendarEvents(
    const QList<CalendarEvent>& events,
    int parserSkippedCount
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->importCalendarEvents(
        events,
        parserSkippedCount
        );
}

Status DataService::deleteCalendarEvent(
    int eventId
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->deleteCalendarEvent(eventId);
}

Status DataService::deleteCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository
        ->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId,
            startDate
            );
}

Status DataService::deleteAllCalendarEvents()
{
    if (!m_calendarEventRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_calendarEventRepository->deleteAllCalendarEvents();
}

Result<QList<ClassConflict>> DataService::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    if (!m_classInfoRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_classInfoRepository->getClassTimeConflicts(
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
    if (!m_rosterRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_rosterRepository->saveRoster(classId, roster);
}

Status DataService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    if (!m_rosterRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_rosterRepository->saveRosters(
        rosters
        );
}

Result<Roster> DataService::loadRoster(
    int classId
    )
{
    if (!m_rosterRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_rosterRepository->loadRoster(
        classId
        );
}

Result<int> DataService::getRosterStudentCount(
    int classId
    )
{
    if (!m_rosterRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_rosterRepository->getRosterStudentCount(
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
    if (!m_speakingEvalRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_speakingEvalRepository->saveSpeakingEval(
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
    if (!m_speakingEvalRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_speakingEvalRepository->loadSpeakingEval(
        classId,
        evaluationName
        );
}

Result<QList<SpeakingEvalScore>> DataService::buildRosterScoreImport(
    int classId,
    const QString& evaluationName
    )
{
    if (!m_speakingEvalRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_speakingEvalRepository->buildRosterScoreImport(
        classId,
        evaluationName
        );
}

Result<int> DataService::saveCampus(
    const CampusRecord &campus
    )
{
    if (!m_campusRecordRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_campusRecordRepository->saveCampus(
        campus
        );
}

Result<CampusRecord> DataService::getCampus(
    int campusId
    )
{
    if (!m_campusRecordRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_campusRecordRepository->getCampus(
        campusId
        );
}

Result<QList<CampusRecord>> DataService::getAllCampuses()
{
    if (!m_campusRecordRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_campusRecordRepository->getAllCampuses();
}

Status DataService::deleteCampus(
    int campusId
    )
{
    if (!m_campusRecordRepository)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return m_campusRecordRepository->deleteCampus(campusId);
}

void DataService::save()
{
    // Compatibility no-op: engine repository writes commit their own
    // transactions. Legacy callers can retire explicit save calls.
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

    return copyDatabaseFile(
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

    return copyDatabaseFile(
        m_session->databasePath(),
        destinationPath
        );
}
