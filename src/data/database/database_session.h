#pragma once

#include "core/result.h"

#include <QSqlDatabase>
#include <QString>

#include <memory>

class CalendarEventRepository;
class CampusRecordRepository;
class ClassInfoRepository;
class ClassRepository;
class ClassTransferRepository;
class GsTeamRepository;
class IntensiveSlotStateRepository;
class NativeEnglishTeacherRepository;
class RosterRepository;
class ScheduleImportRepository;
class SettingsRepository;
class SpeakingEvalRepository;
class TeacherImportRepository;
class TeacherRepository;
class TestingBlockRepository;
class TestingClassRepository;

class DatabaseSession final
{
public:
    DatabaseSession();
    ~DatabaseSession();

    DatabaseSession(const DatabaseSession&) = delete;
    DatabaseSession& operator=(const DatabaseSession&) = delete;

    [[nodiscard]] Status open(const QString& databasePath);
    void close();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString databasePath() const;
    [[nodiscard]] QSqlDatabase database() const;

    SettingsRepository* settingsRepository() const;
    CampusRecordRepository* campusRecordRepository() const;
    TeacherRepository* teacherRepository() const;
    NativeEnglishTeacherRepository* nativeEnglishTeacherRepository() const;
    GsTeamRepository* gsTeamRepository() const;
    TeacherImportRepository* teacherImportRepository() const;
    ClassRepository* classRepository() const;
    ClassTransferRepository* classTransferRepository() const;
    ScheduleImportRepository* scheduleImportRepository() const;
    ClassInfoRepository* classInfoRepository() const;
    IntensiveSlotStateRepository* intensiveSlotStateRepository() const;
    TestingBlockRepository* testingBlockRepository() const;
    TestingClassRepository* testingClassRepository() const;
    CalendarEventRepository* calendarEventRepository() const;
    RosterRepository* rosterRepository() const;
    SpeakingEvalRepository* speakingEvalRepository() const;

private:
    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;

    std::unique_ptr<SettingsRepository> m_settingsRepository;
    std::unique_ptr<CampusRecordRepository> m_campusRecordRepository;
    std::unique_ptr<TeacherRepository> m_teacherRepository;
    std::unique_ptr<NativeEnglishTeacherRepository> m_nativeEnglishTeacherRepository;
    std::unique_ptr<GsTeamRepository> m_gsTeamRepository;
    std::unique_ptr<TeacherImportRepository> m_teacherImportRepository;
    std::unique_ptr<ClassRepository> m_classRepository;
    std::unique_ptr<ClassTransferRepository> m_classTransferRepository;
    std::unique_ptr<ScheduleImportRepository> m_scheduleImportRepository;
    std::unique_ptr<ClassInfoRepository> m_classInfoRepository;
    std::unique_ptr<IntensiveSlotStateRepository> m_intensiveSlotStateRepository;
    std::unique_ptr<TestingBlockRepository> m_testingBlockRepository;
    std::unique_ptr<TestingClassRepository> m_testingClassRepository;
    std::unique_ptr<CalendarEventRepository> m_calendarEventRepository;
    std::unique_ptr<RosterRepository> m_rosterRepository;
    std::unique_ptr<SpeakingEvalRepository> m_speakingEvalRepository;
};
