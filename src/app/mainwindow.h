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

class Sidebar;
class PageManager;

class SidebarController;
class NavigationController;
class EditController;
class ThemeController;
class LanguageController;
class LanguageService;

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

    bool confirmCurrentPageCanLeave(
        bool exiting = false
        ) const;

    void applyNoDatabaseState();

    void applyDatabaseLoadedState();
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

    void initializePages();
    void initializeSidebar();

    void createActions();
    void connectControllers();
    void buildMenus();

    void connectSignals();

    void restoreSplitter();

    void setDatabaseBackedActionsEnabled(
        bool enabled
        );

    // =====================================================
    // UI
    // =====================================================

    Ui::MainWindow* ui = nullptr;

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
    std::unique_ptr<LanguageController> m_languageController;

    // =====================================================
    // State
    // =====================================================

    bool m_isAdmin = false;
    LanguageService* m_languageService = nullptr;
};

#endif // MAINWINDOW_H
