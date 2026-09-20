#include "core/settingsmanager.h"
#include "next/application/language_preferences_port.h"
#include "next/platform/settings_manager_language_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

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

    port.write(LanguagePreference::SystemDefault);
    QCOMPARE(settings.get(languageKey()).toInt(), 0);

    port.write(LanguagePreference::English);
    QCOMPARE(settings.get(languageKey()).toInt(), 1);

    port.write(LanguagePreference::Korean);
    QCOMPARE(settings.get(languageKey()).toInt(), 5);
    QCOMPARE(port.read(), LanguagePreference::Korean);

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

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerLanguagePreferencesPortTests
    )

#include "next_platform_settings_manager_language_preferences_port_tests.moc"
