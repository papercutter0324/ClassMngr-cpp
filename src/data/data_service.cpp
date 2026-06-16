#include "data_service.h"

#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/campus_record_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
bool tableHasColumn(
    QSqlDatabase& db,
    const QString& tableName,
    const QString& columnName
    )
{
    QSqlQuery query(db);

    if (!query.exec(
            QString("PRAGMA table_info(%1)")
                .arg(tableName)
            ))
    {
        qWarning()
            << "Failed to inspect table columns for"
            << tableName
            << ":"
            << query.lastError().text();

        return false;
    }

    while (query.next())
    {
        if (query.value("name").toString() == columnName)
        {
            return true;
        }
    }

    return false;
}

void ensureTableColumn(
    QSqlDatabase& db,
    const QString& tableName,
    const QString& columnName,
    const QString& definition
    )
{
    if (tableHasColumn(db, tableName, columnName))
    {
        return;
    }

    QSqlQuery query(db);

    if (!query.exec(
            QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                .arg(tableName, columnName, definition)
            ))
    {
        qWarning()
            << "Failed to add column"
            << columnName
            << "to"
            << tableName
            << ":"
            << query.lastError().text();
    }
}
} // namespace

DataService::DataService(
    const QString &dbPath
    )
    : m_dbPath(dbPath)
{
}

DataService::~DataService()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
}

bool DataService::open()
{
    QDir().mkpath("data");

    m_db =
        QSqlDatabase::addDatabase("QSQLITE");

    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open())
    {
        return false;
    }

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

    m_classRepository =
        std::make_unique<ClassRepository>(
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

    createTables();

    return true;
}

void DataService::createTables()
{
    QSqlQuery query(m_db);

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS roster_columns (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            name TEXT,

            position INTEGER,

            width INTEGER
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS roster_data (
            class_id INTEGER,

            row_index INTEGER,

            col_index INTEGER,

            value TEXT,

            PRIMARY KEY (
                class_id,
                row_index,
                col_index
            )
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS speaking_evaluations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            evaluation_name TEXT,

            UNIQUE(
                class_id,
                evaluation_name
            )
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS speaking_eval_data (
            evaluation_id INTEGER,

            row_index INTEGER,

            col_0 TEXT,
            col_1 TEXT,
            col_2 TEXT,
            col_3 TEXT,
            col_4 TEXT,
            col_5 TEXT,
            col_6 TEXT,
            col_7 TEXT,
            col_8 TEXT,
            col_9 TEXT,
            col_10 TEXT,

            PRIMARY KEY (
                evaluation_id,
                row_index
            )
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS campuses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            name TEXT NOT NULL,

            building_name TEXT,
            address TEXT,
            phone_number TEXT,
            office_number TEXT,

            transit_steps TEXT,
            arrival_info TEXT,

            image_path TEXT,

            office_wifi TEXT,
            office_wifi_password TEXT,

            printer_name TEXT,
            printer_steps TEXT,

            photocopier_code TEXT,

            housing_locations TEXT
        )
    )");

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("building_name"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("address"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("phone_number"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("office_number"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("transit_steps"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("arrival_info"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("image_path"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("office_wifi"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("office_wifi_password"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("printer_name"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("printer_steps"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("photocopier_code"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("campuses"),
        QStringLiteral("housing_locations"),
        QStringLiteral("TEXT")
        );

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            teacher_kr TEXT,
            teacher_en TEXT,

            room_number TEXT,

            wifi_name TEXT,
            wifi_password TEXT,
            internet_type TEXT DEFAULT 'WiFi',

            zoom_id TEXT,
            zoom_password TEXT,
            projection_type TEXT DEFAULT 'HDMI',

            notes TEXT
        )
    )");

    ensureTableColumn(
        m_db,
        QStringLiteral("teachers"),
        QStringLiteral("internet_type"),
        QStringLiteral("TEXT DEFAULT 'WiFi'")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("teachers"),
        QStringLiteral("projection_type"),
        QStringLiteral("TEXT DEFAULT 'HDMI'")
        );

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            name TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_info (
            class_id INTEGER PRIMARY KEY,

            teacher_id INTEGER,

            class_grade TEXT,
            class_level TEXT,

            reading_book TEXT,
            essay_book TEXT,

            class_color TEXT DEFAULT '#FFFFFF',
            font_color TEXT DEFAULT '#000000',

            notes TEXT,
            time_filler_activities TEXT
        )
    )");

    ensureTableColumn(
        m_db,
        QStringLiteral("class_info"),
        QStringLiteral("time_filler_activities"),
        QStringLiteral("TEXT")
        );

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_intensive_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT,

            UNIQUE(day, start_time)
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS intensive_slot_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            day TEXT,
            start_time TEXT,
            state TEXT,

            UNIQUE(day, start_time)
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS calendar_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            title TEXT NOT NULL,
            event_type TEXT DEFAULT 'Other',
            all_day INTEGER DEFAULT 0,
            start_date TEXT,
            start_time TEXT,
            end_date TEXT,
            end_time TEXT
        )
    )");

    ensureTableColumn(
        m_db,
        QStringLiteral("calendar_events"),
        QStringLiteral("event_type"),
        QStringLiteral("TEXT DEFAULT 'Other'")
        );

    ensureTableColumn(
        m_db,
        QStringLiteral("calendar_events"),
        QStringLiteral("all_day"),
        QStringLiteral("INTEGER DEFAULT 0")
        );
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
    const QString& state
    )
{
    if (m_intensiveSlotStateRepository)
    {
        m_intensiveSlotStateRepository->saveIntensiveSlotState(
            day,
            startTime,
            state
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
    m_db.commit();
}

void DataService::saveAs(
    const QString &destinationPath
    )
{
    QFile::remove(destinationPath);
    QFile::copy(m_dbPath, destinationPath);
}

void DataService::exportAs(
    const QString &destinationPath
    )
{
    QFile::remove(destinationPath);
    QFile::copy(m_dbPath, destinationPath);
}
