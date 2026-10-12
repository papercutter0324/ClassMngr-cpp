#include "core/settingsmanager.h"
#include "next/application/save_mode_preferences.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

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
    void writesBothSaveModesForRoundTrip();
    void invalidTypedSaveModeReturnsInvalidInputAndPreservesStoredValue();
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();
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
writesBothSaveModesForRoundTrip()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerSaveModePreferencesPort port;

    QVERIFY(port.write(SaveMode::Manual));
    QCOMPARE(settings.get(saveModeKey()).toInt(), 1);
    QCOMPARE(port.read(), SaveMode::Manual);

    QVERIFY(port.write(SaveMode::Automatic));
    QCOMPARE(settings.get(saveModeKey()).toInt(), 0);
    QCOMPARE(port.read(), SaveMode::Automatic);
}

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
invalidTypedSaveModeReturnsInvalidInputAndPreservesStoredValue()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerSaveModePreferencesPort port;

    settings.set(saveModeKey(), 1);
    const auto result = port.write(static_cast<SaveMode>(17));

    QVERIFY(!result);
    QCOMPARE(
        result.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(settings.get(saveModeKey()).toInt(), 1);
    QCOMPARE(port.read(), SaveMode::Manual);
}

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString unrelatedKey = QStringLiteral("options/unrelated");
    settings.set(saveModeKey(), 0);
    settings.set(unrelatedKey, 41);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerSaveModePreferencesPort faultablePort(
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

    const auto failedWrite = faultablePort.write(SaveMode::Manual);
    QVERIFY(!failedWrite);
    QCOMPARE(
        failedWrite.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(saveModeKey()).toInt(), 0);
    QCOMPARE(faultablePort.read(), SaveMode::Automatic);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(saveModeKey()).toInt(), 0);

    injectAccessError = false;
    const auto retryResult = faultablePort.write(SaveMode::Manual);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(saveModeKey()).toInt(), 1);
    QCOMPARE(faultablePort.read(), SaveMode::Manual);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(saveModeKey()).toInt(), 1);
}

void NextPlatformSettingsManagerSaveModePreferencesPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString unrelatedKey = QStringLiteral("options/unrelated");
    settings.remove(saveModeKey());
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerSaveModePreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite = faultablePort.write(SaveMode::Manual);
    QVERIFY(!failedWrite);
    QCOMPARE(
        failedWrite.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(!settings.get(saveModeKey()).isValid());
    QCOMPARE(faultablePort.read(), SaveMode::Automatic);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 29);
    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(saveModeKey()));
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
