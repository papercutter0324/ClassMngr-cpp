#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "menu_builder.h"


#include "controllers/navigation_controller.h"
#include "core/application_services.h"
#include "core/appsettings.h"
#include "ui/constants/gui_constants.h"
#include "ui/pages/pagemanager.h"

#include <QSettings>
#include <QTimer>



// =========================================================
// Constructor
// =========================================================

MainWindow::MainWindow(
    std::function<void(const QString&)> progressCallback,
    bool isAdmin,
    QWidget* parent
    )
    : QMainWindow(parent)
    , ui(new Ui::MainWindow())
    , m_isAdmin(isAdmin)
{
    ui->setupUi(this);

    initializeServices();

    initializeWindow();

    progressCallback(tr("Loading pages..."));
    initializePages();

    progressCallback(tr("Loading sidebar..."));
    initializeSidebar();

    progressCallback(tr("Initializing actions..."));
    createActions();

    progressCallback(tr("Initializing controllers..."));
    connectControllers();

    progressCallback(tr("Building menus..."));
    buildMenus();

    progressCallback(tr("Connecting signals..."));
    connectSignals();

    QTimer::singleShot(
        0,
        this,
        &MainWindow::restoreSplitter
        );
}

void MainWindow::initializeServices()
{
    m_services =
        std::make_unique<ApplicationServices>(
            AppSettings::DefaultDatabasePath
            );
}

void MainWindow::initializeWindow()
{
    setWindowTitle(AppSettings::ApplicationName);

    ui->splitter->setChildrenCollapsible(false);

    ui->sidebarWidget->setMinimumWidth(
        UiConstants::MainWindow::SidebarMinWidth);

    ui->sidebarWidget->setMaximumWidth(
        UiConstants::MainWindow::SidebarMaxWidth);

    ui->pagesWidget->setMinimumWidth(
        UiConstants::MainWindow::PagesMinWidth);
}

void MainWindow::initializePages()
{
    m_pages = ui->pagesWidget;

    m_pages->initialize(
        m_services.get()
        );
}

void MainWindow::initializeSidebar()
{
    // TODO
}

void MainWindow::createActions()
{
    m_actions.createActions();
}

void MainWindow::connectControllers()
{
    m_sidebarController =
        std::make_unique<SidebarController>(
            m_services.get(),
            ui->sidebarWidget,
            m_pages,
            this
            );
    m_sidebarController->connectActions(m_actions);

    m_fileController =
        std::make_unique<FileController>(
            m_services.get(),
            this
            );
    m_fileController->connectActions(m_actions);

    // future:
    // m_editController.connectActions(m_actions);
    // m_sidebarController.connectActions(m_actions);
    // m_themeController.connectActions(m_actions);
}

void MainWindow::buildMenus()
{
    MenuBuilder::build(this);
}

void MainWindow::connectSignals()
{
    connect(
        ui->sidebarWidget,
        &Sidebar::itemSelected,
        m_navigationController, // this,
        &NavigationController::handleNavigation // &MainWindow::onSidebarItemSelected
        );
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
}

void MainWindow::onSidebarItemSelected(
    const NavigationData& data)
{
    m_pages->showPage(
        PageType::TeacherInfo
        );

    /*if (data.type == NodeType::Teacher)
    {
        m_pages->showPage(PageType::TeacherInfo);
        return;
    }

    if (data.type == NodeType::Class)
    {
        m_pages->showPage(PageType::ClassInfo);
        return;
    }*/
}

// =========================================================
// Destructor
// =========================================================

MainWindow::~MainWindow() = default;



// =========================================================
// Splitter Persistence
// =========================================================

void MainWindow::restoreSplitter()
{
    QSettings settings;

    ui->splitter->restoreState(
        settings.value("splitterState").toByteArray()
    );
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;

    settings.setValue(
        "splitterState",
        ui->splitter->saveState()
    );

    QMainWindow::closeEvent(event);
}