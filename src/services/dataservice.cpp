#include "dataservice.h"

#include <QDebug>
#include <QDir>
#include <QFile>
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

    // TODO
    // Dynamic speaking_eval_data schema

    // query.exec(...)

    // Campuses
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS campuses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            name TEXT NOT NULL,

            building_name TEXT,
            address TEXT,
            phone_number TEXT,

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

    // Teachers
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            teacher_kr TEXT,
            teacher_en TEXT,

            room_number TEXT,

            wifi_name TEXT,
            wifi_password TEXT,

            zoom_id TEXT,
            zoom_password TEXT,

            notes TEXT
        )
    )");

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

            notes TEXT
        )
    )");

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
            zoom_id,
            zoom_password,
            notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
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
            zoom_id=?,
            zoom_password=?,
            notes=?
        WHERE id=?
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(teacher.notes);
    query.addBindValue(teacher.id);

    query.exec();
}

Teacher DataService::getTeacher(
    int teacherId
    )
{
    Teacher teacher;

    QSqlQuery query;

    query.prepare(R"(
        SELECT *
        FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    if (!query.exec() || !query.next())
    {
        return teacher; // returns empty/default teacher
    }

    teacher.id =           query.value("id").toInt();
    teacher.teacherKr =    query.value("teacher_kr").toString();
    teacher.teacherEn =    query.value("teacher_en").toString();
    teacher.roomNumber =   query.value("room_number").toString();
    teacher.wifiName =     query.value("wifi_name").toString();
    teacher.wifiPassword = query.value("wifi_password").toString();
    teacher.zoomId =       query.value("zoom_id").toString();
    teacher.zoomPassword = query.value("zoom_password").toString();
    teacher.notes =        query.value("notes").toString();

    return teacher;
}

QList<Teacher>
DataService::getAllTeachers()
{
    QList<Teacher> teachers;

    QSqlQuery query;

    query.exec(R"(
        SELECT *
        FROM teachers
        ORDER BY teacher_en
    )");

    while (query.next())
    {
        Teacher teacher;

        teacher.id =           query.value("id").toInt();
        teacher.teacherKr =    query.value("teacher_kr").toString();
        teacher.teacherEn =    query.value("teacher_en").toString();
        teacher.roomNumber =   query.value("room_number").toString();
        teacher.wifiName =     query.value("wifi_name").toString();
        teacher.wifiPassword = query.value("wifi_password").toString();
        teacher.zoomId =       query.value("zoom_id").toString();
        teacher.zoomPassword = query.value("zoom_password").toString();
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
            notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id,
            class_grade=excluded.class_grade,
            class_level=excluded.class_level,
            reading_book=excluded.reading_book,
            essay_book=excluded.essay_book,
            class_color=excluded.class_color,
            font_color=excluded.font_color,
            notes=excluded.notes
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

    QSqlQuery query;

    query.exec(R"(
        SELECT *
        FROM classes
        ORDER BY name
    )");

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

    QSqlQuery query;

    // Main Class Info + Teacher Join
    query.prepare(R"(
        SELECT
            ci.*,

            t.teacher_kr,
            t.teacher_en,
            t.room_number,
            t.wifi_name,
            t.wifi_password,
            t.zoom_id,
            t.zoom_password

        FROM class_info ci

        LEFT JOIN teachers t
        ON ci.teacher_id = t.id

        WHERE ci.class_id = ?
    )");

    query.addBindValue(classId);
    query.exec();

    if (query.next())
    {
        info.teacherId =    query.value("teacher_id").toInt();
        info.teacherKr =    query.value("teacher_kr").toString();
        info.teacherEn =    query.value("teacher_en").toString();
        info.roomNumber =   query.value("room_number").toString();
        info.wifiName =     query.value("wifi_name").toString();
        info.wifiPassword = query.value("wifi_password").toString();
        info.zoomId =       query.value("zoom_id").toString();
        info.zoomPassword = query.value("zoom_password").toString();
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
    }

    // Regular Times
    query.prepare(R"(
        SELECT *
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");

    query.addBindValue(classId);
    query.exec();

    while (query.next())
    {
        ClassTime time;

        time.day =       query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime =   query.value("end_time").toString();

        info.classTimes.append(time);
    }

    // Intensive Times
    query.prepare(R"(
        SELECT *
        FROM class_intensive_times
        WHERE class_id = ?
        ORDER BY id
    )");

    query.addBindValue(classId);

    query.exec();

    while (query.next())
    {
        ClassTime time;

        time.day =       query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime =   query.value("end_time").toString();

        info.intensiveTimes.append(time);
    }

    return info;
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
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(campus.name);
    query.addBindValue(campus.buildingName);
    query.addBindValue(campus.address);
    query.addBindValue(campus.phoneNumber);
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

    campus.id =      query.value("id").toInt();
    campus.name =    query.value("name").toString();
    campus.address = query.value("address").toString();

    return campus;
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
        CampusRecord campus;

        campus.id =
            query.value("id").toInt();

        campus.name =
            query.value("name")
                .toString();

        campuses.append(campus);
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
