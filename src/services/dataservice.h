#pragma once

#include "models/campus.h"
#include "models/class_info.h"
#include "models/class_conflict.h"
#include "models/classroom.h"
#include "models/roster.h"
#include "models/speaking_evaluation.h"
#include "models/teacher.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>



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

    void saveClassInfo(
        const ClassInfo& info
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

    // TODO:
    // Port loadIntensiveSlotStates()
    // Port saveIntensiveSlotState()



    // =====================================================
    // Conflict Detection
    // =====================================================

    QList<ClassConflict> getClassTimeConflicts(
        int classId
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

    void saveSpeakingEvaluations(
        int classId,
        const SpeakingEvaluations& evaluations
        );

    SpeakingEvaluations loadSpeakingEvaluations(
        int classId
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