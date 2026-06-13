#include "dataservice.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace
{

struct TimeInterval
{
    int start{-1};
    int end{-1};
};

constexpr int MinutesPerDay = 24 * 60;
constexpr int MinutesPerWeek = 7 * MinutesPerDay;

int dayIndex(const QString& day)
{
    static const QStringList days{
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday"
    };

    return days.indexOf(day);
}

int timeToMinutes(const QString& value)
{
    const QStringList parts =
        value.trimmed().split(
            ' ',
            Qt::SkipEmptyParts
            );

    if (parts.size() != 2)
    {
        return -1;
    }

    const QStringList timeParts =
        parts[0].split(':');

    if (timeParts.size() != 2)
    {
        return -1;
    }

    bool hourOk = false;
    bool minuteOk = false;

    int hour =
        timeParts[0].toInt(&hourOk);

    const int minute =
        timeParts[1].toInt(&minuteOk);

    const QString period =
        parts[1].toUpper();

    if (
        !hourOk
        || !minuteOk
        || hour < 1
        || hour > 12
        || minute < 0
        || minute > 59
        || (period != "AM" && period != "PM")
        )
    {
        return -1;
    }

    if (period == "AM")
    {
        if (hour == 12)
        {
            hour = 0;
        }
    }
    else if (hour != 12)
    {
        hour += 12;
    }

    return hour * 60 + minute;
}

bool toInterval(
    const ClassTime& time,
    TimeInterval& interval
    )
{
    const int day =
        dayIndex(time.day);

    const int start =
        timeToMinutes(time.startTime);

    const int end =
        timeToMinutes(time.endTime);

    if (day < 0 || start < 0 || end < 0)
    {
        return false;
    }

    interval.start =
        day * MinutesPerDay + start;

    interval.end =
        day * MinutesPerDay + end;

    if (interval.end <= interval.start)
    {
        interval.end += MinutesPerDay;
    }

    return true;
}

bool intervalsOverlap(
    const TimeInterval& first,
    const TimeInterval& second
    )
{
    for (int offset : { -MinutesPerWeek, 0, MinutesPerWeek })
    {
        const int secondStart =
            second.start + offset;

        const int secondEnd =
            second.end + offset;

        if (first.start < secondEnd && secondStart < first.end)
        {
            return true;
        }
    }

    return false;
}

QString classDisplayName(
    const QString& className,
    int classId
    )
{
    if (!className.trimmed().isEmpty())
    {
        return className.trimmed();
    }

    return QString("Class %1").arg(classId);
}

QString normalizedTeacherChoice(
    const QString& value,
    const QStringList& choices,
    const QString& fallback
    )
{
    const QString trimmed =
        value.trimmed();

    for (const QString& choice : choices)
    {
        if (choice.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return choice;
        }
    }

    return fallback;
}

QString normalizedInternetType(
    const QString& value
    )
{
    return normalizedTeacherChoice(
        value,
        {
            QStringLiteral("WiFi"),
            QStringLiteral("LAN"),
            QStringLiteral("Both"),
            QStringLiteral("N/A")
        },
        QStringLiteral("WiFi")
        );
}

QString normalizedProjectionType(
    const QString& value
    )
{
    return normalizedTeacherChoice(
        value,
        {
            QStringLiteral("HDMI"),
            QStringLiteral("Zoom"),
            QStringLiteral("Any"),
            QStringLiteral("N/A")
        },
        QStringLiteral("HDMI")
        );
}

CampusRecord campusFromQuery(
    const QSqlQuery& query
    )
{
    CampusRecord campus;

    campus.id =
        query.value("id").toInt();
    campus.name =
        query.value("name").toString();
    campus.buildingName =
        query.value("building_name").toString();
    campus.address =
        query.value("address").toString();
    campus.phoneNumber =
        query.value("phone_number").toString();
    campus.officeNumber =
        query.value("office_number").toString();
    campus.transitSteps =
        query.value("transit_steps").toString();
    campus.arrivalInfo =
        query.value("arrival_info").toString();
    campus.imagePath =
        query.value("image_path").toString();
    campus.officeWifi =
        query.value("office_wifi").toString();
    campus.officeWifiPassword =
        query.value("office_wifi_password").toString();
    campus.printerName =
        query.value("printer_name").toString();
    campus.printerSteps =
        query.value("printer_steps").toString();
    campus.photocopierCode =
        query.value("photocopier_code").toString();
    campus.housingLocations =
        query.value("housing_locations").toString();

    return campus;
}

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



// =========================================================
// Constructor
// =========================================================

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

// =========================================================
// Database
// =========================================================

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

    createTables();

    return true;
}



// =========================================================
// Tables
// =========================================================

void DataService::createTables()
{
    QSqlQuery query;

    // App Settings
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )");

    // Roster
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

    // Speaking Evaluations
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

    // Campuses
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

    // Teachers
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

    // Classes
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            name TEXT
        )
    )");

    // Class Info
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

    // Class Times
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )");

    // Intensive Times
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

    // Intensive Slot States
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS intensive_slot_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            day TEXT,
            start_time TEXT,
            state TEXT,

            UNIQUE(day, start_time)
        )
    )");
}

// =========================================================
// Settings
// =========================================================

void DataService::saveSetting(
    const QString &key,
    const QVariant &value
    )
{
    QSqlQuery query;

    query.prepare(R"(
        INSERT INTO app_settings (
            key,
            value
        )
        VALUES (?, ?)

        ON CONFLICT(key)
        DO UPDATE SET
            value=excluded.value
    )");

    query.addBindValue(key);
    query.addBindValue(value);

    query.exec();
}

QVariant DataService::loadSetting(
    const QString &key,
    const QVariant &defaultValue
    )
{
    QSqlQuery query;

    query.prepare(R"(
        SELECT value
        FROM app_settings
        WHERE key=?
    )");

    query.addBindValue(key);

    if (!query.exec())
    {
        return defaultValue;
    }

    if (!query.next())
    {
        return defaultValue;
    }

    return query.value(0);
}

// =========================================================
// Teachers
// =========================================================

int DataService::createTeacher(
    const Teacher& teacher
    )
{
    QSqlQuery query;

    query.prepare(R"(
        INSERT INTO teachers (
            teacher_kr,
            teacher_en,
            room_number,
            wifi_name,
            wifi_password,
            internet_type,
            zoom_id,
            zoom_password,
            projection_type,
            notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(
        normalizedInternetType(teacher.internetType));
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(
        normalizedProjectionType(teacher.projectionType));
    query.addBindValue(teacher.notes);

    query.exec();

    return query.lastInsertId().toInt();
}

int DataService::saveTeacher(
    const Teacher& teacher
    )
{
    if (teacher.id > 0)
    {
        updateTeacher(teacher);

        return teacher.id;
    }

    return createTeacher(teacher);
}

void DataService::updateTeacher(
    const Teacher& teacher
    )
{
    QSqlQuery query;

    query.prepare(R"(
        UPDATE teachers
        SET
            teacher_kr=?,
            teacher_en=?,
            room_number=?,
            wifi_name=?,
            wifi_password=?,
            internet_type=?,
            zoom_id=?,
            zoom_password=?,
            projection_type=?,
            notes=?
        WHERE id=?
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(
        normalizedInternetType(teacher.internetType));
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(
        normalizedProjectionType(teacher.projectionType));
    query.addBindValue(teacher.notes);
    query.addBindValue(teacher.id);

    query.exec();
}

Teacher DataService::getTeacher(
    int teacherId
    )
{
    Teacher teacher;

    QSqlQuery query(m_db);

    query.prepare(R"(
        SELECT *
        FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load teacher:"
            << query.lastError().text();

        return teacher;
    }

    if (!query.next())
    {
        return teacher; // returns empty/default teacher
    }

    teacher.id =           query.value("id").toInt();
    teacher.teacherKr =    query.value("teacher_kr").toString();
    teacher.teacherEn =    query.value("teacher_en").toString();
    teacher.roomNumber =   query.value("room_number").toString();
    teacher.wifiName =     query.value("wifi_name").toString();
    teacher.wifiPassword = query.value("wifi_password").toString();
    teacher.internetType =
        normalizedInternetType(
            query.value("internet_type").toString()
            );
    teacher.zoomId =       query.value("zoom_id").toString();
    teacher.zoomPassword = query.value("zoom_password").toString();
    teacher.projectionType =
        normalizedProjectionType(
            query.value("projection_type").toString()
            );
    teacher.notes =        query.value("notes").toString();

    return teacher;
}

QList<Teacher>
DataService::getAllTeachers()
{
    QList<Teacher> teachers;

    QSqlQuery query(m_db);

    if (!query.exec(R"(
        SELECT *
        FROM teachers
        ORDER BY teacher_en
    )"))
    {
        qWarning()
            << "Failed to load teachers:"
            << query.lastError().text();

        return teachers;
    }

    while (query.next())
    {
        Teacher teacher;

        teacher.id =           query.value("id").toInt();
        teacher.teacherKr =    query.value("teacher_kr").toString();
        teacher.teacherEn =    query.value("teacher_en").toString();
        teacher.roomNumber =   query.value("room_number").toString();
        teacher.wifiName =     query.value("wifi_name").toString();
        teacher.wifiPassword = query.value("wifi_password").toString();
        teacher.internetType =
            normalizedInternetType(
                query.value("internet_type").toString()
                );
        teacher.zoomId =       query.value("zoom_id").toString();
        teacher.zoomPassword = query.value("zoom_password").toString();
        teacher.projectionType =
            normalizedProjectionType(
                query.value("projection_type").toString()
                );
        teacher.notes =        query.value("notes").toString();

        teachers.append(teacher);
    }

    return teachers;
}

void DataService::deleteTeacher(
    int teacherId
    )
{
    QSqlQuery query;

    query.prepare(R"(
        UPDATE class_info
        SET teacher_id=NULL
        WHERE teacher_id=?
    )");

    query.addBindValue(teacherId);

    query.exec();

    query.prepare(R"(
        DELETE FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    query.exec();
}

// =========================================================
// Classes
// =========================================================

int DataService::createClass(
    const QString &name
    )
{
    QSqlQuery query;

    query.prepare(R"(
        INSERT INTO classes (
            name
        )
        VALUES (?)
    )");

    query.addBindValue(name);

    query.exec();

    return query.lastInsertId().toInt();
}

bool DataService::saveClassInfo(
    const ClassInfo& info
    )
{
    if (!m_db.transaction())
    {
        qWarning()
            << "Failed to start class info save transaction:"
            << m_db.lastError().text();

        return false;
    }

    QSqlQuery query(m_db);

    auto rollbackOnFailure =
        [this, &query](const QString& operation)
        {
            qWarning()
                << operation
                << "failed while saving class info:"
                << query.lastError().text();

            if (!m_db.rollback())
            {
                qWarning()
                    << "Failed to roll back class info save transaction:"
                    << m_db.lastError().text();
            }

            return false;
        };

    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id,
            class_grade,
            class_level,
            reading_book,
            essay_book,
            class_color,
            font_color,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id,
            class_grade=excluded.class_grade,
            class_level=excluded.class_level,
            reading_book=excluded.reading_book,
            essay_book=excluded.essay_book,
            class_color=excluded.class_color,
            font_color=excluded.font_color,
            notes=excluded.notes,
            time_filler_activities=excluded.time_filler_activities
    )");

    query.addBindValue(info.classId);
    query.addBindValue(info.teacherId);
    query.addBindValue(info.classGrade);
    query.addBindValue(info.classLevel);
    query.addBindValue(info.readingBook);
    query.addBindValue(info.essayBook);
    query.addBindValue(info.classColor);
    query.addBindValue(info.fontColor);
    query.addBindValue(info.notes);
    query.addBindValue(info.timeFillerActivities);

    if (!query.exec())
    {
        return rollbackOnFailure(
            "Upserting class_info"
            );
    }

    // Times
    query.prepare(
        "DELETE FROM class_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    if (!query.exec())
    {
        return rollbackOnFailure(
            "Deleting class_times"
            );
    }

    for (const ClassTime& time : info.classTimes)
    {
        query.prepare(R"(
            INSERT INTO class_times (
                class_id,
                day,
                start_time,
                end_time
            )
            VALUES (?, ?, ?, ?)
        )");

        query.addBindValue(info.classId);
        query.addBindValue(time.day);
        query.addBindValue(time.startTime);
        query.addBindValue(time.endTime);

        if (!query.exec())
        {
            return rollbackOnFailure(
                "Inserting class_times"
                );
        }
    }

    // Intensive Times
    query.prepare(
        "DELETE FROM class_intensive_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    if (!query.exec())
    {
        return rollbackOnFailure(
            "Deleting class_intensive_times"
            );
    }

    for (const ClassTime& time : info.intensiveTimes)
    {
        query.prepare(R"(
            INSERT INTO class_intensive_times (
                class_id,
                day,
                start_time,
                end_time
            )
            VALUES (?, ?, ?, ?)
        )");

        query.addBindValue(info.classId);
        query.addBindValue(time.day);
        query.addBindValue(time.startTime);
        query.addBindValue(time.endTime);

        if (!query.exec())
        {
            return rollbackOnFailure(
                "Inserting class_intensive_times"
                );
        }
    }

    if (!m_db.commit())
    {
        qWarning()
            << "Failed to commit class info save transaction:"
            << m_db.lastError().text();

        if (!m_db.rollback())
        {
            qWarning()
                << "Failed to roll back failed class info save commit:"
                << m_db.lastError().text();
        }

        return false;
    }

    return true;
}

bool DataService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (classId <= 0)
    {
        return false;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            notes=excluded.notes,
            time_filler_activities=excluded.time_filler_activities
    )");

    query.addBindValue(classId);
    query.addBindValue(notes);
    query.addBindValue(timeFillerActivities);

    if (!query.exec())
    {
        qWarning()
            << "Failed to save class notes:"
            << query.lastError().text();

        return false;
    }

    return true;
}

void DataService::updateClassName(
    int classId,
    const QString &name
    )
{
    QSqlQuery query;

    query.prepare(R"(
        UPDATE classes
        SET name=?
        WHERE id=?
    )");

    query.addBindValue(name);
    query.addBindValue(classId);

    query.exec();
}

Classroom DataService::getClassById(
    int classId
    )
{
    Classroom classroom;

    QSqlQuery query;

    query.prepare(R"(
        SELECT *
        FROM classes
        WHERE id=?
    )");

    query.addBindValue(classId);

    query.exec();

    if (!query.next())
    {
        return classroom;
    }

    classroom.id =
        query.value("id").toInt();

    classroom.name =
        query.value("name").toString();

    return classroom;
}

QList<Classroom>
DataService::getClasses()
{
    QList<Classroom> classes;

    QSqlQuery query(m_db);

    if (!query.exec(R"(
        SELECT *
        FROM classes
        ORDER BY name
    )"))
    {
        qWarning()
            << "Failed to load classes:"
            << query.lastError().text();

        return classes;
    }

    while (query.next())
    {
        Classroom classroom;

        classroom.id =
            query.value("id").toInt();

        classroom.name =
            query.value("name").toString();

        classes.append(classroom);
    }

    return classes;
}

ClassInfo DataService::loadClassInfo(
    int classId
    )
{
    ClassInfo info;
    info.classId =    classId;
    info.classColor = "#FFFFFF";
    info.fontColor =  "#000000";

    QSqlQuery query(m_db);

    // Main Class Info + Teacher Join
    query.prepare(R"(
        SELECT
            ci.*,

            t.teacher_kr,
            t.teacher_en,
            t.room_number,
            t.wifi_name,
            t.wifi_password,
            t.internet_type,
            t.zoom_id,
            t.zoom_password,
            t.projection_type

        FROM class_info ci

        LEFT JOIN teachers t
        ON ci.teacher_id = t.id

        WHERE ci.class_id = ?
    )");

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load class info:"
            << query.lastError().text();
    }
    else if (query.next())
    {
        info.teacherId =    query.value("teacher_id").toInt();
        info.teacherKr =    query.value("teacher_kr").toString();
        info.teacherEn =    query.value("teacher_en").toString();
        info.roomNumber =   query.value("room_number").toString();
        info.wifiName =     query.value("wifi_name").toString();
        info.wifiPassword = query.value("wifi_password").toString();
        info.internetType =
            normalizedInternetType(
                query.value("internet_type").toString()
                );
        info.zoomId =       query.value("zoom_id").toString();
        info.zoomPassword = query.value("zoom_password").toString();
        info.projectionType =
            normalizedProjectionType(
                query.value("projection_type").toString()
                );
        info.classGrade =   query.value("class_grade").toString();
        info.classLevel =   query.value("class_level").toString();
        info.readingBook =  query.value("reading_book").toString();
        info.essayBook =    query.value("essay_book").toString();

        const QString classColor =
            query.value("class_color").toString();

        if (!classColor.isEmpty())
        {
            info.classColor = classColor;
        }

        const QString fontColor =
            query.value("font_color").toString();

        if (!fontColor.isEmpty())
        {
            info.fontColor = fontColor;
        }

        info.notes =
            query.value("notes").toString();

        info.timeFillerActivities =
            query.value("time_filler_activities").toString();
    }

    // Regular Times
    query.prepare(R"(
        SELECT *
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load regular class times:"
            << query.lastError().text();
    }
    else
    {
        while (query.next())
        {
            ClassTime time;

            time.day =       query.value("day").toString();
            time.startTime = query.value("start_time").toString();
            time.endTime =   query.value("end_time").toString();

            info.classTimes.append(time);
        }
    }

    // Intensive Times
    query.prepare(R"(
        SELECT *
        FROM class_intensive_times
        WHERE class_id = ?
        ORDER BY id
    )");

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load intensive class times:"
            << query.lastError().text();
    }
    else
    {
        while (query.next())
        {
            ClassTime time;

            time.day =       query.value("day").toString();
            time.startTime = query.value("start_time").toString();
            time.endTime =   query.value("end_time").toString();

            info.intensiveTimes.append(time);
        }
    }

    return info;
}

// =========================================================
// Intensive Slot States
// =========================================================

QList<IntensiveSlotState> DataService::loadIntensiveSlotStates()
{
    QList<IntensiveSlotState> states;

    QSqlQuery query(m_db);

    if (!query.exec(R"(
        SELECT
            day,
            start_time,
            state
        FROM intensive_slot_states
        ORDER BY day, start_time
    )"))
    {
        qWarning()
            << "Failed to load intensive slot states:"
            << query.lastError().text();

        return states;
    }

    while (query.next())
    {
        IntensiveSlotState state;

        state.day =
            query.value("day").toString();

        state.startTime =
            query.value("start_time").toString();

        state.state =
            query.value("state").toString();

        states.append(state);
    }

    return states;
}

void DataService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state
    )
{
    QSqlQuery query(m_db);

    if (state == QStringLiteral("essay"))
    {
        query.prepare(R"(
            DELETE FROM intensive_slot_states
            WHERE day=?
            AND start_time=?
        )");

        query.addBindValue(day);
        query.addBindValue(startTime);

        if (!query.exec())
        {
            qWarning()
                << "Failed to delete intensive slot state:"
                << query.lastError().text();
        }

        return;
    }

    query.prepare(R"(
        INSERT INTO intensive_slot_states (
            day,
            start_time,
            state
        )
        VALUES (?, ?, ?)

        ON CONFLICT(day, start_time)
        DO UPDATE SET
            state=excluded.state
    )");

    query.addBindValue(day);
    query.addBindValue(startTime);
    query.addBindValue(state);

    if (!query.exec())
    {
        qWarning()
            << "Failed to save intensive slot state:"
            << query.lastError().text();
    }
}

QList<ClassConflict> DataService::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    QList<ClassConflict> conflicts;

    const Classroom currentClass =
        getClassById(classId);

    const QString currentClassName =
        classDisplayName(
            currentClass.name,
            classId
            );

    QList<TimeInterval> candidateIntervals;

    for (const ClassTime& time : times)
    {
        TimeInterval interval;

        if (toInterval(time, interval))
        {
            candidateIntervals.append(interval);
        }
        else
        {
            candidateIntervals.append(TimeInterval{});
        }
    }

    for (int i = 0; i < times.size(); ++i)
    {
        if (candidateIntervals[i].start < 0)
        {
            continue;
        }

        for (int j = i + 1; j < times.size(); ++j)
        {
            if (candidateIntervals[j].start < 0)
            {
                continue;
            }

            if (
                intervalsOverlap(
                    candidateIntervals[i],
                    candidateIntervals[j]
                    )
                )
            {
                ClassConflict conflict;
                conflict.classId = classId;
                conflict.className = currentClassName;
                conflict.day = times[i].day;
                conflict.startTime = times[i].startTime;
                conflict.endTime = times[i].endTime;
                conflict.conflictingClassName =
                    currentClassName;

                conflicts.append(conflict);
            }
        }
    }

    const QString tableName =
        type == ScheduleType::Regular
            ? QString("class_times")
            : QString("class_intensive_times");

    QSqlQuery query(m_db);

    query.prepare(
        QString(R"(
            SELECT
                times.class_id,
                classes.name AS class_name,
                times.day,
                times.start_time,
                times.end_time
            FROM %1 times
            LEFT JOIN classes
            ON classes.id = times.class_id
            WHERE times.class_id != ?
        )").arg(tableName)
        );

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load class time conflicts:"
            << query.lastError().text();

        return conflicts;
    }

    while (query.next())
    {
        ClassTime existingTime;
        existingTime.day =
            query.value("day").toString();
        existingTime.startTime =
            query.value("start_time").toString();
        existingTime.endTime =
            query.value("end_time").toString();

        TimeInterval existingInterval;

        if (!toInterval(existingTime, existingInterval))
        {
            continue;
        }

        const int conflictingClassId =
            query.value("class_id").toInt();

        const QString conflictingClassName =
            classDisplayName(
                query.value("class_name").toString(),
                conflictingClassId
                );

        for (int i = 0; i < times.size(); ++i)
        {
            if (candidateIntervals[i].start < 0)
            {
                continue;
            }

            if (
                intervalsOverlap(
                    candidateIntervals[i],
                    existingInterval
                    )
                )
            {
                ClassConflict conflict;
                conflict.classId = classId;
                conflict.className = currentClassName;
                conflict.day = times[i].day;
                conflict.startTime = times[i].startTime;
                conflict.endTime = times[i].endTime;
                conflict.conflictingClassName =
                    conflictingClassName;

                conflicts.append(conflict);
            }
        }
    }

    return conflicts;
}

// =========================================================
// Roster
// =========================================================

void DataService::saveRoster(
    int classId,
    const Roster& roster
    )
{
    if (classId <= 0)
    {
        return;
    }

    if (!m_db.transaction())
    {
        qWarning()
            << "Failed to start roster save transaction:"
            << m_db.lastError().text();

        return;
    }

    QSqlQuery query(m_db);

    auto rollbackOnFailure =
        [this, &query](const QString& operation)
        {
            qWarning()
                << operation
                << "failed while saving roster:"
                << query.lastError().text();

            if (!m_db.rollback())
            {
                qWarning()
                    << "Failed to roll back roster save transaction:"
                    << m_db.lastError().text();
            }

            return;
        };

    query.prepare(
        "DELETE FROM roster_columns WHERE class_id=?"
        );

    query.addBindValue(classId);

    if (!query.exec())
    {
        rollbackOnFailure(
            "Deleting roster_columns"
            );

        return;
    }

    query.prepare(
        "DELETE FROM roster_data WHERE class_id=?"
        );

    query.addBindValue(classId);

    if (!query.exec())
    {
        rollbackOnFailure(
            "Deleting roster_data"
            );

        return;
    }

    for (int column = 0; column < roster.columns.size(); ++column)
    {
        query.prepare(R"(
            INSERT INTO roster_columns (
                class_id,
                name,
                position,
                width
            )
            VALUES (?, ?, ?, ?)
        )");

        const int width =
            column < roster.columnWidths.size()
                ? roster.columnWidths[column]
                : 0;

        query.addBindValue(classId);
        query.addBindValue(roster.columns[column]);
        query.addBindValue(column);
        query.addBindValue(width);

        if (!query.exec())
        {
            rollbackOnFailure(
                "Inserting roster_columns"
                );

            return;
        }
    }

    for (int row = 0; row < roster.rows.size(); ++row)
    {
        const QStringList& rowValues =
            roster.rows[row];

        for (int column = 0; column < roster.columns.size(); ++column)
        {
            const QString value =
                column < rowValues.size()
                    ? rowValues[column]
                    : QString();

            if (value.isEmpty())
            {
                continue;
            }

            query.prepare(R"(
                INSERT INTO roster_data (
                    class_id,
                    row_index,
                    col_index,
                    value
                )
                VALUES (?, ?, ?, ?)
            )");

            query.addBindValue(classId);
            query.addBindValue(row);
            query.addBindValue(column);
            query.addBindValue(value);

            if (!query.exec())
            {
                rollbackOnFailure(
                    "Inserting roster_data"
                    );

                return;
            }
        }
    }

    if (!m_db.commit())
    {
        qWarning()
            << "Failed to commit roster save transaction:"
            << m_db.lastError().text();

        if (!m_db.rollback())
        {
            qWarning()
                << "Failed to roll back failed roster save commit:"
                << m_db.lastError().text();
        }
    }
}

Roster DataService::loadRoster(
    int classId
    )
{
    Roster roster;

    if (classId <= 0)
    {
        return roster;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        SELECT
            name,
            width
        FROM roster_columns
        WHERE class_id=?
        ORDER BY position, id
    )");

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load roster columns:"
            << query.lastError().text();

        return roster;
    }

    while (query.next())
    {
        roster.columns.append(
            query.value("name").toString()
            );

        roster.columnWidths.append(
            query.value("width").toInt()
            );
    }

    if (roster.columns.isEmpty())
    {
        return roster;
    }

    query.prepare(R"(
        SELECT
            row_index,
            col_index,
            value
        FROM roster_data
        WHERE class_id=?
        ORDER BY row_index, col_index
    )");

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load roster data:"
            << query.lastError().text();

        return roster;
    }

    while (query.next())
    {
        const int row =
            query.value("row_index").toInt();

        const int column =
            query.value("col_index").toInt();

        if (
            row < 0
            || column < 0
            || column >= roster.columns.size()
            )
        {
            continue;
        }

        while (roster.rows.size() <= row)
        {
            QStringList emptyRow;

            for (int index = 0; index < roster.columns.size(); ++index)
            {
                emptyRow.append(QString());
            }

            roster.rows.append(emptyRow);
        }

        roster.rows[row][column] =
            query.value("value").toString();
    }

    return roster;
}

int DataService::getRosterStudentCount(
    int classId
    )
{
    const Roster roster =
        loadRoster(classId);

    const int englishColumn =
        roster.columns.indexOf(
            QStringLiteral("English")
            );

    const int koreanColumn =
        roster.columns.indexOf(
            QStringLiteral("Korean")
            );

    if (englishColumn < 0 && koreanColumn < 0)
    {
        return 0;
    }

    int count = 0;

    for (const QStringList& row : roster.rows)
    {
        const bool hasEnglish =
            englishColumn >= 0
            && englishColumn < row.size()
            && !row[englishColumn].trimmed().isEmpty();

        const bool hasKorean =
            koreanColumn >= 0
            && koreanColumn < row.size()
            && !row[koreanColumn].trimmed().isEmpty();

        if (hasEnglish || hasKorean)
        {
            ++count;
        }
    }

    return count;
}

// =========================================================
// Speaking Evaluations
// =========================================================

bool DataService::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return false;
    }

    if (!m_db.transaction())
    {
        qWarning()
            << "Failed to start speaking eval save transaction:"
            << m_db.lastError().text();

        return false;
    }

    QSqlQuery query(m_db);

    auto rollbackOnFailure =
        [this, &query](const QString& operation)
        {
            qWarning()
                << operation
                << "failed while saving speaking eval:"
                << query.lastError().text();

            if (!m_db.rollback())
            {
                qWarning()
                    << "Failed to roll back speaking eval save transaction:"
                    << m_db.lastError().text();
            }

            return false;
        };

    int evaluationId = -1;

    query.prepare(R"(
        SELECT id
        FROM speaking_evaluations
        WHERE class_id=? AND evaluation_name=?
    )");

    query.addBindValue(classId);
    query.addBindValue(evaluationName);

    if (!query.exec())
    {
        return rollbackOnFailure(
            "Selecting speaking_evaluations"
            );
    }

    if (query.next())
    {
        evaluationId =
            query.value("id").toInt();
    }
    else
    {
        query.prepare(R"(
            INSERT INTO speaking_evaluations (
                class_id,
                evaluation_name
            )
            VALUES (?, ?)
        )");

        query.addBindValue(classId);
        query.addBindValue(evaluationName);

        if (!query.exec())
        {
            return rollbackOnFailure(
                "Inserting speaking_evaluations"
                );
        }

        evaluationId =
            query.lastInsertId().toInt();
    }

    for (int row = 0; row < SpeakingEval::RowCount; ++row)
    {
        query.prepare(R"(
            INSERT OR IGNORE INTO speaking_eval_data (
                evaluation_id,
                row_index
            )
            VALUES (?, ?)
        )");

        query.addBindValue(evaluationId);
        query.addBindValue(row);

        if (!query.exec())
        {
            return rollbackOnFailure(
                "Ensuring speaking_eval_data rows"
                );
        }
    }

    query.prepare(R"(
        SELECT *
        FROM speaking_eval_data
        WHERE evaluation_id=?
    )");

    query.addBindValue(evaluationId);

    if (!query.exec())
    {
        return rollbackOnFailure(
            "Selecting speaking_eval_data"
            );
    }

    QHash<int, QStringList> existingRows;

    while (query.next())
    {
        QStringList values;

        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            values.append(
                query.value(
                    QStringLiteral("col_%1")
                        .arg(column)
                    ).toString()
                );
        }

        existingRows.insert(
            query.value("row_index").toInt(),
            values
            );
    }

    QList<SpeakingEvalCellChange> cellsToUpdate =
        dirtyCells;

    if (cellsToUpdate.isEmpty())
    {
        for (int row = 0; row < SpeakingEval::RowCount; ++row)
        {
            for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
            {
                cellsToUpdate.append({ row, column });
            }
        }
    }

    for (const SpeakingEvalCellChange& cell : cellsToUpdate)
    {
        if (
            cell.row < 0
            || cell.row >= SpeakingEval::RowCount
            || cell.column < 0
            || cell.column >= SpeakingEval::ColumnCount
            )
        {
            continue;
        }

        const QString newValue =
            cell.row < rows.size()
            && cell.column < rows[cell.row].size()
                ? rows[cell.row][cell.column]
                : QString();

        const QStringList existingRow =
            existingRows.value(cell.row);

        const QString oldValue =
            cell.column < existingRow.size()
                ? existingRow[cell.column]
                : QString();

        if ((oldValue.isNull() ? QString() : oldValue) == newValue)
        {
            continue;
        }

        query.prepare(
            QString(R"(
                UPDATE speaking_eval_data
                SET col_%1=?
                WHERE evaluation_id=? AND row_index=?
            )").arg(cell.column)
            );

        query.addBindValue(newValue);
        query.addBindValue(evaluationId);
        query.addBindValue(cell.row);

        if (!query.exec())
        {
            return rollbackOnFailure(
                "Updating speaking_eval_data"
                );
        }
    }

    if (!m_db.commit())
    {
        qWarning()
            << "Failed to commit speaking eval save transaction:"
            << m_db.lastError().text();

        if (!m_db.rollback())
        {
            qWarning()
                << "Failed to roll back failed speaking eval commit:"
                << m_db.lastError().text();
        }

        return false;
    }

    return true;
}

SpeakingEvalRows DataService::loadSpeakingEval(
    int classId,
    const QString& evaluationName
    )
{
    SpeakingEvalRows rows;

    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return rows;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        SELECT id
        FROM speaking_evaluations
        WHERE class_id=? AND evaluation_name=?
    )");

    query.addBindValue(classId);
    query.addBindValue(evaluationName);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load speaking evaluation:"
            << query.lastError().text();

        return rows;
    }

    if (!query.next())
    {
        return rows;
    }

    const int evaluationId =
        query.value("id").toInt();

    query.prepare(R"(
        SELECT *
        FROM speaking_eval_data
        WHERE evaluation_id=?
        ORDER BY row_index
    )");

    query.addBindValue(evaluationId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load speaking evaluation rows:"
            << query.lastError().text();

        return rows;
    }

    while (query.next())
    {
        QStringList row;

        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            row.append(
                query.value(
                    QStringLiteral("col_%1")
                        .arg(column)
                    ).toString()
                );
        }

        rows.append(row);
    }

    return rows;
}

QList<SpeakingEvalScore> DataService::buildRosterScoreImport(
    int classId,
    const QString& evaluationName
    )
{
    QList<SpeakingEvalScore> scores;

    const SpeakingEvalRows rows =
        loadSpeakingEval(
            classId,
            evaluationName
            );

    if (rows.isEmpty())
    {
        return scores;
    }

    const QHash<QString, int> gradeToNumber{
        { QStringLiteral("C"), 1 },
        { QStringLiteral("B"), 2 },
        { QStringLiteral("B+"), 3 },
        { QStringLiteral("A"), 4 },
        { QStringLiteral("A+"), 5 }
    };

    const QHash<int, QString> numberToGrade{
        { 1, QStringLiteral("C") },
        { 2, QStringLiteral("B") },
        { 3, QStringLiteral("B+") },
        { 4, QStringLiteral("A") },
        { 5, QStringLiteral("A+") }
    };

    const QList<int> scoreColumns{
        SpeakingEval::toInt(SpeakingEvalColumn::Grammar),
        SpeakingEval::toInt(SpeakingEvalColumn::Pronunciation),
        SpeakingEval::toInt(SpeakingEvalColumn::Fluency),
        SpeakingEval::toInt(SpeakingEvalColumn::Manner),
        SpeakingEval::toInt(SpeakingEvalColumn::Content),
        SpeakingEval::toInt(SpeakingEvalColumn::OverallEffort)
    };

    for (const QStringList& row : rows)
    {
        if (row.size() < SpeakingEval::ColumnCount)
        {
            continue;
        }

        const QString englishName =
            row[SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)]
                .trimmed();

        const QString koreanName =
            row[SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)]
                .trimmed();

        if (englishName.isEmpty() || koreanName.isEmpty())
        {
            continue;
        }

        QList<int> numericScores;
        bool valid = true;

        for (int column : scoreColumns)
        {
            const QString value =
                row[column].trimmed();

            if (!gradeToNumber.contains(value))
            {
                valid = false;
                break;
            }

            numericScores.append(
                gradeToNumber.value(value)
                );
        }

        QString finalGrade =
            QStringLiteral("N/A");

        if (valid && numericScores.size() == scoreColumns.size())
        {
            int sum = 0;

            for (int score : numericScores)
            {
                sum += score;
            }

            const double average =
                static_cast<double>(sum)
                / numericScores.size();

            int rounded =
                static_cast<int>(average);

            if (average - rounded >= 0.4)
            {
                ++rounded;
            }

            rounded =
                qBound(
                    1,
                    rounded,
                    5
                    );

            finalGrade =
                numberToGrade.value(
                    rounded,
                    QStringLiteral("N/A")
                    );
        }

        scores.append(
            {
                englishName,
                koreanName,
                finalGrade
            }
            );
    }

    return scores;
}

void DataService::deleteClass(
    int classId
    )
{
    QSqlQuery query;

    query.prepare("DELETE FROM classes WHERE id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM roster_columns WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM roster_data WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM class_info WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM class_times WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("DELETE FROM class_intensive_times WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    query.prepare("SELECT id FROM speaking_evaluations WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();

    QList<int> evaluationIds;

    while (query.next())
    {
        evaluationIds.append(
            query.value("id").toInt()
            );
    }

    for (int evaluationId : evaluationIds)
    {
        query.prepare("DELETE FROM speaking_eval_data WHERE evaluation_id=?");
        query.addBindValue(evaluationId);
        query.exec();
    }

    query.prepare("DELETE FROM speaking_evaluations WHERE class_id=?");
    query.addBindValue(classId);
    query.exec();
}

// =========================================================
// Campuses
// =========================================================

int DataService::saveCampus(
    const CampusRecord &campus
    )
{
    QSqlQuery query;

    if (campus.id > 0)
    {
        query.prepare(R"(
            UPDATE campuses
            SET
                name=?,
                building_name=?,
                address=?,
                phone_number=?,
                office_number=?,
                transit_steps=?,
                arrival_info=?,
                image_path=?,
                office_wifi=?,
                office_wifi_password=?,
                printer_name=?,
                printer_steps=?,
                photocopier_code=?,
                housing_locations=?
            WHERE id=?
        )");

        query.addBindValue(campus.name);
        query.addBindValue(campus.buildingName);
        query.addBindValue(campus.address);
        query.addBindValue(campus.phoneNumber);
        query.addBindValue(campus.officeNumber);
        query.addBindValue(campus.transitSteps);
        query.addBindValue(campus.arrivalInfo);
        query.addBindValue(campus.imagePath);
        query.addBindValue(campus.officeWifi);
        query.addBindValue(campus.officeWifiPassword);
        query.addBindValue(campus.printerName);
        query.addBindValue(campus.printerSteps);
        query.addBindValue(campus.photocopierCode);
        query.addBindValue(campus.housingLocations);
        query.addBindValue(campus.id);

        query.exec();

        return campus.id;
    }



    query.prepare(R"(
        INSERT INTO campuses (
            name,
            building_name,
            address,
            phone_number,
            office_number,
            transit_steps,
            arrival_info,
            image_path,
            office_wifi,
            office_wifi_password,
            printer_name,
            printer_steps,
            photocopier_code,
            housing_locations
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(campus.name);
    query.addBindValue(campus.buildingName);
    query.addBindValue(campus.address);
    query.addBindValue(campus.phoneNumber);
    query.addBindValue(campus.officeNumber);
    query.addBindValue(campus.transitSteps);
    query.addBindValue(campus.arrivalInfo);
    query.addBindValue(campus.imagePath);
    query.addBindValue(campus.officeWifi);
    query.addBindValue(campus.officeWifiPassword);
    query.addBindValue(campus.printerName);
    query.addBindValue(campus.printerSteps);
    query.addBindValue(campus.photocopierCode);
    query.addBindValue(campus.housingLocations);

    query.exec();

    return query.lastInsertId().toInt();
}

CampusRecord DataService::getCampus(
    int campusId
    )
{
    CampusRecord campus;

    QSqlQuery query;

    query.prepare("SELECT * FROM campuses WHERE id=?");
    query.addBindValue(campusId);
    query.exec();

    if (!query.next())
    {
        return campus;
    }

    return campusFromQuery(query);
}

QList<CampusRecord>
DataService::getAllCampuses()
{
    QList<CampusRecord> campuses;

    QSqlQuery query;

    query.exec(R"(
        SELECT *
        FROM campuses
        ORDER BY name
    )");

    while (query.next())
    {
        campuses.append(
            campusFromQuery(query)
            );
    }

    return campuses;
}

void DataService::deleteCampus(
    int campusId
    )
{
    QSqlQuery query;

    query.prepare(R"(
        DELETE FROM campuses
        WHERE id=?
    )");

    query.addBindValue(campusId);

    query.exec();
}

// =========================================================
// Manual Saving
// =========================================================

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
