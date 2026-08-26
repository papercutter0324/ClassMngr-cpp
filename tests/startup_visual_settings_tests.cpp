#include "app/controllers/font_size_controller.h"
#include "app/controllers/theme_controller.h"
#include "core/fontmanager.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QtTest>

class StartupVisualSettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void controllerConnectionsLeaveStartupSettingsApplied();
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

    ActionRegistry actions;
    actions.createActions();

    ThemeController themeController(&themeService);
    FontSizeController fontSizeController(nullptr);
    themeController.connectActions(actions);
    fontSizeController.connectActions(actions);

    QCOMPARE(themeService.currentTheme(), Theme::Light);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(FontSize::ExtraLarge)
        );

    actions.themeState->set(Theme::Light);
    actions.themeState->set(Theme::Dark);
    QCOMPARE(themeService.currentTheme(), Theme::Dark);

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

QTEST_MAIN(StartupVisualSettingsTests)

#include "startup_visual_settings_tests.moc"
