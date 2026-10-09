#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

namespace
{
class AutomaticUpdatePreferenceRestorer final
{
public:
    explicit AutomaticUpdatePreferenceRestorer(QAction& action)
        : m_action(action),
          m_originalEnabled(
              ClassMngr::Next::Platform::
                  SettingsManagerAutomaticUpdatePreferencesPort()
                  .read()
                  .automaticChecksEnabled
              )
    {
    }

    ~AutomaticUpdatePreferenceRestorer()
    {
        m_action.setChecked(m_originalEnabled);
        SettingsManager::instance().sync();
    }

    AutomaticUpdatePreferenceRestorer(
        const AutomaticUpdatePreferenceRestorer&
        ) = delete;
    AutomaticUpdatePreferenceRestorer& operator=(
        const AutomaticUpdatePreferenceRestorer&
        ) = delete;

private:
    QAction& m_action;
    bool m_originalEnabled = true;
};
}

class MainWindowAutomaticUpdatePreferenceActionParityTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void actionPersistsDisabledAndEnabledPreferences();

private:
    std::unique_ptr<QTemporaryDir> m_settingsDirectory;
};

void MainWindowAutomaticUpdatePreferenceActionParityTests::initTestCase()
{
    const QString isolatedTempRoot =
        QDir::cleanPath(qEnvironmentVariable("TMP"));
    QVERIFY(!isolatedTempRoot.isEmpty());
    QCOMPARE(
        QDir::cleanPath(qEnvironmentVariable("TEMP")),
        isolatedTempRoot
        );
    QCOMPARE(
        QDir::cleanPath(qEnvironmentVariable("TMPDIR")),
        isolatedTempRoot
        );
    QCOMPARE(
        QDir::cleanPath(
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation
                )
            ),
        isolatedTempRoot
        );
    QVERIFY(QDir().mkpath(isolatedTempRoot));
    m_settingsDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settingsDirectory->isValid());

    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory->path().toUtf8()
            )
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowAutomaticUpdatePreferenceActionParityTests::
actionPersistsDisabledAndEnabledPreferences()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    QAction* const action = window.actions().automaticallyCheckForUpdates;
    QVERIFY(action);
    AutomaticUpdatePreferenceRestorer preferenceRestorer(*action);

    const ClassMngr::Next::Platform::
        SettingsManagerAutomaticUpdatePreferencesPort preferences;
    QVERIFY(action->isEnabled());
    QVERIFY(action->isCheckable());
    QVERIFY(action->isChecked());
    QVERIFY(preferences.read().automaticChecksEnabled);

    action->trigger();
    QVERIFY(!action->isChecked());
    QVERIFY(!preferences.read().automaticChecksEnabled);

    action->trigger();
    QVERIFY(action->isChecked());
    QVERIFY(preferences.read().automaticChecksEnabled);
}

QTEST_MAIN(MainWindowAutomaticUpdatePreferenceActionParityTests)

#include "mainwindow_automatic_update_preference_action_parity_tests.moc"
