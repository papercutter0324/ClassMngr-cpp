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
#include "core/updater/update_service.h"
#include "ui/shared/widgets/splash/splashscreen.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"
#include "core/utils/platform.h"

#if !defined(Q_OS_MACOS)
#include "ui/shared/styles/file_dialog_icon_style.h"
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

#include <memory>

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

StartupPerformanceMode startupPerformanceMode(
    const QStringList& args
    )
{
    StartupPerformanceMode mode;

    mode.enabled =
        args.contains(
            QStringLiteral("--startup-performance-test")
            );

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
    metrics.insert(
        QStringLiteral("scenario"),
        QJsonObject{
            {QStringLiteral("name"), startupScenarioName(mode.scenario)},
            {QStringLiteral("actions"), QJsonArray{
                mode.scenario == StartupPerformanceMode::Scenario::Minimal
                    ? QStringLiteral("launch without a startup database")
                    : QStringLiteral("load the supplied or saved startup database"),
                QStringLiteral("construct the main window"),
                QStringLiteral("capture startup-complete and requested settled checkpoints"),
                QStringLiteral("suppress interactive startup prompts during profiling")
            }},
            {QStringLiteral("settleMilliseconds"), mode.settleMilliseconds}
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

    const Language savedLanguage = LanguageService::savedLanguage();

    const FontSize savedFontSize =
        fontSizeFromStoredValue(
            SettingsManager::instance().get(
                OptionKeys::FontSize,
                fontSizeOffset(FontSize::Normal)
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

    UpdateService updateService;
    UpdateController updateController(
        &updateService,
        &app
        );
    updateController.setSplashScreen(
        splash.get()
        );

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Checking for updates..."
            )
        );

    if (
        !startupPerformance.enabled
        || startupPerformance.scenario
            == StartupPerformanceMode::Scenario::Representative
        )
    {
        updateController.startStartupCheck();
    }



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

    MainWindow window(
        updateProgress,
        isAdminMode(app.arguments()),
        &languageService,
        &updateController,
        {
            .loadMostRecentDatabase =
                !startupPerformance.enabled
                || startupPerformance.scenario
                    == StartupPerformanceMode::Scenario::Representative,
            .runPostShowStartupTasks =
                !startupPerformance.enabled
                || startupPerformance.scenario
                    == StartupPerformanceMode::Scenario::Representative,
            .showStartupBirthdayPrompt = !startupPerformance.enabled,
            .initialDatabasePath = initialDatabasePath,
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

    auto finish =
        [
            &app,
            &window,
            &splash,
            &splashLease,
            &updateController,
            &startupPerformance,
            &startupProfiler,
            progressUpdates,
            progress
        ]()
    {
        const bool preserveUpdateDialogFocus =
            updateController.hasVisibleDialog();

        if (preserveUpdateDialogFocus)
        {
            window.setAttribute(
                Qt::WA_ShowWithoutActivating,
                true
                );
        }

        window.show();

        if (startupPerformance.enabled)
        {
            startupProfiler.checkpoint(QStringLiteral("window-shown"));
        }

        if (preserveUpdateDialogFocus)
        {
            window.setAttribute(
                Qt::WA_ShowWithoutActivating,
                false
                );
        }

        splash.reset();
        splashLease->reset();
        updateController.setSplashScreen(nullptr);
        updateController.setStartupComplete();

        if (startupPerformance.enabled)
        {
            startupProfiler.checkpoint(QStringLiteral("startup-complete"));

            const auto finishPerformanceRun =
                [
                    &app,
                    &startupProfiler,
                    &startupPerformance,
                    progressUpdates,
                    progress
                ]()
                {
                    const bool metricsWritten =
                        writeStartupPerformanceMetrics(
                            startupPerformance.outputPath,
                            startupProfiler,
                            startupPerformance,
                            progressUpdates,
                            progress
                            );
                    app.exit(metricsWritten ? 0 : 2);
                };

            const int settleMilliseconds =
                startupPerformance.settleMilliseconds;
            if (settleMilliseconds >= 1000)
            {
                QTimer::singleShot(
                    1000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(QStringLiteral("settled-1s"));
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
                        startupProfiler.checkpoint(QStringLiteral("settled-5s"));
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
                        startupProfiler.checkpoint(QStringLiteral("settled-30s"));
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
                            QStringLiteral("elapsedMs=%1").arg(settleMilliseconds)
                            );
                    }
                    finishPerformanceRun();
                }
                );
        }
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
