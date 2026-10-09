#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "next/platform/settings_manager_theme_preferences_port.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

namespace
{
class ApplicationThemeStateRestorer final
{
public:
    ApplicationThemeStateRestorer(
        QApplication& application,
        MainWindow& window,
        const QPalette& palette,
        const QString& stylesheet
        )
        : m_application(application)
        , m_window(window)
        , m_palette(palette)
        , m_stylesheet(stylesheet)
    {
    }

    ~ApplicationThemeStateRestorer()
    {
        if (m_window.actions().themeState)
        {
            m_window.actions().themeState->set(Theme::Light);
        }

        if (
            m_window.services()
            && m_window.services()->themeService()
            )
        {
            m_window.services()->themeService()->setTheme(Theme::Light);
        }

        m_application.setPalette(m_palette);
        m_application.setStyleSheet(m_stylesheet);
    }

    ApplicationThemeStateRestorer(
        const ApplicationThemeStateRestorer&
        ) = delete;
    ApplicationThemeStateRestorer& operator=(
        const ApplicationThemeStateRestorer&
        ) = delete;

private:
    QApplication& m_application;
    MainWindow& m_window;
    QPalette m_palette;
    QString m_stylesheet;
};
}

class MainWindowThemeActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void darkThemeActionAndLightRestorationUpdateApplicationState();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowThemeActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowThemeActionParityTests::
darkThemeActionAndLightRestorationUpdateApplicationState()
{
    const QPalette initialPalette = QApplication::palette();
    const QString initialStylesheet = qApp->styleSheet();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    using PersistedTheme = ClassMngr::Next::Application::Theme;
    using ClassMngr::Next::Platform::SettingsManagerThemePreferencesPort;
    const SettingsManagerThemePreferencesPort preferences;
    preferences.write(PersistedTheme::Light);
    SettingsManager::instance().sync();

    auto startupThemeService = std::make_unique<ThemeService>();
    startupThemeService->setTheme(Theme::Light);

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.startupThemeService = std::move(startupThemeService);

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    ApplicationThemeStateRestorer themeStateRestorer(
        *qApp,
        window,
        initialPalette,
        initialStylesheet
        );

    ThemeService* const themeService =
        window.services()->themeService();
    QVERIFY(themeService);
    QCOMPARE(themeService->currentTheme(), Theme::Light);

    auto* const themeState = window.actions().themeState;
    QVERIFY(themeState);
    QCOMPARE(themeState->current(), Theme::Light);

    QAction* const darkAction = themeState->action(Theme::Dark);
    QAction* const lightAction = themeState->action(Theme::Light);
    QVERIFY(darkAction);
    QVERIFY(lightAction);
    QVERIFY(darkAction->isEnabled());
    QVERIFY(!darkAction->isChecked());
    QVERIFY(lightAction->isChecked());

    darkAction->trigger();

    QCOMPARE(themeState->current(), Theme::Dark);
    QVERIFY(darkAction->isChecked());
    QVERIFY(!lightAction->isChecked());
    QCOMPARE(preferences.read(), PersistedTheme::Dark);
    QCOMPARE(themeService->currentTheme(), Theme::Dark);
    QCOMPARE(
        QApplication::palette().color(QPalette::Window),
        QColor("#202326")
        );
    QCOMPARE(window.property("theme").toString(), QStringLiteral("dark"));

    lightAction->trigger();

    QCOMPARE(themeState->current(), Theme::Light);
    QVERIFY(lightAction->isChecked());
    QVERIFY(!darkAction->isChecked());
    QCOMPARE(preferences.read(), PersistedTheme::Light);
    QCOMPARE(themeService->currentTheme(), Theme::Light);
    QCOMPARE(
        QApplication::palette().color(QPalette::Window),
        QColor("#eff0f1")
        );
    QCOMPARE(window.property("theme").toString(), QStringLiteral("light"));
}

QTEST_MAIN(MainWindowThemeActionParityTests)

#include "mainwindow_theme_action_parity_tests.moc"
