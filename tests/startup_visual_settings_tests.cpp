#include "app/controllers/font_size_controller.h"
#include "app/controllers/theme_controller.h"
#include "core/fontmanager.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "next/platform/theme_preference_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QStyleHints>
#include <QtTest>

class StartupVisualSettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void controllerConnectionsLeaveStartupSettingsApplied();
    void themePreferencePortMapsTypedValuesAndRejectsInvalidInput();
    void unchangedThemeDoesNotRestyleWidgets();
};

void StartupVisualSettingsTests
    ::controllerConnectionsLeaveStartupSettingsApplied()
{
    auto* app =
        qobject_cast<QApplication*>(
            QCoreApplication::instance()
            );
    QVERIFY(app);

    SettingsManager& settings = SettingsManager::instance();
    const QString themeKey = QString::fromUtf8(OptionKeys::Theme);
    const QString fontSizeKey = QString::fromUtf8(OptionKeys::FontSize);
    const QVariant savedTheme = settings.get(themeKey);
    const QVariant savedFontSize = settings.get(fontSizeKey);

    settings.set(
        themeKey,
        static_cast<int>(Theme::Dark)
        );
    settings.set(
        fontSizeKey,
        fontSizeOffset(FontSize::ExtraLarge)
        );

    FontManager::setSizeOffset(
        fontSizeOffset(FontSize::ExtraLarge)
        );
    FontManager::applyGlobalFont(
        *app,
        QStringLiteral("en_US")
        );

    ThemeService themeService;
    themeService.setTheme(Theme::Light);

    QSignalSpy themeChanges(
        &themeService,
        static_cast<void (ThemeService::*)(Theme)>(
            &ThemeService::themeChanged
            )
        );

    ActionRegistry actions;
    actions.createActions();

    ThemeController themeController(themeService);
    FontSizeController fontSizeController(nullptr);
    themeController.connectActions(actions);
    fontSizeController.connectActions(actions);

    QCOMPARE(themeService.currentTheme(), Theme::Light);
    QCOMPARE(
        themeController.preferencesSnapshot().themePreference(),
        ClassMngr::Next::Application::ThemePreference::Dark
        );
    QCOMPARE(themeChanges.count(), 0);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(FontSize::ExtraLarge)
        );

    actions.themeState->set(Theme::Light);
    QCOMPARE(
        themeController.preferencesSnapshot().themePreference(),
        ClassMngr::Next::Application::ThemePreference::Light
        );
    actions.themeState->set(Theme::Dark);
    QCOMPARE(themeService.currentTheme(), Theme::Dark);
    QCOMPARE(
        themeController.preferencesSnapshot().themePreference(),
        ClassMngr::Next::Application::ThemePreference::Dark
        );

    actions.fontSizeState->set(FontSize::Normal);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(FontSize::Normal)
        );
    QCOMPARE(
        app->font().pointSize(),
        FontManager::getPlatformFontSize()
        );

    actions.fontSizeState->set(FontSize::ExtraLarge);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(FontSize::ExtraLarge)
        );
    QCOMPARE(
        app->font().pointSize(),
        FontManager::getPlatformFontSize()
            + fontSizeOffset(FontSize::ExtraLarge)
        );

    settings.set(themeKey, savedTheme);
    settings.set(fontSizeKey, savedFontSize);
    settings.sync();
}

void StartupVisualSettingsTests::
themePreferencePortMapsTypedValuesAndRejectsInvalidInput()
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app);

    ThemeService themeService;
    ClassMngr::Next::Platform::ThemePreferencePort port(themeService);
    QSignalSpy themeChanges(
        &themeService,
        static_cast<void (ThemeService::*)(Theme)>(
            &ThemeService::themeChanged
            )
        );

    QVERIFY(port.apply(
        ClassMngr::Next::Application::ThemePreference::SystemDefault
        ));
    const Theme systemTheme =
        app->styleHints()->colorScheme() == Qt::ColorScheme::Dark
            ? Theme::Dark
            : Theme::Light;
    QCOMPARE(themeService.currentTheme(), systemTheme);

    QVERIFY(port.apply(
        ClassMngr::Next::Application::ThemePreference::Light
        ));
    QCOMPARE(themeService.currentTheme(), Theme::Light);

    QVERIFY(port.apply(
        ClassMngr::Next::Application::ThemePreference::Dark
        ));
    QCOMPARE(themeService.currentTheme(), Theme::Dark);

    const int changesBeforeInvalid = themeChanges.count();
    const Theme themeBeforeInvalid = themeService.currentTheme();
    const auto invalid = port.apply(
        static_cast<ClassMngr::Next::Application::ThemePreference>(99)
        );
    QVERIFY(!invalid);
    QCOMPARE(
        invalid.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(themeService.currentTheme(), themeBeforeInvalid);
    QCOMPARE(themeChanges.count(), changesBeforeInvalid);

    ThemeController controller(themeService);
    const auto controllerSnapshotBeforeInvalid =
        controller.preferencesSnapshot();
    const int controllerChangesBeforeInvalid = themeChanges.count();
    const auto controllerInvalid = controller.changeTheme(
        static_cast<ClassMngr::Next::Application::ThemePreference>(99)
        );
    QVERIFY(!controllerInvalid);
    QVERIFY(
        controller.preferencesSnapshot()
        == controllerSnapshotBeforeInvalid
        );
    QCOMPARE(themeService.currentTheme(), themeBeforeInvalid);
    QCOMPARE(themeChanges.count(), controllerChangesBeforeInvalid);
}

void StartupVisualSettingsTests::unchangedThemeDoesNotRestyleWidgets()
{
    ThemeService themeService;
    themeService.setTheme(Theme::Dark);

    const QPalette darkPalette = qApp->palette();

    QWidget widget;
    widget.setProperty("theme", QStringLiteral("unchanged"));

    QSignalSpy themeChanges(
        &themeService,
        static_cast<void (ThemeService::*)(Theme)>(
            &ThemeService::themeChanged
            )
        );

    ClassMngr::Next::Platform::ThemePreferencePort port(themeService);
    QVERIFY(port.apply(
        ClassMngr::Next::Application::ThemePreference::Dark
        ));

    QCOMPARE(themeChanges.count(), 0);
    QCOMPARE(qApp->palette(), darkPalette);
    QCOMPARE(
        widget.property("theme").toString(),
        QStringLiteral("unchanged")
        );

    QVERIFY(port.apply(
        ClassMngr::Next::Application::ThemePreference::Light
        ));

    QCOMPARE(themeChanges.count(), 1);
    QCOMPARE(
        widget.property("theme").toString(),
        QStringLiteral("light")
        );
}

QTEST_MAIN(StartupVisualSettingsTests)

#include "startup_visual_settings_tests.moc"
