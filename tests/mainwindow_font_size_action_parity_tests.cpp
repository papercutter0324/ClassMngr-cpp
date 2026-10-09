#include "app/mainwindow.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/platform/settings_manager_font_size_preferences_port.h"
#include "ui/shared/constants/options.h"

#include <QAction>
#include <QApplication>
#include <QTemporaryDir>
#include <QtTest>

#include <utility>

namespace
{
class ApplicationFontStateRestorer final
{
public:
    explicit ApplicationFontStateRestorer(QApplication& application)
        : m_application(application)
        , m_font(QApplication::font())
        , m_sizeOffset(FontManager::sizeOffset())
    {
    }

    ~ApplicationFontStateRestorer()
    {
        using PersistedFontSize =
            ClassMngr::Next::Application::FontSize;
        ClassMngr::Next::Platform::
            SettingsManagerFontSizePreferencesPort().write(
                PersistedFontSize::Normal
                );
        SettingsManager::instance().sync();

        FontManager::applyFontSize(
            m_application,
            QStringLiteral("en_US"),
            m_sizeOffset
            );
        m_application.setFont(m_font);
        FontManager::setSizeOffset(m_sizeOffset);
    }

    ApplicationFontStateRestorer(
        const ApplicationFontStateRestorer&
        ) = delete;
    ApplicationFontStateRestorer& operator=(
        const ApplicationFontStateRestorer&
        ) = delete;

private:
    QApplication& m_application;
    QFont m_font;
    int m_sizeOffset = 0;
};
}

class MainWindowFontSizeActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void allFontSizeActionsUpdateStateAndPersistPreference();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowFontSizeActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowFontSizeActionParityTests::
allFontSizeActionsUpdateStateAndPersistPreference()
{
    ApplicationFontStateRestorer fontStateRestorer(*qApp);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    using PersistedFontSize = ClassMngr::Next::Application::FontSize;
    using ClassMngr::Next::Platform::SettingsManagerFontSizePreferencesPort;
    const SettingsManagerFontSizePreferencesPort preferences;
    preferences.write(PersistedFontSize::Normal);
    SettingsManager::instance().sync();

    FontManager::setSizeOffset(fontSizeOffset(::FontSize::Normal));
    FontManager::applyGlobalFont(
        *qApp,
        languageService.loadedLocaleName()
        );

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    auto* const fontSizeState = window.actions().fontSizeState;
    QVERIFY(fontSizeState);
    QCOMPARE(fontSizeState->current(), ::FontSize::Normal);

    QAction* const smallAction =
        window.actions().fontSizeState->action(::FontSize::Small);
    QAction* const normalAction =
        window.actions().fontSizeState->action(::FontSize::Normal);
    QAction* const largeAction =
        window.actions().fontSizeState->action(::FontSize::Large);
    QAction* const extraLargeAction =
        window.actions().fontSizeState->action(::FontSize::ExtraLarge);
    QVERIFY(smallAction);
    QVERIFY(normalAction);
    QVERIFY(largeAction);
    QVERIFY(extraLargeAction);
    QVERIFY(normalAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    QVERIFY(!extraLargeAction->isChecked());

    smallAction->trigger();

    QCOMPARE(fontSizeState->current(), ::FontSize::Small);
    QVERIFY(smallAction->isChecked());
    QVERIFY(!normalAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    QVERIFY(!extraLargeAction->isChecked());
    SettingsManager::instance().sync();
    QVERIFY(preferences.read() == PersistedFontSize::Small);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(::FontSize::Small)
        );
    QCOMPARE(
        QApplication::font().pointSize(),
        FontManager::getPlatformFontSize()
            + fontSizeOffset(::FontSize::Small)
        );

    largeAction->trigger();

    QCOMPARE(fontSizeState->current(), ::FontSize::Large);
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!normalAction->isChecked());
    QVERIFY(largeAction->isChecked());
    QVERIFY(!extraLargeAction->isChecked());
    SettingsManager::instance().sync();
    QVERIFY(preferences.read() == PersistedFontSize::Large);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(::FontSize::Large)
        );
    QCOMPARE(
        QApplication::font().pointSize(),
        FontManager::getPlatformFontSize()
            + fontSizeOffset(::FontSize::Large)
        );

    extraLargeAction->trigger();

    QCOMPARE(fontSizeState->current(), ::FontSize::ExtraLarge);
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!normalAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    QVERIFY(extraLargeAction->isChecked());
    SettingsManager::instance().sync();
    QVERIFY(preferences.read() == PersistedFontSize::ExtraLarge);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(::FontSize::ExtraLarge)
        );
    QCOMPARE(
        QApplication::font().pointSize(),
        FontManager::getPlatformFontSize()
            + fontSizeOffset(::FontSize::ExtraLarge)
        );

    normalAction->trigger();

    QCOMPARE(fontSizeState->current(), ::FontSize::Normal);
    QVERIFY(!smallAction->isChecked());
    QVERIFY(normalAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    QVERIFY(!extraLargeAction->isChecked());
    SettingsManager::instance().sync();
    QVERIFY(preferences.read() == PersistedFontSize::Normal);
    QCOMPARE(
        FontManager::sizeOffset(),
        fontSizeOffset(::FontSize::Normal)
        );
    QCOMPARE(
        QApplication::font().pointSize(),
        FontManager::getPlatformFontSize()
            + fontSizeOffset(::FontSize::Normal)
        );
}

QTEST_MAIN(MainWindowFontSizeActionParityTests)

#include "mainwindow_font_size_action_parity_tests.moc"
