#include "dataservice.h"

#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>



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



    // =====================================================
    // App Settings
    // =====================================================

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )");



    // =====================================================
    // Roster
    // =====================================================

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



    // =====================================================
    // Speaking Evaluations
    // =====================================================

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



    // =====================================================
    // TODO
    // Dynamic speaking_eval_data schema
    // =====================================================

    // query.exec(...)



    // =====================================================
    // Campuses
    // =====================================================

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



    // =====================================================
    // Teachers
    // =====================================================

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



    // =====================================================
    // Classes
    // =====================================================

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            name TEXT
        )
    )");



    // =====================================================
    // Class Info
    // =====================================================

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



    // =====================================================
    // Class Times
    // =====================================================

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )");



    // =====================================================
    // Intensive Times
    // =====================================================

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_intensive_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )");



    // =====================================================
    // Intensive Slot States
    // =====================================================

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
    const TeacherRecord &teacher
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



void DataService::updateTeacher(
    const TeacherRecord &teacher
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



QList<TeacherRecord>
DataService::getAllTeachers()
{
    QList<TeacherRecord> teachers;

    QSqlQuery query;

    query.exec(R"(
        SELECT *
        FROM teachers
        ORDER BY teacher_en
    )");

    while (query.next())
    {
        TeacherRecord teacher;

        teacher.id =
            query.value("id").toInt();

        teacher.teacherKr =
            query.value("teacher_kr")
                .toString();

        teacher.teacherEn =
            query.value("teacher_en")
                .toString();

        teacher.roomNumber =
            query.value("room_number")
                .toString();

        teacher.wifiName =
            query.value("wifi_name")
                .toString();

        teacher.wifiPassword =
            query.value("wifi_password")
                .toString();

        teacher.zoomId =
            query.value("zoom_id")
                .toString();

        teacher.zoomPassword =
            query.value("zoom_password")
                .toString();

        teacher.notes =
            query.value("notes")
                .toString();

        teachers.append(teacher);
    }

    return teachers;
}



TeacherRecord DataService::getTeacher(
    int teacherId
    )
{
    TeacherRecord teacher;

    QSqlQuery query;

    query.prepare(R"(
        SELECT *
        FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    query.exec();

    if (!query.next())
    {
        return teacher;
    }

    teacher.id =
        query.value("id").toInt();

    teacher.teacherKr =
        query.value("teacher_kr")
            .toString();

    teacher.teacherEn =
        query.value("teacher_en")
            .toString();

    return teacher;
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



QList<ClassroomRecord>
DataService::getClasses()
{
    QList<ClassroomRecord> classes;

    QSqlQuery query;

    query.exec(R"(
        SELECT *
        FROM classes
        ORDER BY name
    )");

    while (query.next())
    {
        ClassroomRecord classroom;

        classroom.id =
            query.value("id").toInt();

        classroom.name =
            query.value("name").toString();

        classes.append(classroom);
    }

    return classes;
}



ClassroomRecord DataService::getClassById(
    int classId
    )
{
    ClassroomRecord classroom;

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



void DataService::deleteClass(
    int classId
    )
{
    QSqlQuery query;



    // =====================================================
    // Classes
    // =====================================================

    query.prepare(R"(
        DELETE FROM classes
        WHERE id=?
    )");

    query.addBindValue(classId);

    query.exec();



    // =====================================================
    // Roster
    // =====================================================

    query.prepare(R"(
        DELETE FROM roster_columns
        WHERE class_id=?
    )");

    query.addBindValue(classId);

    query.exec();



    query.prepare(R"(
        DELETE FROM roster_data
        WHERE class_id=?
    )");

    query.addBindValue(classId);

    query.exec();



    // =====================================================
    // Class Info
    // =====================================================

    query.prepare(R"(
        DELETE FROM class_info
        WHERE class_id=?
    )");

    query.addBindValue(classId);

    query.exec();



    // =====================================================
    // Class Times
    // =====================================================

    query.prepare(R"(
        DELETE FROM class_times
        WHERE class_id=?
    )");

    query.addBindValue(classId);

    query.exec();



    // =====================================================
    // Intensive Times
    // =====================================================

    query.prepare(R"(
        DELETE FROM class_intensive_times
        WHERE class_id=?
    )");

    query.addBindValue(classId);

    query.exec();



    // =====================================================
    // TODO
    // Delete speaking evaluation rows
    // =====================================================
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

    query.prepare(R"(
        SELECT *
        FROM campuses
        WHERE id=?
    )");

    query.addBindValue(campusId);

    query.exec();

    if (!query.next())
    {
        return campus;
    }

    campus.id =
        query.value("id").toInt();

    campus.name =
        query.value("name")
            .toString();

    campus.address =
        query.value("address")
            .toString();

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

    QFile::copy(
        m_dbPath,
        destinationPath
        );
}



void DataService::exportAs(
    const QString &destinationPath
    )
{
    QFile::remove(destinationPath);

    QFile::copy(
        m_dbPath,
        destinationPath
        );
}