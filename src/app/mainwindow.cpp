#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui/constants/gui_constants.h"

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
    // Window Setup
    // =====================================================

    setWindowTitle(tr("ClassMngr"));



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
    // TODO
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
    QSettings settings(
        "PaperCloud",
        "ClassMngr"
        );

    QList<int> sizes =
        settings.value("splitterSizes")
            .value<QList<int>>();

    if (!sizes.isEmpty())
    {
        ui->splitter->setSizes(sizes);
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings(
        "PaperCloud",
        "ClassMngr"
        );

    settings.setValue(
        "splitterSizes",
        QVariant::fromValue(
            ui->splitter->sizes()
            )
        );

    QMainWindow::closeEvent(event);
}