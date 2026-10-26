#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <string>
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
    void failedActionRestoresPreferenceAndSuccessfulRetryPersists();

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

void MainWindowAutomaticUpdatePreferenceActionParityTests::
failedActionRestoresPreferenceAndSuccessfulRetryPersists()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = QString::fromUtf8(
        SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
        );
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    settings.set(key, true);
    settings.set(unrelatedKey, 53);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    auto automaticUpdatePreferences = std::make_shared<
        ClassMngr::Next::Platform::
            SettingsManagerAutomaticUpdatePreferencesPort
        >(
            [&injectAccessError, &synchronizationAttempts](QSettings& settings)
            {
                ++synchronizationAttempts;
                if (injectAccessError)
                {
                    return QSettings::AccessError;
                }

                settings.sync();
                return settings.status();
            }
            );

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.automaticUpdatePreferencesPort =
        automaticUpdatePreferences;
    int warningCount = 0;
    ClassMngr::Next::Domain::OperationError warningError;
    startupOptions.automaticUpdatePreferenceErrorHandler =
        [&warningCount, &warningError](
            const ClassMngr::Next::Domain::OperationError& error
            )
        {
            ++warningCount;
            warningError = error;
        };

    MainWindow window(
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    QAction* const action = window.actions().automaticallyCheckForUpdates;
    QVERIFY(action);
    AutomaticUpdatePreferenceRestorer preferenceRestorer(*action);
    QVERIFY(action->isCheckable());
    QVERIFY(action->isChecked());
    QVERIFY(automaticUpdatePreferences->read().automaticChecksEnabled);

    action->trigger();
    QVERIFY(action->isChecked());
    QVERIFY(automaticUpdatePreferences->read().automaticChecksEnabled);
    QVERIFY(settings.get(key).toBool());
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(warningCount, 1);
    QCOMPARE(
        warningError.code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(warningError.recoverable);
    QVERIFY(
        warningError.message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(settings.get(unrelatedKey).toInt(), 53);

    QSettings persistedSettings(
        m_settingsDirectory->filePath(
            QStringLiteral("PaperCloud/ClassMngr.ini")
            ),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(persistedSettings.value(key).toBool());
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 53);

    injectAccessError = false;
    action->trigger();
    QVERIFY(!action->isChecked());
    QVERIFY(!automaticUpdatePreferences->read().automaticChecksEnabled);
    QVERIFY(!settings.get(key).toBool());
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(warningCount, 1);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 53);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.value(key).toBool());
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 53);
}

QTEST_MAIN(MainWindowAutomaticUpdatePreferenceActionParityTests)

#include "mainwindow_automatic_update_preference_action_parity_tests.moc"
