#pragma once

#include "core/result.h"

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

// Session around the portable engine database pipeline. File-backed opens are
// normalized, migrated, and validated by the engine before retained Qt-facing
// repositories are made available. Qt SQL compatibility fixtures must open
// their own explicit connection; the session does not own a Qt connection.
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
    [[nodiscard]] bool isEngineBacked() const;
    [[nodiscard]] QString databasePath() const;

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
