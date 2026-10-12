#include "core/settingsmanager.h"
#include "next/application/language_preferences_port.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString languageKey()
{
    return QString::fromUtf8(
        OptionKeys::Language
        );
}

} // namespace

class NextPlatformSettingsManagerLanguagePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalKey();
    void mapsCanonicalStoredLanguageValues();
    void migratesLegacyEnglishValuesToOne();
    void unknownValuesDefaultToSystemDefault();
    void writesTypedValuesAndClears();
    void unavailableSettingsDefaultToSystemDefault();
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsRoot.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
usesTheExactCanonicalKey()
{
    QCOMPARE(
        languageKey(),
        QStringLiteral("options/language")
        );

    SettingsManager::instance().set(
        languageKey(),
        5
        );

    const SettingsManagerLanguagePreferencesPort port;
    QCOMPARE(
        port.read(),
        LanguagePreference::Korean
        );
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
mapsCanonicalStoredLanguageValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerLanguagePreferencesPort port;

    settings.set(languageKey(), 0);
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );

    settings.set(languageKey(), 1);
    QCOMPARE(
        port.read(),
        LanguagePreference::English
        );
    QCOMPARE(settings.get(languageKey()).toInt(), 1);

    settings.set(languageKey(), 5);
    QCOMPARE(
        port.read(),
        LanguagePreference::Korean
        );
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
migratesLegacyEnglishValuesToOne()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerLanguagePreferencesPort port;

    for (const int legacyValue : {2, 3, 4})
    {
        settings.set(languageKey(), legacyValue);

        QCOMPARE(
            port.read(),
            LanguagePreference::English
            );
        QCOMPARE(settings.get(languageKey()).toInt(), 1);
    }
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
unknownValuesDefaultToSystemDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerLanguagePreferencesPort port;

    settings.remove(languageKey());
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );

    settings.set(languageKey(), -1);
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );
    QCOMPARE(settings.get(languageKey()).toInt(), -1);

    settings.set(languageKey(), 6);
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );
    QCOMPARE(settings.get(languageKey()).toInt(), 6);

    settings.set(languageKey(), QStringLiteral("unknown"));
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );
    QCOMPARE(
        settings.get(languageKey()).toString(),
        QStringLiteral("unknown")
        );
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
writesTypedValuesAndClears()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerLanguagePreferencesPort port;

    QVERIFY(port.write(LanguagePreference::SystemDefault));
    QCOMPARE(settings.get(languageKey()).toInt(), 0);

    QVERIFY(port.write(LanguagePreference::English));
    QCOMPARE(settings.get(languageKey()).toInt(), 1);

    QVERIFY(port.write(LanguagePreference::Korean));
    QCOMPARE(settings.get(languageKey()).toInt(), 5);
    QCOMPARE(port.read(), LanguagePreference::Korean);

    const auto invalidWrite = port.write(
        static_cast<LanguagePreference>(99)
        );
    QVERIFY(!invalidWrite);
    QCOMPARE(invalidWrite.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(settings.get(languageKey()).toInt(), 5);

    port.clear();
    QVERIFY(!settings.get(languageKey()).isValid());
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
unavailableSettingsDefaultToSystemDefault()
{
    SettingsManager::instance().set(
        languageKey(),
        QVariant()
        );

    const SettingsManagerLanguagePreferencesPort port;
    QCOMPARE(
        port.read(),
        LanguagePreference::SystemDefault
        );
    QVERIFY(!SettingsManager::instance().get(languageKey()).isValid());
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = languageKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.set(key, 1);
    settings.set(unrelatedKey, 41);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerLanguagePreferencesPort faultablePort(
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

    const auto failedWrite =
        faultablePort.write(LanguagePreference::Korean);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key).toInt(), 1);
    QCOMPARE(faultablePort.read(), LanguagePreference::English);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 1);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);

    injectAccessError = false;
    const auto retryResult = faultablePort.write(LanguagePreference::Korean);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key).toInt(), 5);
    QCOMPARE(faultablePort.read(), LanguagePreference::Korean);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 5);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);
}

void NextPlatformSettingsManagerLanguagePreferencesPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = languageKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.remove(key);
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerLanguagePreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite = faultablePort.write(LanguagePreference::Korean);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QVERIFY(!settings.get(key).isValid());
    QCOMPARE(faultablePort.read(), LanguagePreference::SystemDefault);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 29);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 29);
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerLanguagePreferencesPortTests
    )

#include "next_platform_settings_manager_language_preferences_port_tests.moc"
