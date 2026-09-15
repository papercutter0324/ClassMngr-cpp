#include "app/mainwindow.h"
#include "app/controllers/update_controller.h"
#include "app/startup_database_path.h"
#include "core/appsettings.h"
#include "core/build_info.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/resource_paths.h"
#include "core/settingsmanager.h"
#include "core/startup_profiler.h"
#include "core/theme_service.h"
#include "core/updater/update_service.h"
#include "ui/shared/widgets/splash/splashscreen.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"
#include "core/utils/platform.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "ui/shared/pages/pagemanager.h"

#if !defined(Q_OS_MACOS)
#include "ui/shared/styles/file_dialog_icon_style.h"
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

#include <functional>
#include <memory>
#include <optional>

// Later: move MainWindow construction behind an ApplicationBootstrap class

// =========================================================
// Helpers
// =========================================================

QIcon getAppIcon()
{
    Platform userPlatform = getPlatform();

    if (userPlatform == Platform::WINDOWS)
    {
        return QIcon(ResourcePaths::Icons::appWindows());
    }

    return QIcon(ResourcePaths::Icons::appDefault());
}


bool isAdminMode(const QStringList &args)
{
    return args.contains(AppSettings::AdminModeArgument);
}

struct StartupPerformanceMode
{
    bool enabled = false;
    QString outputPath;
    bool visualCaptureEnabled = false;
    QString visualCaptureOutputPath;
    std::optional<Language> visualLanguageOverride;
    std::optional<Theme> visualThemeOverride;
    bool workflowEnabled = false;
    enum class Scenario
    {
        Minimal,
        Representative
    };

    Scenario scenario = Scenario::Minimal;
    int settleMilliseconds = 0;
};

QString startupScenarioName(
    StartupPerformanceMode::Scenario scenario
    )
{
    return scenario == StartupPerformanceMode::Scenario::Representative
        ? QStringLiteral("representative-startup")
        : QStringLiteral("minimal-startup");
}

constexpr int StartupWorkflowStepDelayMilliseconds = 150;

const QList<PageType>& startupWorkflowPageTypes()
{
    static const QList<PageType> pageTypes{
        PageType::MyWorkspace,
        PageType::Schedule,
        PageType::Classes,
        PageType::TestingClasses,
        PageType::TeacherInfo,
        PageType::NativeEnglishTeachers,
        PageType::GsTeam,
        PageType::CampusDashboard,
        PageType::SubPrep,
        PageType::MyClasses,
        PageType::PdfViewer,
        PageType::MyWorkspace
    };

    return pageTypes;
}

void appendStartupWorkflowTrace(const QString& message)
{
    const QString outputPath =
        qEnvironmentVariable("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH")
            .trimmed();
    if (outputPath.isEmpty())
    {
        return;
    }

    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        file.write((message + QLatin1Char('\n')).toUtf8());
        file.flush();
    }
}

StartupPerformanceMode startupPerformanceMode(
    const QStringList& args
    )
{
    StartupPerformanceMode mode;

    mode.enabled =
        args.contains(
            QStringLiteral("--startup-performance-test")
            );
    mode.workflowEnabled =
        args.contains(
            QStringLiteral("--startup-performance-workflow")
            );
    mode.enabled = mode.enabled || mode.workflowEnabled;

    const int outputIndex =
        args.indexOf(
            QStringLiteral("--startup-performance-output")
            );

    if (
        outputIndex >= 0
        && outputIndex + 1 < args.size()
        )
    {
        mode.outputPath =
            args.at(outputIndex + 1);
    }

    const int visualCaptureIndex =
        args.indexOf(
            QStringLiteral("--startup-visual-capture-output")
            );
    if (visualCaptureIndex >= 0)
    {
        mode.enabled = true;
        mode.visualCaptureEnabled = true;

        if (visualCaptureIndex + 1 < args.size())
        {
            mode.visualCaptureOutputPath =
                args.at(visualCaptureIndex + 1);
        }
    }

    const int visualLanguageIndex =
        args.indexOf(
            QStringLiteral("--startup-visual-capture-language")
            );
    if (visualLanguageIndex >= 0 && visualLanguageIndex + 1 < args.size())
    {
        const QString language =
            args.at(visualLanguageIndex + 1).trimmed().toLower();
        if (language == QStringLiteral("english"))
        {
            mode.visualLanguageOverride = Language::English;
        }
        else if (language == QStringLiteral("korean"))
        {
            mode.visualLanguageOverride = Language::Korean;
        }
        else
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Unknown startup visual capture language '%1'."
                    ).arg(language);
        }
    }

    const int visualThemeIndex =
        args.indexOf(
            QStringLiteral("--startup-visual-capture-theme")
            );
    if (visualThemeIndex >= 0 && visualThemeIndex + 1 < args.size())
    {
        const QString theme =
            args.at(visualThemeIndex + 1).trimmed().toLower();
        if (theme == QStringLiteral("light"))
        {
            mode.visualThemeOverride = Theme::Light;
        }
        else if (theme == QStringLiteral("dark"))
        {
            mode.visualThemeOverride = Theme::Dark;
        }
        else
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Unknown startup visual capture theme '%1'."
                    ).arg(theme);
        }
    }

    const int scenarioIndex =
        args.indexOf(
            QStringLiteral("--startup-performance-scenario")
            );
    if (scenarioIndex >= 0 && scenarioIndex + 1 < args.size())
    {
        const QString scenario = args.at(scenarioIndex + 1).trimmed();
        if (scenario == QStringLiteral("representative"))
        {
            mode.scenario = StartupPerformanceMode::Scenario::Representative;
            mode.settleMilliseconds = 30000;
        }
        else if (scenario != QStringLiteral("minimal"))
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Unknown startup profiling scenario '%1'; using minimal."
                    ).arg(scenario);
        }
    }

    // A supplied database makes a visual capture representative unless the
    // caller explicitly requested the minimal scenario.
    if (
        mode.visualCaptureEnabled
        && scenarioIndex < 0
        && !startupDatabasePath(args).trimmed().isEmpty()
        )
    {
        mode.scenario = StartupPerformanceMode::Scenario::Representative;
        mode.settleMilliseconds = 30000;
    }

    if (
        mode.workflowEnabled
        && scenarioIndex < 0
        && !startupDatabasePath(args).trimmed().isEmpty()
        )
    {
        mode.scenario = StartupPerformanceMode::Scenario::Representative;
        mode.settleMilliseconds = 5000;
    }

    const int settleIndex =
        args.indexOf(
            QStringLiteral("--startup-performance-settle-ms")
            );
    if (settleIndex >= 0 && settleIndex + 1 < args.size())
    {
        bool converted = false;
        const int settleMilliseconds = args.at(settleIndex + 1).toInt(&converted);
        if (converted && settleMilliseconds >= 0)
        {
            mode.settleMilliseconds = settleMilliseconds;
        }
        else
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Ignoring invalid --startup-performance-settle-ms value '%1'."
                    ).arg(args.at(settleIndex + 1));
        }
    }

    return mode;
}

bool captureStartupVisual(
    const QString& outputDirectoryPath,
    QWidget& window,
    const QString& checkpointName
    )
{
    if (outputDirectoryPath.trimmed().isEmpty())
    {
        qWarning()
            << "Startup visual capture output directory was not provided.";
        return false;
    }

    QDir outputDirectory(outputDirectoryPath);
    if (!outputDirectory.mkpath(QStringLiteral(".")))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to create startup visual capture directory %1."
                ).arg(outputDirectoryPath);
        return false;
    }

    const QPixmap screenshot = window.grab();
    if (screenshot.isNull() || screenshot.size().isEmpty())
    {
        qWarning()
            << "Startup visual capture returned an empty window image.";
        return false;
    }

    const QString outputPath =
        outputDirectory.filePath(
            QStringLiteral("%1.png").arg(checkpointName)
            );
    if (!screenshot.save(outputPath, "PNG"))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to write startup visual capture to %1."
                ).arg(outputPath);
        return false;
    }

    qInfo().noquote()
        << QStringLiteral(
            "Wrote startup visual capture %1 (%2x%3)."
            )
            .arg(
                outputPath,
                QString::number(screenshot.width()),
                QString::number(screenshot.height())
                );
    return true;
}

void scheduleStartupPerformanceWorkflow(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    const auto pageTypes =
        std::make_shared<QList<PageType>>(startupWorkflowPageTypes());
    const auto pageIndex = std::make_shared<int>(0);
    const auto runNextPage =
        std::make_shared<std::function<void()>>();

    *runNextPage =
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            pageTypes,
            pageIndex,
            runNextPage,
            completion
        ]()
    {
        if (*pageIndex >= pageTypes->size())
        {
            appendStartupWorkflowTrace(QStringLiteral("complete"));
            qInfo().noquote()
                << QStringLiteral(
                    "Startup performance workflow completed; returning to My Workspace."
                    );
            profiler.checkpoint(
                QStringLiteral("workflow-complete"),
                QStringLiteral("returned-to-my-workspace")
                );
            completion();
            return;
        }

        const PageType pageType = pageTypes->at(*pageIndex);
        const QString pageIdentifier =
            PageManager::pageTypeIdentifier(pageType);

        appendStartupWorkflowTrace(
            QStringLiteral("start %1").arg(pageIdentifier)
            );
        qInfo().noquote()
            << QStringLiteral("Startup performance workflow entering %1.")
                .arg(pageIdentifier);

        profiler.checkpoint(
            QStringLiteral("workflow-page-start"),
            pageIdentifier
            );

        PageManager* pages = window.pageManager();
        bool pageReady = false;
        if (pages)
        {
            appendStartupWorkflowTrace(
                QStringLiteral("showPage %1").arg(pageIdentifier)
                );
            pages->showPage(pageType);
            appendStartupWorkflowTrace(
                QStringLiteral("showPage-returned %1").arg(pageIdentifier)
                );
            app.processEvents();
            appendStartupWorkflowTrace(
                QStringLiteral("events-processed %1").arg(pageIdentifier)
                );
            pageReady = pages->isCurrentPage(pageType);
        }

        if (!pageReady)
        {
            *workflowSucceeded = false;
            profiler.checkpoint(
                QStringLiteral("workflow-page-failed"),
                pageIdentifier
                );
        }
        else
        {
            qInfo().noquote()
                << QStringLiteral("Startup performance workflow ready: %1.")
                    .arg(pageIdentifier);
            profiler.checkpoint(
                QStringLiteral("workflow-page-ready"),
                pageIdentifier
                );
        }

        // Calendar is deliberately a child of My Workspace and is not
        // created by the startup route. Open it once in the workflow, then
        // return to the normal schedule tab before leaving the page.
        if (
            pageReady
            && *pageIndex == 0
            && pageType == PageType::MyWorkspace
            && pages->myWorkspacePage()
            )
        {
            MyWorkspacePage* workspace = pages->myWorkspacePage();
            profiler.checkpoint(
                QStringLiteral("workflow-child-start"),
                QStringLiteral("calendar")
                );
            appendStartupWorkflowTrace(QStringLiteral("calendar-start"));
            workspace->openTab(WorkspaceTab::Calendar);
            appendStartupWorkflowTrace(QStringLiteral("calendar-open-returned"));
            app.processEvents();
            appendStartupWorkflowTrace(QStringLiteral("calendar-events-processed"));

            const bool calendarReady = workspace->calendarPage() != nullptr;
            if (!calendarReady)
            {
                *workflowSucceeded = false;
            }
            profiler.checkpoint(
                calendarReady
                    ? QStringLiteral("workflow-child-ready")
                    : QStringLiteral("workflow-child-failed"),
                QStringLiteral("calendar")
                );

            workspace->openTab(WorkspaceTab::Schedule);
            appendStartupWorkflowTrace(QStringLiteral("calendar-schedule-returned"));
            app.processEvents();
            profiler.checkpoint(
                QStringLiteral("workflow-child-released"),
                QStringLiteral("calendar")
                );
        }

        ++*pageIndex;
        QTimer::singleShot(
            StartupWorkflowStepDelayMilliseconds,
            &app,
            [runNextPage]()
            {
                (*runNextPage)();
            }
            );
    };

    QTimer::singleShot(
        0,
        &app,
        [runNextPage]()
        {
            (*runNextPage)();
        }
        );
}

bool writeStartupPerformanceMetrics(
    const QString& outputPath,
    const StartupProfiler& profiler,
    const StartupPerformanceMode& mode,
    int progressUpdates,
    int finalProgress
    )
{
    if (outputPath.trimmed().isEmpty())
    {
        qWarning()
            << "Startup performance output path was not provided.";
        return false;
    }

    QFile file(outputPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to write startup performance metrics to %1: %2"
                )
                .arg(outputPath, file.errorString());
        return false;
    }

    QJsonObject metrics = profiler.reportJson();

    metrics.insert(
        QStringLiteral("progressUpdates"),
        progressUpdates
        );
    metrics.insert(
        QStringLiteral("finalProgress"),
        finalProgress
        );

    metrics.insert(
        QStringLiteral("format"),
        QStringLiteral("classmngr-startup-profile-v2")
        );

    QJsonArray scenarioActions{
        mode.scenario == StartupPerformanceMode::Scenario::Minimal
            ? QStringLiteral("launch without a startup database")
            : QStringLiteral("load the supplied or saved startup database"),
        QStringLiteral("construct the main window"),
        QStringLiteral("capture startup-complete and requested settled checkpoints"),
        QStringLiteral("suppress interactive startup prompts during profiling")
    };
    if (mode.workflowEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "navigate through all registered page routes and return to My Workspace"
                )
            );
    }

    QJsonArray workflowPages;
    for (const PageType pageType : startupWorkflowPageTypes())
    {
        workflowPages.append(PageManager::pageTypeIdentifier(pageType));
    }

    metrics.insert(
        QStringLiteral("scenario"),
        QJsonObject{
            {QStringLiteral("name"), startupScenarioName(mode.scenario)},
            {QStringLiteral("actions"), scenarioActions},
            {QStringLiteral("settleMilliseconds"), mode.settleMilliseconds}
        }
        );

    metrics.insert(
        QStringLiteral("workflow"),
        QJsonObject{
            {QStringLiteral("enabled"), mode.workflowEnabled},
            {QStringLiteral("stepDelayMilliseconds"),
             StartupWorkflowStepDelayMilliseconds},
            {QStringLiteral("pages"), workflowPages}
        }
        );

    file.write(
        QJsonDocument(metrics).toJson(QJsonDocument::Indented)
        );

    return file.error() == QFile::NoError;
}



// =========================================================
// Main
// =========================================================

int main(int argc, char *argv[])
{
    StartupProfiler startupProfiler;
    QStringList launchArguments;
    launchArguments.reserve(argc);
    for (int index = 0; index < argc; ++index)
    {
        launchArguments.append(QString::fromLocal8Bit(argv[index]));
    }

    const StartupPerformanceMode startupPerformance =
        startupPerformanceMode(launchArguments);
    if (startupPerformance.enabled)
    {
        StartupProfiler::activate(&startupProfiler);
        startupProfiler.checkpoint(QStringLiteral("process-start"));
    }

#if !defined(Q_OS_MACOS)
    // The custom file-dialog style is for Qt's widget dialog.  On macOS,
    // retain the native NSOpenPanel: forcing the widget implementation can
    // deadlock while it is initialized.
    QApplication::setAttribute(
        Qt::AA_DontUseNativeDialogs
        );
#endif
    QApplication app(argc, argv);

#if !defined(Q_OS_MACOS)
    app.setStyle(
        new FileDialogIconStyle()
        );
#endif

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("qapplication-created"));
    }

    const QString initialDatabasePath =
        startupPerformance.enabled
            && !startupPerformance.visualCaptureEnabled
            && startupPerformance.scenario
                == StartupPerformanceMode::Scenario::Minimal
            ? QString()
            : startupDatabasePath(
                app.arguments()
                );

    app.setApplicationName(AppSettings::ApplicationName);
    app.setOrganizationName(AppSettings::OrganizationName);
    app.setApplicationVersion(
        QString::fromUtf8(BuildInfo::Version)
        );

    // =====================================================
    // Translation Support
    // =====================================================

    LanguageService languageService;

    const Language savedLanguage =
        startupPerformance.visualCaptureEnabled
        && startupPerformance.visualLanguageOverride.has_value()
            ? *startupPerformance.visualLanguageOverride
            : LanguageService::savedLanguage();

    const FontSize savedFontSize =
        fontSizeFromStoredValue(
            SettingsManager::instance().get(
                OptionKeys::FontSize,
                fontSizeOffset(FontSize::Normal)
                ).toInt()
            );

    const Theme savedTheme =
        startupPerformance.visualCaptureEnabled
        && startupPerformance.visualThemeOverride.has_value()
            ? *startupPerformance.visualThemeOverride
            : themeFromStoredValue(
                  SettingsManager::instance().get(
                      OptionKeys::Theme,
                      static_cast<int>(Theme::SystemDefault)
                      ).toInt()
                  );

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("preferences-resolved"));
    }

    languageService.setLanguage(
        savedLanguage
        );

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("locale-applied"));
    }



    // =====================================================
    // Fonts
    // =====================================================

    FontManager::setSizeOffset(
        fontSizeOffset(savedFontSize)
        );

    FontManager::applyGlobalFont(
        app,
        languageService.loadedLocaleName()
        );

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("font-applied"));
    }
    // FontManager::debugDump();

    auto startupThemeService =
        std::make_unique<ThemeService>();
    startupThemeService->setTheme(savedTheme);

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("theme-applied"));
    }


    // =====================================================
    // Splash Screen
    // =====================================================

    if (
        const Status resourcePackStatus =
            ResourcePackManager::instance().initialize();
        !resourcePackStatus
        )
    {
        qWarning().noquote() << resourcePackStatus.error();
    }

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(
            QStringLiteral("resource-system-initialized")
            );
    }

    auto splashLease = ResourcePaths::Splash::acquire();
    if (!splashLease)
    {
        qWarning().noquote() << splashLease.error();
        return 1;
    }

    auto splash = std::make_unique<SplashScreen>(
        ResourcePaths::Splash::imagePath(*splashLease)
        );

    splash->centerOnScreen();
    splash->show();

    app.processEvents();

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("splash-shown"));
    }



    // =====================================================
    // Progress Callback
    // =====================================================

    // Language and font setup complete before the splash can be displayed.
    int completedStartupSteps = 2;
    int progressUpdates = 0;
    int progress = 0;
    bool visualCaptureSucceeded = true;

    auto updateProgress =
        [&](const QString &message)
    {
        ++completedStartupSteps;
        ++progressUpdates;

        progress =
            qMin(
                100,
                (100 * completedStartupSteps)
                    / AppSettings::StartupProgressSteps
                );

        splash->setMessage(message);
        splash->setProgress(progress);

        app.processEvents();
    };



    // =====================================================
    // Startup Timer
    // =====================================================

    QElapsedTimer startupTimer;

    startupTimer.start();

    // =====================================================
    // Main Window
    // =====================================================

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Loading resource packs..."
            )
        );

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Loading application icon..."
            )
        );

    app.setWindowIcon(getAppIcon());

    std::unique_ptr<UpdateService> updateService;
    std::unique_ptr<UpdateController> updateController;

    MainWindow window(
        updateProgress,
        isAdminMode(app.arguments()),
        &languageService,
        nullptr,
        {
            .loadMostRecentDatabase =
                !startupPerformance.enabled
                || startupPerformance.scenario
                    == StartupPerformanceMode::Scenario::Representative,
            .initialDatabasePath = initialDatabasePath,
            .startupThemeService = std::move(startupThemeService),
            .checkpointCallback =
                [&startupProfiler, &startupPerformance](
                    const QString& name,
                    const QString& detail
                    )
                {
                    if (startupPerformance.enabled)
                    {
                        startupProfiler.checkpoint(name, detail);
                    }
                }
        }
        );

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Ready..."
            )
        );

    Q_ASSERT(
        completedStartupSteps
            == AppSettings::StartupProgressSteps
        );

    // =====================================================
    // Ensure Minimum Splash Time
    // =====================================================

    int elapsed =
        static_cast<int>(
            startupTimer.elapsed()
            );

    int remaining =
        qMax(0, AppSettings::MinimumSplashDurationMs - elapsed);



    // =====================================================
    // Finish Startup
    // =====================================================

    auto runPostStartupTasks =
        [
            &app,
            &window,
            &updateService,
            &updateController,
            &startupPerformance,
            &startupProfiler,
            &visualCaptureSucceeded,
            progressUpdates,
            progress
        ]()
    {
        // These tasks intentionally run in a later event-loop turn, after
        // startup-complete. Normal startup only starts the updater here;
        // explicit profiling may opt into the page lifecycle workflow below.
        if (!startupPerformance.enabled)
        {
            updateService = std::make_unique<UpdateService>();
            updateController = std::make_unique<UpdateController>(
                updateService.get(),
                &app
                );
            window.attachUpdateController(updateController.get());
            updateController->setStartupComplete();
            updateController->startAutomaticCheck();
            return;
        }

        const auto workflowSucceeded = std::make_shared<bool>(true);

        const auto finishPerformanceRun =
            [
                &app,
                &window,
                &startupProfiler,
                &startupPerformance,
                &visualCaptureSucceeded,
                workflowSucceeded,
                progressUpdates,
                progress
            ]()
        {
            if (
                startupPerformance.visualCaptureEnabled
                && startupPerformance.settleMilliseconds > 0
                )
            {
                app.processEvents();
                visualCaptureSucceeded =
                    captureStartupVisual(
                        startupPerformance.visualCaptureOutputPath,
                        window,
                        QStringLiteral("settled-final")
                        )
                    && visualCaptureSucceeded;
            }

            const bool metricsWritten =
                startupPerformance.visualCaptureEnabled
                && startupPerformance.outputPath.trimmed().isEmpty()
                ? true
                : writeStartupPerformanceMetrics(
                      startupPerformance.outputPath,
                      startupProfiler,
                      startupPerformance,
                      progressUpdates,
                      progress
                      );
            app.exit(
                metricsWritten
                    && visualCaptureSucceeded
                    && *workflowSucceeded
                    ? 0
                    : 2
                );
        };

        const auto scheduleSettledCompletion =
            [
                &app,
                &startupProfiler,
                &startupPerformance,
                finishPerformanceRun
            ]()
        {
            const int settleMilliseconds =
                startupPerformance.settleMilliseconds;
            if (settleMilliseconds >= 1000)
            {
                QTimer::singleShot(
                    1000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-1s")
                            );
                    }
                    );
            }
            if (settleMilliseconds >= 5000)
            {
                QTimer::singleShot(
                    5000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-5s")
                            );
                    }
                    );
            }
            if (settleMilliseconds >= 30000)
            {
                QTimer::singleShot(
                    30000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-30s")
                            );
                    }
                    );
            }

            const int completionDelayMilliseconds =
                settleMilliseconds > 0
                    ? settleMilliseconds + 500
                    : 0;
            QTimer::singleShot(
                completionDelayMilliseconds,
                &app,
                [
                    &startupProfiler,
                    settleMilliseconds,
                    finishPerformanceRun
                ]()
                {
                    if (
                        settleMilliseconds > 0
                        && settleMilliseconds != 1000
                        && settleMilliseconds != 5000
                        && settleMilliseconds != 30000
                        )
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-final"),
                            QStringLiteral("elapsedMs=%1")
                                .arg(settleMilliseconds)
                            );
                    }
                    finishPerformanceRun();
                }
                );
        };

        if (startupPerformance.workflowEnabled)
        {
            scheduleStartupPerformanceWorkflow(
                app,
                window,
                startupProfiler,
                workflowSucceeded,
                scheduleSettledCompletion
                );
            return;
        }

        scheduleSettledCompletion();
    };

    // This is the single transition from startup to normal operation.  The
    // splash and its resource lease are gone before the checkpoint is taken,
    // and all optional work is queued only after that snapshot.
    auto finish =
        [
            &app,
            &window,
            &splash,
            &splashLease,
            &startupPerformance,
            &startupProfiler,
            &visualCaptureSucceeded,
            runPostStartupTasks
        ]()
    {
        window.show();
        Q_ASSERT(window.isVisible());

        if (startupPerformance.enabled)
        {
            startupProfiler.checkpoint(QStringLiteral("window-shown"));
        }

        splash.reset();
        splashLease->reset();

        if (startupPerformance.enabled)
        {
            startupProfiler.checkpoint(QStringLiteral("startup-complete"));
            StartupProfiler::recordStartupCompleteScheduleWidgetDiagnostic();
        }

        if (startupPerformance.visualCaptureEnabled)
        {
            app.processEvents();
            visualCaptureSucceeded =
                captureStartupVisual(
                    startupPerformance.visualCaptureOutputPath,
                    window,
                    QStringLiteral("startup-complete")
                    )
                && visualCaptureSucceeded;
        }

        QTimer::singleShot(
            0,
            &app,
            runPostStartupTasks
            );
    };

    auto startFinish = [&splash, &finish]()
    {
        splash->fadeOut(finish);
    };

    if (remaining <= 0)
        startFinish();
    else
        QTimer::singleShot(remaining, startFinish);



    // =====================================================
    // Run App
    // =====================================================

    return app.exec();
}
