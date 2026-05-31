#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <functional>



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
        QWidget *parent = nullptr
        );

    ~MainWindow() override;



protected:

    void closeEvent(
        QCloseEvent *event
        ) override;



private:

    // =====================================================
    // Initialization
    // =====================================================

    void initializePages();

    void initializeSidebar();

    void initializeControllers();

    void connectSignals();

    void restoreSplitter();



    // =====================================================
    // UI
    // =====================================================

    // =====================================================
    // Services
    // =====================================================

    std::unique_ptr<ApplicationServices> m_services;



    // =====================================================
    // Core Widgets
    // =====================================================

    Sidebar *m_sidebar = nullptr;

    PageManager *m_pages = nullptr;



    // =====================================================
    // Controllers
    // =====================================================

    SidebarController *m_sidebarController = nullptr;

    NavigationController *m_navigationController = nullptr;



    // =====================================================
    // State
    // =====================================================

    bool m_isAdmin = false;
};



#endif // MAINWINDOW_H