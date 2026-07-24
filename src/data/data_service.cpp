#include "data_service.h"

#include "data/database/database_schema_manager.h"
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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QVariant>

DataService::DataService(
    const QString &dbPath
    )
    : m_dbPath(dbPath)
{
}

DataService::~DataService()
{
    closeDatabase();
}

bool DataService::open()
{
    if (m_dbPath.trimmed().isEmpty())
    {
        return false;
    }

    return openDatabase(m_dbPath).has_value();
}

Status DataService::openDatabase(
    const QString& dbPath
    )
{
    if (dbPath.trimmed().isEmpty())
    {
        closeDatabase();
        return std::unexpected(
            QStringLiteral("No database path was provided.")
            );
    }

    const QFileInfo databaseInfo(dbPath);
    const QString normalizedPath =
        databaseInfo.absoluteFilePath();

    closeDatabase();

    if (normalizedPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Database path could not be resolved.")
            );
    }

    if (
        !databaseInfo.absolutePath().isEmpty()
        && !QDir().mkpath(databaseInfo.absolutePath())
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to create database directory:\n%1")
                .arg(databaseInfo.absolutePath())
            );
    }

    m_db =
        QSqlDatabase::addDatabase("QSQLITE");

    m_db.setDatabaseName(normalizedPath);

    if (!m_db.open())
    {
        const QString openError =
            m_db.lastError().text();

        const QString connectionName =
            m_db.connectionName();

        m_db =
            QSqlDatabase();

        if (
            !connectionName.isEmpty()
            && QSqlDatabase::contains(connectionName)
            )
        {
            QSqlDatabase::removeDatabase(connectionName);
        }

        return std::unexpected(
            QStringLiteral("Unable to open database:\n%1\n\n%2")
                .arg(normalizedPath, openError)
            );
    }

    m_dbPath =
        normalizedPath;

    m_settingsRepository =
        std::make_unique<SettingsRepository>(
            m_db
            );

    m_campusRecordRepository =
        std::make_unique<CampusRecordRepository>(
            m_db
            );

    m_teacherRepository =
        std::make_unique<TeacherRepository>(
            m_db
            );

    m_nativeEnglishTeacherRepository =
        std::make_unique<NativeEnglishTeacherRepository>(m_db);

    m_gsTeamRepository =
        std::make_unique<GsTeamRepository>(m_db);

    m_teacherImportRepository =
        std::make_unique<TeacherImportRepository>(m_db);

    m_classRepository =
        std::make_unique<ClassRepository>(
            m_db
            );

    m_classTransferRepository =
        std::make_unique<ClassTransferRepository>(
            m_db
            );

    m_scheduleImportRepository =
        std::make_unique<ScheduleImportRepository>(
            m_db
            );

    m_classInfoRepository =
        std::make_unique<ClassInfoRepository>(
            m_db
            );

    m_intensiveSlotStateRepository =
        std::make_unique<IntensiveSlotStateRepository>(
            m_db
            );

    m_calendarEventRepository =
        std::make_unique<CalendarEventRepository>(
            m_db
            );

    m_rosterRepository =
        std::make_unique<RosterRepository>(
            m_db
            );

    m_speakingEvalRepository =
        std::make_unique<SpeakingEvalRepository>(
            m_db
            );

    DatabaseSchemaManager::ensureSchema(m_db);

    return {};
}

void DataService::closeDatabase()
{
    m_teacherImportRepository.reset();
    m_gsTeamRepository.reset();
    m_nativeEnglishTeacherRepository.reset();
    m_settingsRepository.reset();
    m_campusRecordRepository.reset();
    m_teacherRepository.reset();
    m_classRepository.reset();
    m_classTransferRepository.reset();
    m_scheduleImportRepository.reset();
    m_classInfoRepository.reset();
    m_intensiveSlotStateRepository.reset();
    m_calendarEventRepository.reset();
    m_rosterRepository.reset();
    m_speakingEvalRepository.reset();

    if (!m_db.isValid())
    {
        m_dbPath.clear();
        return;
    }

    const QString connectionName =
        m_db.connectionName();

    if (m_db.isOpen())
    {
        m_db.close();
    }

    m_db =
        QSqlDatabase();

    if (
        !connectionName.isEmpty()
        && QSqlDatabase::contains(connectionName)
        )
    {
        QSqlDatabase::removeDatabase(connectionName);
    }

    m_dbPath.clear();
}

bool DataService::isOpen() const
{
    return m_db.isValid()
        && m_db.isOpen();
}

QString DataService::currentDatabasePath() const
{
    return m_dbPath;
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

int DataService::createTeacher(
    const Teacher& teacher
    )
{
    if (!m_teacherRepository)
    {
        return 0;
    }

    return m_teacherRepository->createTeacher(
        teacher
        );
}

int DataService::saveTeacher(
    const Teacher& teacher
    )
{
    if (!m_teacherRepository)
    {
        return 0;
    }

    return m_teacherRepository->saveTeacher(
        teacher
        );
}

void DataService::updateTeacher(
    const Teacher& teacher
    )
{
    if (m_teacherRepository)
    {
        m_teacherRepository->updateTeacher(
            teacher
            );
    }
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

void DataService::deleteTeacher(
    int teacherId
    )
{
    if (m_teacherRepository)
    {
        m_teacherRepository->deleteTeacher(
            teacherId
            );
    }
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
        return std::unexpected(QStringLiteral("No database is open."));
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
        return std::unexpected(QStringLiteral("No database is open."));
    }
    return m_gsTeamRepository->saveDirectory(members, deletedIds);
}

Result<TeacherImportSummary> DataService::importTeachers(
    const TeacherImportPlan& plan
    )
{
    if (!m_teacherImportRepository)
    {
        return std::unexpected(QStringLiteral("No database is open."));
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

int DataService::createClass(
    const QString &name
    )
{
    if (!m_classRepository)
    {
        return 0;
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

void DataService::updateClassName(
    int classId,
    const QString &name
    )
{
    if (m_classRepository)
    {
        m_classRepository->updateClassName(
            classId,
            name
            );
    }
}

void DataService::deleteClass(
    int classId
    )
{
    if (m_classRepository)
    {
        m_classRepository->deleteClass(
            classId
            );
    }
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
            QStringLiteral("Schedule import is unavailable.")
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
            QStringLiteral("Schedule import is unavailable.")
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

    m_db.commit();
}

Status DataService::saveAs(
    const QString &destinationPath
    )
{
    if (!isOpen())
    {
        return std::unexpected(
            QStringLiteral("No database is open.")
            );
    }

    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    const QString sourcePath =
        QFileInfo(m_dbPath).absoluteFilePath();

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
            QStringLiteral("Unable to replace existing database file:\n%1")
                .arg(targetPath)
            );
    }

    if (!QFile::copy(sourcePath, targetPath))
    {
        return std::unexpected(
            QStringLiteral("Unable to copy database to:\n%1")
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
            QStringLiteral("No database is open.")
            );
    }

    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    const QString sourcePath =
        QFileInfo(m_dbPath).absoluteFilePath();

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
            QStringLiteral("Unable to replace existing database file:\n%1")
                .arg(targetPath)
            );
    }

    if (!QFile::copy(sourcePath, targetPath))
    {
        return std::unexpected(
            QStringLiteral("Unable to copy database to:\n%1")
                .arg(targetPath)
            );
    }

    return {};
}
