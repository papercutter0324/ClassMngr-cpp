#ifndef DATASERVICE_H
#define DATASERVICE_H

#include "models/teacher.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>



// =========================================================
// Classroom Record
// =========================================================

struct ClassroomRecord
{
    int id = -1;

    QString name;
};



// =========================================================
// Campus Record
// =========================================================

struct CampusRecord
{
    int id = -1;

    QString name;

    QString buildingName;
    QString address;
    QString phoneNumber;

    QString transitSteps;
    QString arrivalInfo;

    QString imagePath;

    QString officeWifi;
    QString officeWifiPassword;

    QString printerName;
    QString printerSteps;

    QString photocopierCode;

    QString housingLocations;
};



// =========================================================
// Data Service
// =========================================================

class DataService
{
public:

    explicit DataService(
        const QString &dbPath = "data/app.db"
        );

    ~DataService();



    // =====================================================
    // Setup
    // =====================================================

    bool open();

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
        const QString &name
        );

    QList<ClassroomRecord> getClasses();

    ClassroomRecord getClassById(
        int classId
        );

    void updateClassName(
        int classId,
        const QString &name
        );

    void deleteClass(
        int classId
        );



    // =====================================================
    // Class Info
    // =====================================================

    // TODO:
    // Port saveClassInfo()
    // Port loadClassInfo()
    // Port class time management
    // Port intensive time management



    // =====================================================
    // Intensive Slot States
    // =====================================================

    // TODO:
    // Port loadIntensiveSlotStates()
    // Port saveIntensiveSlotState()



    // =====================================================
    // Conflict Detection
    // =====================================================

    // TODO:
    // Port getClassTimeConflicts()
    // Port time parsing helpers



    // =====================================================
    // Roster
    // =====================================================

    // TODO:
    // Port saveRoster()
    // Port loadRoster()
    // Port buildRosterScoreImport()
    // Port getRosterStudentCount()



    // =====================================================
    // Speaking Evaluations
    // =====================================================

    // TODO:
    // Port saveSpeakingEval()
    // Port loadSpeakingEval()



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

    void saveAs(
        const QString &destinationPath
        );

    void exportAs(
        const QString &destinationPath
        );



private:

    QString m_dbPath;

    QSqlDatabase m_db;
};



#endif // DATASERVICE_H