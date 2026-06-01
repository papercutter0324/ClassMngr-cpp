#include "mainwindow.h"
#include "ui_mainwindow.h"


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
    QWidget *parent
)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isAdmin(isAdmin)
{
    ui->setupUi(this);



    // =====================================================
    // Services
    // =====================================================

    m_services =
        std::make_unique<ApplicationServices>(
            AppSettings::DefaultDatabasePath
        );



    // =====================================================
    // Window Setup
    // =====================================================

    setWindowTitle(AppSettings::ApplicationName);



    // =====================================================
    // Initial UI Configuration
    // =====================================================

    ui->splitter->setChildrenCollapsible(false);

    ui->sidebarWidget->setMinimumWidth(
        UiConstants::MainWindow::SidebarMinWidth);

    ui->sidebarWidget->setMaximumWidth(
        UiConstants::MainWindow::SidebarMaxWidth);

    ui->pagesWidget->setMinimumWidth(
        UiConstants::MainWindow::PagesMinWidth);



    // =====================================================
    // Initialize Components
    // =====================================================

    progressCallback(tr("Loading pages..."));

    initializePages();

    progressCallback(tr("Loading sidebar..."));

    initializeSidebar();



    // =====================================================
    // Controllers
    // =====================================================

    progressCallback(tr("Initializing controllers..."));

    initializeControllers();



    // =====================================================
    // Signals
    // =====================================================

    progressCallback(tr("Connecting signals..."));

    connectSignals();



    // =====================================================
    // Restore Splitter
    // =====================================================

    QTimer::singleShot(
        0,
        this,
        &MainWindow::restoreSplitter
        );
}



void MainWindow::showEvent(
    QShowEvent* event
    )
{
    QMainWindow::showEvent(event);
}



// =========================================================
// Destructor
// =========================================================

MainWindow::~MainWindow()
{
    delete ui;
}



// =========================================================
// Initialization
// =========================================================

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


void MainWindow::initializeControllers()
{
    // TODO
}


void MainWindow::connectSignals()
{
    // TODO
}



// =========================================================
// Splitter Persistence
// =========================================================

void MainWindow::restoreSplitter()
{
    QSettings settings;

    ui->splitter->restoreState(
        settings.value("splitterState")
            .toByteArray()
        );
}


void MainWindow::closeEvent(
    QCloseEvent *event
    )
{
    QSettings settings;

    settings.setValue(
        "splitterState",
        ui->splitter->saveState()
        );

    QMainWindow::closeEvent(event);
}