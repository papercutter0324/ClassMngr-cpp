#ifndef APPLICATIONSERVICES_H
#define APPLICATIONSERVICES_H

#include "core/result.h"
#include "features/documents/document_catalog.h"
#include "data/repositories/repository_error.h"
#include "data/database/workspace_store_fwd.h"

#include <QString>
#include <memory>

class DataService;
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

    [[nodiscard]] RepositoryStatus openDatabase(
        const QString& databasePath
        );

    // Opens a workspace that already exists. Unlike openDatabase(), this path
    // never creates a database file or its parent directories.
    [[nodiscard]] RepositoryStatus openExistingDatabase(
        const QString& databasePath
        );

    void closeDatabase();

    [[nodiscard]] bool hasOpenDatabase() const;

    [[nodiscard]] QString currentDatabasePath() const;

    void saveDatabase();

    [[nodiscard]] RepositoryStatus saveDatabaseAs(
        const QString& destinationPath
        );

    [[nodiscard]] RepositoryStatus exportDatabaseAs(
        const QString& destinationPath
        );

    [[nodiscard]] DataService* dataService() const;
    [[nodiscard]] WorkspaceStore* workspaceStore() const;
    // Transitional compatibility accessor for the canonical workspace store.
    // It returns the same object as workspaceStore().
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
    // Declared first so workspace persistence outlives all service facades.
    std::unique_ptr<WorkspaceStore> m_workspaceStore;
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
