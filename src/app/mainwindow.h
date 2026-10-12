#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app/controllers/file_controller.h"
#include "core/theme_service.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QMainWindow>
#include <QShowEvent>
#include <QCloseEvent>

#include <functional>
#include <memory>



QT_BEGIN_NAMESPACE

namespace Ui
{
class MainWindow;
}

QT_END_NAMESPACE



// =========================================================
// Forward Declarations
// =========================================================

class ApplicationServices;
class CalendarPage;

class Sidebar;
class PageManager;

class SidebarController;
class NavigationController;
class EditController;
class ThemeController;
class UpdateController;
class LanguageController;
class FontSizeController;
class LanguageService;

namespace ClassMngr::Next::Application
{
class AutomaticUpdatePreferencesPort;
class AiCommentProviderPreferencesPort;
class AiCommentVoicePreferencesPort;
class DocumentViewerBackgroundPreferencesPort;
class ExcelImportTimeoutPreferencesPort;
class FontSizePreferencesPort;
class LanguagePreferencesPort;
class SaveModePreferencesPort;
class SidebarDisplayPreferencesPort;
}

namespace ClassMngr::Next::Domain
{
struct OperationError;
}

struct MainWindowStartupOptions
{
    bool loadMostRecentDatabase = true;
    QString initialDatabasePath;
    std::unique_ptr<ThemeService> startupThemeService;
    std::function<void(const QString&, const QString&)> checkpointCallback;
    std::shared_ptr<
        ClassMngr::Next::Application::ExcelImportTimeoutPreferencesPort
        > excelImportTimeoutPreferencesPort;
    std::shared_ptr<
        ClassMngr::Next::Application::SaveModePreferencesPort
        > saveModePreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> saveModePreferenceErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::AutomaticUpdatePreferencesPort
        > automaticUpdatePreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> automaticUpdatePreferenceErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::FontSizePreferencesPort
        > fontSizePreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> fontSizePreferenceErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::LanguagePreferencesPort
        > languagePreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> languagePreferenceErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::SidebarDisplayPreferencesPort
        > sidebarDisplayPreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> sidebarDisplayPreferencesErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::DocumentViewerBackgroundPreferencesPort
        > documentViewerBackgroundPreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> documentViewerBackgroundPreferenceErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::AiCommentProviderPreferencesPort
        > aiCommentProviderPreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> aiCommentProviderPreferenceErrorHandler;
    std::shared_ptr<
        ClassMngr::Next::Application::AiCommentVoicePreferencesPort
        > aiCommentVoicePreferencesPort;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> aiCommentVoicePreferenceErrorHandler;
};

// =========================================================
// Main Window
// =========================================================

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    explicit MainWindow(
        std::function<void(const QString&)> progressCallback,
        bool isAdmin,
        LanguageService* languageService,
        UpdateController* updateController,
        MainWindowStartupOptions startupOptions = {},
        QWidget *parent = nullptr
        );

    ~MainWindow() override;

    // Change to: const ActionRegistry& actions() const; ??
    ActionRegistry& actions()
    {
        return m_actions;
    }

    bool isAdmin() const {
        return m_isAdmin;
    }

    [[nodiscard]] ApplicationServices* services() const;
    [[nodiscard]] PageManager* pageManager() const;

    [[nodiscard]] CalendarPage* calendarPage() const;
    [[nodiscard]] CalendarPage* ensureCalendarPage() const;

    void refreshSchedulePreferences();

    void refreshNavigationPreferences();

    void attachUpdateController(
        UpdateController* updateController
        );

    bool confirmCurrentPageCanLeave(
        bool exiting = false
        ) const;

    void applyNoDatabaseState();

    void applyDatabaseLoadedState();
    void clearDatabaseBackedState();
    void retranslateUi();

protected:

    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:

    // =====================================================
    // Initialization
    // =====================================================

    void initializeServices();
    void initializeWindow();
    void adjustSizeForAvailableScreen();

    void initializePages();
    void initializeSidebar();

    void createActions();
    void connectControllers();
    void buildMenus();

    void connectSignals();

    void recordStartupCheckpoint(
        const QString& name,
        const QString& detail = QString()
        );

    void setDefaultSidebarWidth();
    void startInitialSetup();

    void setDatabaseBackedActionsEnabled(
        bool enabled
        );

    void updatePrintExportActions();

    // =====================================================
    // UI
    // =====================================================

    std::unique_ptr<Ui::MainWindow> ui;

    ActionRegistry m_actions;
    std::unique_ptr<FileController> m_fileController;

    void onSidebarItemSelected(
        const NavigationData& data
    );

    // =====================================================
    // Services
    // =====================================================

    std::unique_ptr<ApplicationServices> m_services;

    // =====================================================
    // Core Widgets
    // =====================================================

    Sidebar* m_sidebar = nullptr;
    PageManager* m_pages = nullptr;

    // =====================================================
    // Controllers
    // =====================================================

    std::unique_ptr<SidebarController> m_sidebarController;
    std::unique_ptr<NavigationController> m_navigationController;
    std::unique_ptr<EditController> m_editController;
    std::unique_ptr<ThemeController> m_themeController;
    UpdateController* m_updateController = nullptr;
    std::unique_ptr<LanguageController> m_languageController;
    std::unique_ptr<FontSizeController> m_fontSizeController;

    // =====================================================
    // State
    // =====================================================

    bool m_isAdmin = false;
    bool m_testingClassesReturnToPersonalSchedule = true;
    bool m_startupSidebarWidthApplied = false;
    MainWindowStartupOptions m_startupOptions;
    LanguageService* m_languageService = nullptr;
};

#endif // MAINWINDOW_H
