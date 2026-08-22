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

#if defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#endif

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
};

struct ProcessMemoryMetrics
{
    qint64 workingSetBytes = -1;
    qint64 privateBytes = -1;
};

ProcessMemoryMetrics currentProcessMemoryMetrics()
{
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);

    if (
        GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)
            )
        )
    {
        return {
            static_cast<qint64>(counters.WorkingSetSize),
            static_cast<qint64>(counters.PrivateUsage)
        };
    }
#endif

    return {};
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

    return mode;
}

bool writeStartupPerformanceMetrics(
    const QString& outputPath,
    qint64 processStartToWindowConstructedMs,
    qint64 processStartToReadyMs,
    ProcessMemoryMetrics windowConstructedMemory,
    ProcessMemoryMetrics readyMemory,
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

    QJsonObject metrics;

    metrics.insert(
        QStringLiteral("processStartToWindowConstructedMs"),
        static_cast<double>(processStartToWindowConstructedMs)
        );
    metrics.insert(
        QStringLiteral("processStartToReadyMs"),
        static_cast<double>(processStartToReadyMs)
        );
    metrics.insert(
        QStringLiteral("windowConstructedWorkingSetBytes"),
        static_cast<double>(windowConstructedMemory.workingSetBytes)
        );
    metrics.insert(
        QStringLiteral("windowConstructedPrivateBytes"),
        static_cast<double>(windowConstructedMemory.privateBytes)
        );
    metrics.insert(
        QStringLiteral("progressUpdates"),
        progressUpdates
        );
    metrics.insert(
        QStringLiteral("finalProgress"),
        finalProgress
        );

    QJsonArray checkpoints;
    checkpoints.append(
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("window-constructed")},
            {QStringLiteral("elapsedMs"), static_cast<double>(processStartToWindowConstructedMs)},
            {QStringLiteral("workingSetBytes"), static_cast<double>(windowConstructedMemory.workingSetBytes)},
            {QStringLiteral("privateBytes"), static_cast<double>(windowConstructedMemory.privateBytes)}
        }
        );
    checkpoints.append(
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("ready")},
            {QStringLiteral("elapsedMs"), static_cast<double>(processStartToReadyMs)},
            {QStringLiteral("workingSetBytes"), static_cast<double>(readyMemory.workingSetBytes)},
            {QStringLiteral("privateBytes"), static_cast<double>(readyMemory.privateBytes)}
        }
        );
    metrics.insert(QStringLiteral("format"), QStringLiteral("classmngr-scenario-report-v1"));
    metrics.insert(
        QStringLiteral("scenario"),
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("startup-empty-profile")},
            {QStringLiteral("actions"), QJsonArray{
                QStringLiteral("launch with an empty settings profile"),
                QStringLiteral("construct the main window"),
                QStringLiteral("wait until the startup-ready checkpoint")
            }}
        }
        );
    metrics.insert(QStringLiteral("checkpoints"), checkpoints);

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
    QElapsedTimer processStartupTimer;

    processStartupTimer.start();

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

    const StartupPerformanceMode startupPerformance =
        startupPerformanceMode(
            app.arguments()
            );

    const QString initialDatabasePath =
        startupPerformance.enabled
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

    languageService.setLanguage(
        LanguageService::savedLanguage()
        );



    // =====================================================
    // Fonts
    // =====================================================

    const FontSize savedFontSize =
        fontSizeFromStoredValue(
            SettingsManager::instance().get(
                OptionKeys::FontSize,
                fontSizeOffset(FontSize::Normal)
                ).toInt()
            );

    FontManager::setSizeOffset(
        fontSizeOffset(savedFontSize)
        );

    FontManager::applyGlobalFont(
        app,
        languageService.loadedLocaleName()
        );
    // FontManager::debugDump();


    // =====================================================
    // Splash Screen
    // =====================================================

    SplashScreen splash;

    splash.centerOnScreen();
    splash.show();

    app.processEvents();



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

        splash.setMessage(message);
        splash.setProgress(progress);

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
        &splash
        );

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Checking for updates..."
            )
        );

    if (!startupPerformance.enabled)
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

    if (
        const Status resourcePackStatus =
            ResourcePackManager::instance().initialize();
        !resourcePackStatus
        )
    {
        qWarning().noquote()
            << resourcePackStatus.error();
    }

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
            .loadMostRecentDatabase = !startupPerformance.enabled,
            .runPostShowStartupTasks = !startupPerformance.enabled,
            .initialDatabasePath = initialDatabasePath
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

    const qint64 processStartToWindowConstructedMs =
        processStartupTimer.elapsed();
    const ProcessMemoryMetrics windowConstructedMemory =
        currentProcessMemoryMetrics();



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
            &updateController,
            &startupPerformance,
            &processStartupTimer,
            processStartToWindowConstructedMs,
            windowConstructedMemory,
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

        if (preserveUpdateDialogFocus)
        {
            window.setAttribute(
                Qt::WA_ShowWithoutActivating,
                false
                );
        }

        splash.close();
        updateController.setStartupComplete();

        if (startupPerformance.enabled)
        {
            const bool metricsWritten =
                writeStartupPerformanceMetrics(
                    startupPerformance.outputPath,
                    processStartToWindowConstructedMs,
                    processStartupTimer.elapsed(),
                    windowConstructedMemory,
                    currentProcessMemoryMetrics(),
                    progressUpdates,
                    progress
                    );

            QTimer::singleShot(
                0,
                &app,
                [&app, metricsWritten]()
                {
                    app.exit(metricsWritten ? 0 : 2);
                }
                );
        }
    };

    auto startFinish = [&splash, &finish]()
    {
        splash.fadeOut(finish);
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
