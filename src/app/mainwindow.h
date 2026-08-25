#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app/controllers/file_controller.h"
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
class MemoryUsageDialog;

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

struct MainWindowStartupOptions
{
    bool loadMostRecentDatabase = true;
    bool runPostShowStartupTasks = true;
    QString initialDatabasePath;
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

    [[nodiscard]] CalendarPage* calendarPage() const;
    [[nodiscard]] CalendarPage* ensureCalendarPage() const;

    void refreshSchedulePreferences();

    void refreshNavigationPreferences();

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

    void setDefaultSidebarWidth();
    void reapplyStartupFontSize();
    void showStartupDatabasePage();
    void startInitialSetup();

    void setDatabaseBackedActionsEnabled(
        bool enabled
        );

    void updatePrintExportActions();
    void showMemoryUsageMonitor();

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
    std::unique_ptr<MemoryUsageDialog> m_memoryUsageDialog;

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
    bool m_startupFontSizeRefreshQueued = false;
    bool m_startupBirthdayCheckQueued = false;
    bool m_startupSidebarWidthApplied = false;
    MainWindowStartupOptions m_startupOptions;
    LanguageService* m_languageService = nullptr;
};

#endif // MAINWINDOW_H
