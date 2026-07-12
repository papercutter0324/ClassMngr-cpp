#include "app/mainwindow.h"
#include "core/appsettings.h"
#include "core/build_info.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/resource_paths.h"
#include "core/settingsmanager.h"
#include "ui/shared/widgets/splash/splashscreen.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"
#include "core/utils/platform.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

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
        QStringLiteral("progressUpdates"),
        progressUpdates
        );
    metrics.insert(
        QStringLiteral("finalProgress"),
        finalProgress
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
    QElapsedTimer processStartupTimer;

    processStartupTimer.start();

    QApplication app(argc, argv);

    const StartupPerformanceMode startupPerformance =
        startupPerformanceMode(
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
        {
            .loadMostRecentDatabase = !startupPerformance.enabled,
            .runPostShowStartupTasks = !startupPerformance.enabled
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
            &startupPerformance,
            &processStartupTimer,
            processStartToWindowConstructedMs,
            progressUpdates,
            progress
        ]()
    {
        window.show();
        splash.close();

        if (startupPerformance.enabled)
        {
            const bool metricsWritten =
                writeStartupPerformanceMetrics(
                    startupPerformance.outputPath,
                    processStartToWindowConstructedMs,
                    processStartupTimer.elapsed(),
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
