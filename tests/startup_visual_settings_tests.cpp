#include "app/controllers/font_size_controller.h"
#include "app/controllers/theme_controller.h"
#include "core/fontmanager.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "next/application/theme_preferences_port.h"
#include "next/platform/theme_preference_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QAction>
#include <QPalette>
#include <QStyleHints>
#include <QtTest>

#include <optional>
#include <string>
#include <utility>

namespace
{
class InMemoryThemePreferencesPort final
    : public ClassMngr::Next::Application::ThemePreferencesPort
{
public:
    using ThemePreference = ClassMngr::Next::Application::Theme;

    [[nodiscard]] ThemePreference read() const override
    {
        return m_storedTheme;
    }

    [[nodiscard]] ClassMngr::Next::Domain::Result<void> write(
        const ThemePreference theme
        ) const override
    {
        ++m_writeCount;
        if (m_writeFailure.has_value())
        {
            return ClassMngr::Next::Domain::Result<void>::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = *m_writeFailure,
                .recoverable = true
            });
        }

        m_storedTheme = theme;
        return ClassMngr::Next::Domain::Result<void>::success();
    }

    void setWriteFailure(std::string message)
    {
        m_writeFailure = std::move(message);
    }

    void clearWriteFailure()
    {
        m_writeFailure.reset();
    }

    [[nodiscard]] int writeCount() const
    {
        return m_writeCount;
    }

private:
    mutable ThemePreference m_storedTheme = ThemePreference::Dark;
    mutable int m_writeCount = 0;
    std::optional<std::string> m_writeFailure;
};

class ApplicationVisualStateRestorer final
{
public:
    explicit ApplicationVisualStateRestorer(QApplication& application)
        : m_application(application)
        , m_palette(application.palette())
        , m_stylesheet(application.styleSheet())
    {
    }

    ~ApplicationVisualStateRestorer()
    {
        m_application.setPalette(m_palette);
        m_application.setStyleSheet(m_stylesheet);
    }

    ApplicationVisualStateRestorer(
        const ApplicationVisualStateRestorer&
        ) = delete;
    ApplicationVisualStateRestorer& operator=(
        const ApplicationVisualStateRestorer&
        ) = delete;

private:
    QApplication& m_application;
    QPalette m_palette;
    QString m_stylesheet;
};
} // namespace

class StartupVisualSettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void controllerConnectionsLeaveStartupSettingsApplied();
    void themePreferencePortMapsTypedValuesAndRejectsInvalidInput();
    void themeSaveFailureRollsBackAndCanBeRetried();
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

void StartupVisualSettingsTests::themeSaveFailureRollsBackAndCanBeRetried()
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app);
    ApplicationVisualStateRestorer restoreApplicationVisualState(*app);

    ThemeService themeService;
    themeService.setTheme(Theme::Dark);
    QSignalSpy themeChanges(
        &themeService,
        static_cast<void (ThemeService::*)(Theme)>(
            &ThemeService::themeChanged
            )
        );

    ActionRegistry actions;
    actions.createActions();
    QVERIFY(actions.themeState);
    actions.themeState->set(Theme::Dark);

    InMemoryThemePreferencesPort preferences;
    const std::string diagnostic = "injected theme preference write failure";
    preferences.setWriteFailure(diagnostic);

    int failureCallbackCount = 0;
    bool callbackObservedRestoredSelection = false;
    ClassMngr::Next::Domain::OperationError callbackError;
    ThemeController controller(
        themeService,
        preferences,
        [&failureCallbackCount,
         &callbackObservedRestoredSelection,
         &callbackError,
         &actions](
            const ClassMngr::Next::Domain::OperationError& error
            )
        {
            ++failureCallbackCount;
            callbackError = error;
            callbackObservedRestoredSelection =
                actions.themeState
                && actions.themeState->current() == Theme::Dark
                && actions.themeState->action(Theme::Dark)->isChecked()
                && !actions.themeState->action(Theme::Light)->isChecked();
        }
        );
    controller.connectActions(actions);

    const auto snapshotBeforeFailure = controller.preferencesSnapshot();
    QAction* const darkAction = actions.themeState->action(Theme::Dark);
    QAction* const lightAction = actions.themeState->action(Theme::Light);
    QAction* const systemDefaultAction =
        actions.themeState->action(Theme::SystemDefault);
    QVERIFY(darkAction);
    QVERIFY(lightAction);
    QVERIFY(systemDefaultAction);
    QCOMPARE(actions.themeState->current(), Theme::Dark);
    QCOMPARE(preferences.read(),
        ClassMngr::Next::Application::Theme::Dark);

    lightAction->trigger();

    QCOMPARE(failureCallbackCount, 1);
    QVERIFY(callbackObservedRestoredSelection);
    QCOMPARE(callbackError.code, ClassMngr::Next::Domain::ErrorCode::Technical);
    QVERIFY(callbackError.recoverable);
    QVERIFY(callbackError.message == diagnostic);
    QCOMPARE(preferences.writeCount(), 1);
    QCOMPARE(preferences.read(),
        ClassMngr::Next::Application::Theme::Dark);
    QVERIFY(controller.preferencesSnapshot() == snapshotBeforeFailure);
    QCOMPARE(themeService.currentTheme(), Theme::Dark);
    QCOMPARE(themeChanges.count(), 0);
    QCOMPARE(actions.themeState->current(), Theme::Dark);
    QVERIFY(darkAction->isChecked());
    QVERIFY(!lightAction->isChecked());
    QVERIFY(!systemDefaultAction->isChecked());

    preferences.clearWriteFailure();
    lightAction->trigger();

    QCOMPARE(failureCallbackCount, 1);
    QCOMPARE(preferences.writeCount(), 2);
    QCOMPARE(preferences.read(),
        ClassMngr::Next::Application::Theme::Light);
    QCOMPARE(
        controller.preferencesSnapshot().themePreference(),
        ClassMngr::Next::Application::ThemePreference::Light
        );
    QCOMPARE(themeService.currentTheme(), Theme::Light);
    QCOMPARE(themeChanges.count(), 1);
    QCOMPARE(actions.themeState->current(), Theme::Light);
    QVERIFY(!darkAction->isChecked());
    QVERIFY(lightAction->isChecked());
    QVERIFY(!systemDefaultAction->isChecked());
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
