#include "app/mainwindow.h"
#include "core/appsettings.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "ui/shared/widgets/splash/splashscreen.h"
#include "core/utils/platform.h"

#include <QApplication>
#include <QIcon>
#include <QTimer>
#include <QElapsedTimer>
#include <QLocale>
#include <QTranslator>

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



// =========================================================
// Main
// =========================================================

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName(AppSettings::ApplicationName);
    app.setOrganizationName(AppSettings::OrganizationName);
    app.setWindowIcon(getAppIcon());



    // =====================================================
    // Translation Support
    // =====================================================

    QTranslator translator;

    // Update this later to use: auto* translator = new QTranslator(&app);

    const QStringList uiLanguages =
        QLocale::system().uiLanguages();

    for (const QString &locale : uiLanguages)
    {
        const QString baseName =
            AppSettings::TranslationPrefix + QLocale(locale).name();

        if (translator.load(":/i18n/" + baseName))
        {
            app.installTranslator(&translator);
            break;
        }
    }



    // =====================================================
    // Fonts
    // =====================================================

    FontManager::applyGlobalFont(app);
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

    int progress = 0;

    const double step =
        100.0 / AppSettings::StartupProgressSteps;

    auto updateProgress =
        [&](const QString &message)
    {
        progress = qMin(
            static_cast<int>(progress + step),
            100
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

    MainWindow window(
        updateProgress,
        isAdminMode(app.arguments())
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

    auto finish = [&window, &splash]()
    {
        window.show();
        splash.close();
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
