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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QVariant>

DataService::DataService(
    const QString &dbPath
    )
    : m_initialDatabasePath(dbPath)
    , m_session(std::make_unique<DatabaseSession>())
{
}

DataService::~DataService()
{
    closeDatabase();
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
    return m_session.get();
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

void DataService::saveSetting(
    const QString &key,
    const QVariant &value
    )
{
    if (m_settingsRepository)
    {
        m_settingsRepository->saveSetting(
            key,
            value
            );
    }
}

QVariant DataService::loadSetting(
    const QString &key,
    const QVariant &defaultValue
    )
{
    if (!m_settingsRepository)
    {
        return defaultValue;
    }

    return m_settingsRepository->loadSetting(
        key,
        defaultValue
        );
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

Teacher DataService::getTeacher(
    int teacherId
    )
{
    if (!m_teacherRepository)
    {
        return Teacher();
    }

    return m_teacherRepository->getTeacher(
        teacherId
        );
}

QList<Teacher> DataService::getAllTeachers()
{
    if (!m_teacherRepository)
    {
        return {};
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

QList<NativeEnglishTeacher> DataService::getNativeEnglishTeachers()
{
    return m_nativeEnglishTeacherRepository
        ? m_nativeEnglishTeacherRepository->getAll()
        : QList<NativeEnglishTeacher>{};
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

QList<GsTeamMember> DataService::getGsTeamMembers()
{
    return m_gsTeamRepository
        ? m_gsTeamRepository->getAll()
        : QList<GsTeamMember>{};
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

QDate DataService::latestTeacherImportDate()
{
    const QString value = loadSetting(
        QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting),
        QString()
        ).toString();
    return QDate::fromString(value, Qt::ISODate);
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

QList<Classroom> DataService::getClasses()
{
    if (!m_classRepository)
    {
        return {};
    }

    return m_classRepository->getClasses();
}

Classroom DataService::getClassById(
    int classId
    )
{
    if (!m_classRepository)
    {
        return Classroom();
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

bool DataService::saveClassInfo(
    const ClassInfo& info
    )
{
    if (!m_classInfoRepository)
    {
        return false;
    }

    return m_classInfoRepository->saveClassInfo(
        info
        );
}

bool DataService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (!m_classInfoRepository)
    {
        return false;
    }

    return m_classInfoRepository->saveClassNotes(
        classId,
        notes,
        timeFillerActivities
        );
}

ClassInfo DataService::loadClassInfo(
    int classId
    )
{
    if (!m_classInfoRepository)
    {
        ClassInfo info;
        info.classId = classId;

        return info;
    }

    return m_classInfoRepository->loadClassInfo(
        classId
        );
}

QList<IntensiveSlotState> DataService::loadIntensiveSlotStates()
{
    if (!m_intensiveSlotStateRepository)
    {
        return {};
    }

    return m_intensiveSlotStateRepository->loadIntensiveSlotStates();
}

void DataService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    )
{
    if (m_intensiveSlotStateRepository)
    {
        m_intensiveSlotStateRepository->saveIntensiveSlotState(
            day,
            startTime,
            state,
            defaultState
            );
    }
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

QList<CalendarEvent> DataService::loadCalendarEventsForDate(
    const QDate& date
    )
{
    if (!m_calendarEventRepository)
    {
        return {};
    }

    return m_calendarEventRepository->loadCalendarEventsForDate(
        date
        );
}

QList<CalendarEvent> DataService::loadCalendarEventsInRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    if (!m_calendarEventRepository)
    {
        return {};
    }

    return m_calendarEventRepository->loadCalendarEventsInRange(
        startDate,
        endDate
        );
}

QList<CalendarEvent> DataService::loadUpcomingCalendarEvents(
    const QDate& fromDate,
    int limit
    )
{
    if (!m_calendarEventRepository)
    {
        return {};
    }

    return m_calendarEventRepository->loadUpcomingCalendarEvents(
        fromDate,
        limit
        );
}

CalendarEvent DataService::getCalendarEvent(
    int eventId
    )
{
    if (!m_calendarEventRepository)
    {
        return CalendarEvent();
    }

    return m_calendarEventRepository->getCalendarEvent(
        eventId
        );
}

QList<CalendarEvent> DataService::loadCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    if (!m_calendarEventRepository)
    {
        return {};
    }

    return m_calendarEventRepository->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        startDate
        );
}

int DataService::saveCalendarEvent(
    const CalendarEvent& event
    )
{
    if (!m_calendarEventRepository)
    {
        return -1;
    }

    return m_calendarEventRepository->saveCalendarEvent(
        event
        );
}

void DataService::deleteCalendarEvent(
    int eventId
    )
{
    if (m_calendarEventRepository)
    {
        m_calendarEventRepository->deleteCalendarEvent(
            eventId
            );
    }
}

void DataService::deleteCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    if (m_calendarEventRepository)
    {
        m_calendarEventRepository->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId,
            startDate
            );
    }
}

void DataService::deleteAllCalendarEvents()
{
    if (m_calendarEventRepository)
    {
        m_calendarEventRepository->deleteAllCalendarEvents();
    }
}

QList<ClassConflict> DataService::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    if (!m_classInfoRepository)
    {
        return {};
    }

    return m_classInfoRepository->getClassTimeConflicts(
        classId,
        times,
        type
        );
}

void DataService::saveRoster(
    int classId,
    const Roster& roster
    )
{
    if (m_rosterRepository)
    {
        m_rosterRepository->saveRoster(
            classId,
            roster
            );
    }
}

bool DataService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    if (!m_rosterRepository)
    {
        return false;
    }

    return m_rosterRepository->saveRosters(
        rosters
        );
}

Roster DataService::loadRoster(
    int classId
    )
{
    if (!m_rosterRepository)
    {
        return Roster();
    }

    return m_rosterRepository->loadRoster(
        classId
        );
}

int DataService::getRosterStudentCount(
    int classId
    )
{
    if (!m_rosterRepository)
    {
        return 0;
    }

    return m_rosterRepository->getRosterStudentCount(
        classId
        );
}

bool DataService::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    if (!m_speakingEvalRepository)
    {
        return false;
    }

    return m_speakingEvalRepository->saveSpeakingEval(
        classId,
        evaluationName,
        rows,
        dirtyCells
        );
}

SpeakingEvalRows DataService::loadSpeakingEval(
    int classId,
    const QString& evaluationName
    )
{
    if (!m_speakingEvalRepository)
    {
        return {};
    }

    return m_speakingEvalRepository->loadSpeakingEval(
        classId,
        evaluationName
        );
}

QList<SpeakingEvalScore> DataService::buildRosterScoreImport(
    int classId,
    const QString& evaluationName
    )
{
    if (!m_speakingEvalRepository)
    {
        return {};
    }

    return m_speakingEvalRepository->buildRosterScoreImport(
        classId,
        evaluationName
        );
}

int DataService::saveCampus(
    const CampusRecord &campus
    )
{
    if (!m_campusRecordRepository)
    {
        return 0;
    }

    return m_campusRecordRepository->saveCampus(
        campus
        );
}

CampusRecord DataService::getCampus(
    int campusId
    )
{
    if (!m_campusRecordRepository)
    {
        return CampusRecord();
    }

    return m_campusRecordRepository->getCampus(
        campusId
        );
}

QList<CampusRecord> DataService::getAllCampuses()
{
    if (!m_campusRecordRepository)
    {
        return {};
    }

    return m_campusRecordRepository->getAllCampuses();
}

void DataService::deleteCampus(
    int campusId
    )
{
    if (m_campusRecordRepository)
    {
        m_campusRecordRepository->deleteCampus(
            campusId
            );
    }
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

    const QString sourcePath =
        QFileInfo(m_session->databasePath()).absoluteFilePath();

    const QFileInfo targetInfo(destinationPath);
    const QString targetPath =
        targetInfo.absoluteFilePath();

    if (sourcePath == targetPath)
    {
        return {};
    }

    if (
        !targetInfo.absolutePath().isEmpty()
        && !QDir().mkpath(targetInfo.absolutePath())
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to create destination directory:\n%1")
                .arg(targetInfo.absolutePath())
            );
    }

    if (
        QFile::exists(targetPath)
        && !QFile::remove(targetPath)
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to replace existing Teacher Profile file:\n%1")
                .arg(targetPath)
            );
    }

    if (!QFile::copy(sourcePath, targetPath))
    {
        return std::unexpected(
            QStringLiteral("Unable to copy Teacher Profile to:\n%1")
                .arg(targetPath)
            );
    }

    return {};
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

    const QString sourcePath =
        QFileInfo(m_session->databasePath()).absoluteFilePath();

    const QFileInfo targetInfo(destinationPath);
    const QString targetPath =
        targetInfo.absoluteFilePath();

    if (sourcePath == targetPath)
    {
        return {};
    }

    if (
        !targetInfo.absolutePath().isEmpty()
        && !QDir().mkpath(targetInfo.absolutePath())
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to create destination directory:\n%1")
                .arg(targetInfo.absolutePath())
            );
    }

    if (
        QFile::exists(targetPath)
        && !QFile::remove(targetPath)
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to replace existing Teacher Profile file:\n%1")
                .arg(targetPath)
            );
    }

    if (!QFile::copy(sourcePath, targetPath))
    {
        return std::unexpected(
            QStringLiteral("Unable to copy Teacher Profile to:\n%1")
                .arg(targetPath)
            );
    }

    return {};
}
