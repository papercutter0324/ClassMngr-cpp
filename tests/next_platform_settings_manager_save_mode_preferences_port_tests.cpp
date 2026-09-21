#include "core/settingsmanager.h"
#include "next/application/save_mode_preferences.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString saveModeKey()
{
    return QString::fromUtf8(
        OptionKeys::SaveMode
        );
}

} // namespace

class NextPlatformSettingsManagerSaveModePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalKey();
    void mapsBothStoredSaveModes();
    void missingMalformedUnknownAndUnavailableDefaultToAutomatic();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
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

void NextPlatformSettingsManagerSaveModePreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
usesTheExactCanonicalKey()
{
    QCOMPARE(
        saveModeKey(),
        QStringLiteral("options/saveMode")
        );

    SettingsManager::instance().set(
        saveModeKey(),
        1
        );

    const SettingsManagerSaveModePreferencesPort port;
    QCOMPARE(
        port.read(),
        SaveMode::Manual
        );
}

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
mapsBothStoredSaveModes()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerSaveModePreferencesPort port;

    const int storedValues[] = {0, 1};
    const SaveMode expectedValues[] = {
        SaveMode::Automatic,
        SaveMode::Manual
    };

    for (int index = 0; index < 2; ++index)
    {
        settings.set(
            saveModeKey(),
            storedValues[index]
            );
        QCOMPARE(
            port.read(),
            expectedValues[index]
            );
    }
}

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
missingMalformedUnknownAndUnavailableDefaultToAutomatic()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerSaveModePreferencesPort port;

    settings.remove(saveModeKey());
    QCOMPARE(
        port.read(),
        SaveMode::Automatic
        );

    settings.set(
        saveModeKey(),
        QStringLiteral("not-a-save-mode")
        );
    QCOMPARE(
        port.read(),
        SaveMode::Automatic
        );

    for (const int value : {-1, 2, 99})
    {
        settings.set(
            saveModeKey(),
            value
            );
        QCOMPARE(
            port.read(),
            SaveMode::Automatic
            );
    }

    settings.set(
        saveModeKey(),
        QVariant()
        );
    QCOMPARE(
        port.read(),
        SaveMode::Automatic
        );
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerSaveModePreferencesPortTests
    )

#include "next_platform_settings_manager_save_mode_preferences_port_tests.moc"
