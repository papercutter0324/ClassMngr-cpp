#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "menu_builder.h"

#include "app/controllers/edit_controller.h"
#include "app/controllers/font_size_controller.h"
#include "app/controllers/language_controller.h"
#include "app/controllers/navigation_controller.h"
#include "app/controllers/sidebar_controller.h"
#include "app/controllers/theme_controller.h"
#include "app/controllers/update_controller.h"
#include "core/application_services.h"
#include "core/appsettings.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/classes/ui/classes_page.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/roster/ui/rosters_page.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/dialogs/about_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QDialog>
#include <QLayout>
#include <QMenuBar>
#include <QScreen>
#include <QTimer>

// =========================================================
// Constructor
// =========================================================

MainWindow::MainWindow(
    std::function<void(const QString&)> progressCallback,
    bool isAdmin,
    LanguageService* languageService,
    UpdateController* updateController,
    MainWindowStartupOptions startupOptions,
    QWidget* parent
    )
    : QMainWindow(parent)
    , ui(new Ui::MainWindow())
    , m_isAdmin(isAdmin)
    , m_startupOptions(startupOptions)
    , m_languageService(languageService)
    , m_updateController(updateController)
{
    progressCallback(tr("Creating main window..."));
    ui->setupUi(this);

    progressCallback(tr("Initializing application services..."));
    initializeServices();

    progressCallback(tr("Configuring window..."));
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
    m_fileController->populateRecentMenu();

    progressCallback(tr("Connecting signals..."));
    connectSignals();

    progressCallback(tr("Loading recent file..."));
    if (
        !m_startupOptions
            .initialDatabasePath
            .trimmed()
            .isEmpty()
        )
    {
        m_fileController->loadDatabaseOnStartup(
            m_startupOptions.initialDatabasePath
            );
        showStartupDatabasePage();
    }
    else if (m_startupOptions.loadMostRecentDatabase)
    {
        m_fileController->loadMostRecentDatabase();
        showStartupDatabasePage();
    }
    else
    {
        applyNoDatabaseState();
    }

    progressCallback(tr("Restoring window layout..."));
    restoreSplitter();
}

ApplicationServices* MainWindow::services() const
{
    return m_services.get();
}

CalendarPage* MainWindow::calendarPage() const
{
    return m_pages
        ? m_pages->calendarPage()
        : nullptr;
}

void MainWindow::refreshSchedulePreferences()
{
    if (m_pages)
    {
        m_pages->refreshSchedulePreferences();
    }
}

void MainWindow::refreshNavigationPreferences()
{
    if (m_pages)
    {
        m_pages->refreshNavigationPreferences();
    }
}

void MainWindow::initializeServices()
{
    m_services =
        std::make_unique<ApplicationServices>();
}

void MainWindow::initializeWindow()
{
    QString windowTitle =
        QString::fromUtf8(AppSettings::ApplicationName);

    if (m_isAdmin)
    {
        windowTitle += tr(" [ADMIN]");
    }

    setWindowTitle(windowTitle);

    ui->splitter->setChildrenCollapsible(false);

    ui->sidebarWidget->setMinimumWidth(
        UiConstants::MainWindow::SidebarMinWidth);

    ui->sidebarWidget->setMaximumWidth(
        UiConstants::MainWindow::SidebarMaxWidth);

    ui->pagesWidget->setMinimumWidth(
        UiConstants::MainWindow::PagesMinWidth);

    adjustSizeForAvailableScreen();
}

void MainWindow::adjustSizeForAvailableScreen()
{
    QScreen* availableScreen =
        screen();

    if (!availableScreen)
    {
        return;
    }

    const QSize defaultWindowSize =
        size();

    const QSize availableScreenSize =
        availableScreen
            ->availableGeometry()
            .size();

    if (
        availableScreenSize.width()
            < defaultWindowSize.width()
        || availableScreenSize.height()
            < defaultWindowSize.height()
        )
    {
        resize(minimumSize());
    }
}

void MainWindow::initializePages()
{
    m_pages = ui->pagesWidget;

    m_pages->initialize(
        m_services.get(),
        m_isAdmin
        );
}

void MainWindow::initializeSidebar()
{
    if (!ui || !ui->sidebarWidget)
    {
        return;
    }

    ui->sidebarWidget->setDocumentCatalog(
        m_services
            ? m_services->documentCatalog()
            : nullptr,
        m_languageService
            ? m_languageService->loadedLocaleName()
            : QString()
        );
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

    m_navigationController =
        std::make_unique<NavigationController>(
            m_services.get(),
            ui->sidebarWidget,
            m_pages,
            this
            );

    m_fileController =
        std::make_unique<FileController>(
            m_services.get(),
            this
            );
    m_fileController->connectActions(m_actions);

    m_editController =
        std::make_unique<EditController>(
            this
            );
    m_editController->connectActions(m_actions);

    m_themeController =
        std::make_unique<ThemeController>(
            m_services->themeService(),
            this
            );
    m_themeController->connectActions(m_actions);

    if (m_updateController)
    {
        m_updateController->attachMainWindow(
            this,
            m_actions
            );
    }

    m_languageController =
        std::make_unique<LanguageController>(
            m_languageService,
            this,
            this
            );
    m_languageController->connectActions(m_actions);

    m_fontSizeController =
        std::make_unique<FontSizeController>(
            m_languageService,
            this
            );
    m_fontSizeController->connectActions(m_actions);
}

void MainWindow::buildMenus()
{
    MenuBuilder::build(this);
}

void MainWindow::retranslateUi()
{
    const QStringList selectedKeys =
        ui && ui->sidebarWidget
            ? ui->sidebarWidget->selectedKeys()
            : QStringList();

    const int selectedClassId =
        ui && ui->sidebarWidget
            ? ui->sidebarWidget->getSelectedClassId()
            : -1;

    const int selectedTeacherId =
        ui && ui->sidebarWidget
            ? ui->sidebarWidget->getSelectedTeacherId()
            : -1;

    const QStringList expandedRootKeys =
        ui && ui->sidebarWidget
            ? ui->sidebarWidget->expandedRootKeys()
            : QStringList();

    initializeWindow();

    m_actions.retranslate();

    if (menuBar())
    {
        menuBar()->clear();
        buildMenus();
        updatePrintExportActions();
    }

    if (m_fileController)
    {
        m_fileController->populateRecentMenu();
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->setDocumentCatalog(
            m_services
                ? m_services->documentCatalog()
                : nullptr,
            m_languageService
                ? m_languageService->loadedLocaleName()
                : QString()
            );
    }

    if (m_sidebarController)
    {
        m_sidebarController->refreshAllSidebars();
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->restoreExpandedRootKeys(
            expandedRootKeys
            );

        ui->sidebarWidget->setDatabaseSectionsVisible(
            m_services && m_services->hasOpenDatabase()
            );

        ui->sidebarWidget->selectByKeys(
            selectedKeys,
            selectedClassId,
            selectedTeacherId
            );
    }

    if (m_pages)
    {
        m_pages->retranslatePages();
    }
}

void MainWindow::connectSignals()
{
    if (m_pages && m_actions.printCurrentPage)
    {
        connect(
            m_actions.printCurrentPage,
            &QAction::triggered,
            m_pages,
            &PageManager::printCurrentPage
            );
    }

    if (m_pages && m_actions.saveCurrentPageAs)
    {
        connect(
            m_actions.saveCurrentPageAs,
            &QAction::triggered,
            m_pages,
            &PageManager::saveCurrentPageAs
            );
    }

    if (m_pages)
    {
        connect(
            m_pages,
            &PageManager::outputCapabilitiesChanged,
            this,
            &MainWindow::updatePrintExportActions
            );
    }

    updatePrintExportActions();

    if (m_pages)
    {
        connect(
            m_pages,
            &PageManager::initialSetupRequested,
            this,
            &MainWindow::startInitialSetup
            );
    }

    if (m_pages && m_actions.openFile)
    {
        connect(
            m_pages,
            &PageManager::openDatabaseRequested,
            m_actions.openFile,
            &QAction::trigger
            );
    }

    if (m_pages && m_actions.newFile)
    {
        connect(
            m_pages,
            &PageManager::newDatabaseRequested,
            m_actions.newFile,
            &QAction::trigger
            );
    }

    if (m_actions.saveModeState)
    {
        const auto previousSaveModeHandler =
            m_actions.saveModeState->onChanged;

        m_actions.saveModeState->onChanged =
            [this, previousSaveModeHandler](SaveMode mode)
        {
            if (previousSaveModeHandler)
            {
                previousSaveModeHandler(mode);
            }

            if (m_pages)
            {
                m_pages->setSaveMode(mode);
            }
        };

        m_actions.saveModeState->onChanged(
            m_actions.saveModeState->current()
            );
    }

    if (m_actions.documentPageSpacingState)
    {
        const auto previousDocumentPageSpacingHandler =
            m_actions.documentPageSpacingState->onChanged;

        m_actions.documentPageSpacingState->onChanged =
            [this, previousDocumentPageSpacingHandler](
                DocumentPageSpacing spacing
                )
        {
            if (previousDocumentPageSpacingHandler)
            {
                previousDocumentPageSpacingHandler(spacing);
            }

            if (m_pages)
            {
                m_pages->setDocumentPageSpacing(spacing);
            }
        };

        m_actions.documentPageSpacingState->onChanged(
            m_actions.documentPageSpacingState->current()
            );
    }

    if (m_actions.documentViewerBackgroundState)
    {
        const auto previousDocumentViewerBackgroundHandler =
            m_actions.documentViewerBackgroundState->onChanged;

        m_actions.documentViewerBackgroundState->onChanged =
            [this, previousDocumentViewerBackgroundHandler](
                DocumentViewerBackground background
                )
        {
            if (previousDocumentViewerBackgroundHandler)
            {
                previousDocumentViewerBackgroundHandler(background);
            }

            if (m_pages)
            {
                m_pages->setDocumentViewerBackground(background);
            }
        };

        m_actions.documentViewerBackgroundState->onChanged(
            m_actions.documentViewerBackgroundState->current()
            );
    }

    ui->sidebarWidget->setOverflowTooltipsEnabled(
        m_actions.showSidebarTooltips->isChecked()
        );

    ui->sidebarWidget->setOverflowMarqueeEnabled(
        m_actions.animateSidebarText->isChecked()
        );

    connect(
        m_actions.showSidebarTooltips,
        &QAction::toggled,
        ui->sidebarWidget,
        &Sidebar::setOverflowTooltipsEnabled
        );

    connect(
        m_actions.animateSidebarText,
        &QAction::toggled,
        ui->sidebarWidget,
        &Sidebar::setOverflowMarqueeEnabled
        );

    connect(
        ui->sidebarWidget,
        &Sidebar::itemSelected,
        m_navigationController.get(),
        &NavigationController::handleNavigation
        );

    connect(
        m_pages->classesPage(),
        &ClassesPage::classInfoSaved,
        m_sidebarController.get(),
        &SidebarController::handleClassInfoSaved
        );

    connect(
        m_pages->schedulePage(),
        &SchedulePage::classInfoSaved,
        m_sidebarController.get(),
        &SidebarController::handleClassInfoSaved
        );

    connect(
        m_pages->mySchedulePage(),
        &SchedulePage::classInfoSaved,
        m_sidebarController.get(),
        &SidebarController::handleClassInfoSaved
        );

    connect(
        m_pages->mySchedulePage(),
        &SchedulePage::displayModeChanged,
        m_pages->classesPage(),
        &ClassesPage::setScheduleDisplayMode
        );
    connect(
        m_pages->mySchedulePage(),
        &SchedulePage::displayModeChanged,
        m_pages->rostersPage(),
        &RostersPage::setScheduleDisplayMode
        );
    connect(
        m_pages->mySchedulePage(),
        &SchedulePage::displayModeChanged,
        m_pages->speakingPage(),
        &SpeakingEvalPage::setScheduleDisplayMode
        );
    m_pages->classesPage()->setScheduleDisplayMode(
        m_pages->mySchedulePage()->displayMode()
        );
    m_pages->rostersPage()->setScheduleDisplayMode(
        m_pages->mySchedulePage()->displayMode()
        );
    m_pages->speakingPage()->setScheduleDisplayMode(
        m_pages->mySchedulePage()->displayMode()
        );

    const auto connectScheduleImport =
        [this](SchedulePage* page)
        {
            connect(
                page,
                &SchedulePage::scheduleImportRequested,
                this,
                [this, page]()
                {
                    ScheduleImportDialog dialog(
                        m_services.get(),
                        page
                        );

                    if (dialog.exec() != QDialog::Accepted)
                    {
                        return;
                    }

                    m_pages->schedulePage()->refresh();
                    m_pages->mySchedulePage()->refresh();
                    m_sidebarController->refreshTeacherSidebar();
                    m_sidebarController->handleClassInfoSaved(-1);
                }
                );
        };

    connectScheduleImport(m_pages->schedulePage());
    connectScheduleImport(m_pages->mySchedulePage());

    const auto connectTestingClasses =
        [this](
            SchedulePage* page,
            bool personalSchedule
            )
        {
            connect(
                page,
                &SchedulePage::testingClassesRequested,
                this,
                [this, personalSchedule](
                    int classId,
                    const QString& day,
                    const QString& startTime
                    )
                {
                    if (!m_pages->confirmCurrentPageCanLeave())
                    {
                        return;
                    }

                    m_testingClassesReturnToPersonalSchedule =
                        personalSchedule;
                    m_pages->testingClassesPage()->openTestingClass(
                        classId,
                        day,
                        startTime
                        );
                    m_pages->showPage(
                        PageType::TestingClasses
                        );
                }
                );
        };

    connectTestingClasses(
        m_pages->schedulePage(),
        false
        );
    connectTestingClasses(
        m_pages->mySchedulePage(),
        true
        );

    connect(
        m_pages->testingClassesPage(),
        &TestingClassesPage::returnToScheduleRequested,
        this,
        [this]()
        {
            if (!m_pages->confirmCurrentPageCanLeave())
            {
                return;
            }

            m_pages->showPage(
                m_testingClassesReturnToPersonalSchedule
                    ? PageType::MySchedule
                    : PageType::Schedule
                );
            (
                m_testingClassesReturnToPersonalSchedule
                    ? m_pages->mySchedulePage()
                    : m_pages->schedulePage()
                )
                ->refresh();
        }
        );

    connect(
        m_pages->testingClassesPage(),
        &TestingClassesPage::testingDataChanged,
        this,
        [this]()
        {
            m_pages->schedulePage()->refresh();
            m_pages->mySchedulePage()->refresh();
        }
        );

    connect(
        m_pages->teacherPage(),
        &TeacherInfoPage::teacherSaved,
        m_sidebarController.get(),
        &SidebarController::handleTeacherSaved
        );

    connect(
        m_pages->campusDashboard(),
        &CampusDashboardPage::sectionChanged,
        ui->sidebarWidget,
        &Sidebar::selectCampusSection
        );

    if (m_actions.manageCampuses)
    {
        connect(
            m_actions.manageCampuses,
            &QAction::triggered,
            this,
            [this]()
            {
                if (!m_pages || !m_pages->confirmCurrentPageCanLeave())
                {
                    return;
                }

                m_pages->showPage(PageType::CampusDashboard);
                ui->sidebarWidget->selectCampusSection(
                    QStringLiteral("campus_information")
                    );
            }
            );
    }

    if (m_actions.about)
    {
        connect(
            m_actions.about,
            &QAction::triggered,
            this,
            [this]()
            {
                AboutDialog dialog(this);
                dialog.exec();
            }
            );
    }
}

void MainWindow::updatePrintExportActions()
{
    const PageOutputCapabilities capabilities =
        m_pages
            ? m_pages->outputCapabilities()
            : PageOutputCapabilities{};

    if (m_actions.printCurrentPage)
    {
        m_actions.printCurrentPage->setEnabled(
            capabilities.printEnabled
            );
    }

    if (m_actions.saveCurrentPageAs)
    {
        m_actions.saveCurrentPageAs->setEnabled(
            capabilities.saveAsEnabled
            );
    }

    if (m_actions.printExportMenu)
    {
        m_actions.printExportMenu->setEnabled(
            capabilities.printEnabled
            || capabilities.saveAsEnabled
            );
    }
}

void MainWindow::startInitialSetup()
{
    DialogServices::showInformation(
        this,
        tr("Create a Teacher Profile file"),
        tr("Choose a filename and select a location to save your Teacher Profile. It is recommended to use an iCloud, OneDrive, Google Drive, or other cloud storage folder to help avoid accidental data loss.\n\n"
           "To replace an existing profile with new data, select its .tps file in the next dialog.")
        );

    if (
        !m_fileController
        || !m_services
        || !m_fileController->createInitialSetupDatabaseInteractive()
        )
    {
        return;
    }

    InitialSetupWizard wizard(
        m_services.get(),
        this
        );

    if (wizard.exec() != QDialog::Accepted)
    {
        m_fileController->cancelInitialSetup();
        return;
    }

    m_fileController->finishInitialSetup();

    if (m_pages)
    {
        m_pages->refreshAll();
        m_pages->showPage(PageType::MySchedule);
    }

    if (m_sidebarController)
    {
        m_sidebarController->refreshAllSidebars();
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->selectMyInfoSection(
            QStringLiteral("my_info_schedule")
            );
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!m_startupBirthdayCheckQueued)
    {
        m_startupBirthdayCheckQueued = true;

        if (
            m_startupOptions.runPostShowStartupTasks
            && m_services
            && m_services->hasOpenDatabase()
            && m_sidebarController
            )
        {
            QTimer::singleShot(
                0,
                this,
                [this]
                {
                    if (
                        m_services
                        && m_services->hasOpenDatabase()
                        && m_sidebarController
                        )
                    {
                        m_sidebarController
                            ->showUpcomingBirthdaysIfRelevantOnStartup();
                    }
                }
                );
        }
    }

    if (
        !m_startupFontSizeRefreshQueued
        && m_fontSizeController
        && m_actions.fontSizeState
        && m_startupOptions.runPostShowStartupTasks
        )
    {
        m_startupFontSizeRefreshQueued =
            true;

        QTimer::singleShot(
            0,
            this,
            &MainWindow::reapplyStartupFontSize
            );
    }

}

void MainWindow::reapplyStartupFontSize()
{
    if (!m_fontSizeController || !m_actions.fontSizeState)
    {
        return;
    }

    m_fontSizeController->changeFontSize(
        m_actions.fontSizeState->current()
        );

    if (m_sidebarController)
    {
        m_sidebarController->refreshAllSidebars();
    }

    if (m_pages)
    {
        m_pages->refreshAll();
    }

    if (QLayout* mainLayout = layout())
    {
        mainLayout->invalidate();
    }

    updateGeometry();
    update();
    repaint();
}

void MainWindow::showStartupDatabasePage()
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
        || !m_pages
        )
    {
        return;
    }

    m_pages->showPage(PageType::MySchedule);

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->selectMyInfoSection(
            QStringLiteral("my_info_schedule")
            );
    }
}

bool MainWindow::confirmCurrentPageCanLeave(
    bool exiting
    ) const
{
    return !m_pages
        || m_pages->confirmCurrentPageCanLeave(exiting);
}

void MainWindow::clearDatabaseBackedState()
{
    if (m_pages)
    {
        m_pages->clearDatabaseState();
    }
}

void MainWindow::applyNoDatabaseState()
{
    clearDatabaseBackedState();

    if (m_pages)
    {
        m_pages->setDatabaseOpen(false);
    }

    if (m_sidebarController)
    {
        m_sidebarController->refreshAllSidebars();
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->setDatabaseSectionsVisible(false);
    }

    setDatabaseBackedActionsEnabled(false);

    if (m_pages && m_pages->campusDashboard())
    {
        m_pages->showPage(PageType::CampusDashboard);
        m_pages->campusDashboard()->showInformation();
        m_pages->campusDashboard()->refresh();
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->selectCampusSection(
            QStringLiteral("campus_information")
            );
    }
}

void MainWindow::applyDatabaseLoadedState()
{
    if (m_pages)
    {
        m_pages->setDatabaseOpen(true);
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->setDatabaseSectionsVisible(true);
    }

    setDatabaseBackedActionsEnabled(true);

    if (m_sidebarController)
    {
        m_sidebarController->refreshAllSidebars();
    }

    if (m_pages && m_pages->personalDetailsPage())
    {
        m_pages->showPage(PageType::PersonalDetails);
        m_pages->refreshAll();
        m_pages->personalDetailsPage()->scrollToTop();
    }

    if (ui && ui->sidebarWidget)
    {
        ui->sidebarWidget->selectMyInfoSection(
            QStringLiteral("my_info_information")
            );
    }
}

void MainWindow::setDatabaseBackedActionsEnabled(
    bool enabled
    )
{
    if (m_actions.saveFile)
    {
        m_actions.saveFile->setEnabled(enabled);
    }

    if (m_actions.saveAsFile)
    {
        m_actions.saveAsFile->setEnabled(enabled);
    }

    if (m_actions.exportAsFile)
    {
        m_actions.exportAsFile->setEnabled(enabled);
    }

    if (m_actions.closeFile)
    {
        m_actions.closeFile->setEnabled(enabled);
    }

    if (m_actions.newClass)
    {
        m_actions.newClass->setEnabled(enabled);
    }

    if (m_actions.deleteClass)
    {
        m_actions.deleteClass->setEnabled(enabled);
    }

    if (m_actions.importClasses)
    {
        m_actions.importClasses->setEnabled(enabled);
    }

    if (m_actions.exportClasses)
    {
        m_actions.exportClasses->setEnabled(enabled);
    }

    if (m_actions.newTeacher)
    {
        m_actions.newTeacher->setEnabled(enabled);
    }

    if (m_actions.deleteTeacher)
    {
        m_actions.deleteTeacher->setEnabled(enabled);
    }

    if (m_actions.upcomingBirthdays)
    {
        m_actions.upcomingBirthdays->setEnabled(enabled);
    }
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
        m_pages->showPage(PageType::Classes);
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
    ui->splitter->restoreState(
        SettingsManager::instance().getSplitterState()
        );
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmCurrentPageCanLeave(true))
    {
        event->ignore();
        return;
    }

    SettingsManager::instance().setSplitterState(
        ui->splitter->saveState()
        );

    QMainWindow::closeEvent(event);
}
