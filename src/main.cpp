#include "app/mainwindow.h"
#include "core/fontmanager.h"
#include "widgets/splash/splashscreen.h"
#include "utils/platform.h"

#include <QApplication>
#include <QIcon>
#include <QTimer>
#include <QElapsedTimer>
#include <QLocale>
#include <QTranslator>



// =========================================================
// Helpers
// =========================================================

QIcon getAppIcon()
{
    Platform usrPlatform = getPlatform();

    if (usrPlatform == Platform::WINDOWS)
    {
        return QIcon(":/icons/app_icon.ico");
    }

    return QIcon(":/icons/icon_256x256.png");
}


bool isAdminMode(const QStringList &args)
{
    return args.contains("--enable-admin");
}



// =========================================================
// Main
// =========================================================

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("ClassMngr");
    app.setWindowIcon(getAppIcon());



    // =====================================================
    // Translation Support
    // =====================================================

    QTranslator translator;

    const QStringList uiLanguages =
        QLocale::system().uiLanguages();

    for (const QString &locale : uiLanguages)
    {
        const QString baseName =
            "ClassMngr_" + QLocale(locale).name();

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
    FontManager::debugDump();



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

    constexpr int TOTAL_STEPS = 8;

    const double step =
        100.0 / TOTAL_STEPS;

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

    constexpr int MIN_SPLASH_MS = 2000;

    int elapsed =
        static_cast<int>(
            startupTimer.elapsed()
            );

    int remaining =
        qMax(0, MIN_SPLASH_MS - elapsed);



    // =====================================================
    // Finish Startup
    // =====================================================

    auto finish =
        [&]()
    {
        window.show();
        splash.close();
    };

    auto startFinish =
        [&]()
    {
        splash.fadeOut(finish);
    };

    QTimer::singleShot(
        remaining,
        startFinish
        );



    // =====================================================
    // Run App
    // =====================================================

    return app.exec();
}