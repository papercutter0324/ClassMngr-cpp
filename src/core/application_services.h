#ifndef APPLICATIONSERVICES_H
#define APPLICATIONSERVICES_H

#include "core/result.h"
#include "features/documents/document_catalog.h"

#include <QString>
#include <memory>

class DataService;
class DatabaseSession;
class ThemeService;
class SettingsService;
class TeacherService;
class ClassService;
class ScheduleService;
class CalendarService;
class RosterService;
class SpeakingEvaluationService;

class ApplicationServices
{
public:
    ApplicationServices();

    explicit ApplicationServices(
        std::unique_ptr<ThemeService> themeService
        );

    ~ApplicationServices();

    [[nodiscard]] Status openDatabase(
        const QString& databasePath
        );

    void closeDatabase();

    [[nodiscard]] bool hasOpenDatabase() const;

    [[nodiscard]] QString currentDatabasePath() const;

    void saveDatabase();

    [[nodiscard]] Status saveDatabaseAs(
        const QString& destinationPath
        );

    [[nodiscard]] Status exportDatabaseAs(
        const QString& destinationPath
        );

    [[nodiscard]] DataService* dataService() const;
    // Session identity and narrow-service binding. Lifecycle operations must
    // use ApplicationServices to keep the compatibility facade synchronized.
    [[nodiscard]] DatabaseSession* databaseSession() const;
    [[nodiscard]] SettingsService* settingsService() const;
    [[nodiscard]] TeacherService* teacherService() const;
    [[nodiscard]] ClassService* classService() const;
    [[nodiscard]] ScheduleService* scheduleService() const;
    [[nodiscard]] CalendarService* calendarService() const;
    [[nodiscard]] RosterService* rosterService() const;
    [[nodiscard]] SpeakingEvaluationService* speakingEvaluationService() const;
    [[nodiscard]] ThemeService* themeService() const;
    [[nodiscard]] const DocumentCatalog* documentCatalog() const;

private:
    // Declared first so the session outlives the facade and all feature services.
    std::unique_ptr<DatabaseSession> m_databaseSession;
    std::unique_ptr<DataService> m_dataService;
    mutable std::unique_ptr<SettingsService> m_settingsService;
    mutable std::unique_ptr<TeacherService> m_teacherService;
    mutable std::unique_ptr<ClassService> m_classService;
    mutable std::unique_ptr<ScheduleService> m_scheduleService;
    mutable std::unique_ptr<CalendarService> m_calendarService;
    mutable std::unique_ptr<RosterService> m_rosterService;
    mutable std::unique_ptr<SpeakingEvaluationService> m_speakingEvaluationService;
    std::unique_ptr<ThemeService> m_themeService;
    std::unique_ptr<DocumentCatalog> m_documentCatalog;
};

#endif // APPLICATIONSERVICES_H
