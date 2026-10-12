#include "core/settingsmanager.h"
#include "next/application/font_size_preferences.h"
#include "next/platform/settings_manager_font_size_preferences_port.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString fontSizeKey()
{
    return QString::fromUtf8(
        OptionKeys::FontSize
        );
}

} // namespace

class NextPlatformSettingsManagerFontSizePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactLegacyKey();
    void mapsAllStoredFontSizeValues();
    void writesAllFontSizeValuesForRoundTrip();
    void missingAndUnknownValuesDefaultToNormal();
    void unavailableSettingsDefaultToNormal();
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
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

void NextPlatformSettingsManagerFontSizePreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
usesTheExactLegacyKey()
{
    QCOMPARE(
        fontSizeKey(),
        QStringLiteral("options/fontSize")
        );

    SettingsManager::instance().set(
        fontSizeKey(),
        4
        );

    const SettingsManagerFontSizePreferencesPort port;
    QCOMPARE(
        port.read(),
        FontSize::ExtraLarge
        );
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
mapsAllStoredFontSizeValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerFontSizePreferencesPort port;

    const FontSize expectedFontSizes[] = {
        FontSize::Small,
        FontSize::Normal,
        FontSize::Large,
        FontSize::ExtraLarge
    };
    const int storedValues[] = { -2, 0, 2, 4 };

    for (int index = 0; index < 4; ++index)
    {
        settings.set(fontSizeKey(), storedValues[index]);
        QCOMPARE(
            port.read(),
            expectedFontSizes[index]
            );
    }
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
writesAllFontSizeValuesForRoundTrip()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerFontSizePreferencesPort port;

    const FontSize expectedFontSizes[] = {
        FontSize::Small,
        FontSize::Normal,
        FontSize::Large,
        FontSize::ExtraLarge
    };
    const int expectedStoredValues[] = {-2, 0, 2, 4};

    for (int index = 0; index < 4; ++index)
    {
        QVERIFY(port.write(expectedFontSizes[index]));
        QCOMPARE(
            settings.get(fontSizeKey()).toInt(),
            expectedStoredValues[index]
            );
        QCOMPARE(
            port.read(),
            expectedFontSizes[index]
            );
    }

    const auto invalidWrite = port.write(
        static_cast<FontSize>(99)
        );
    QVERIFY(!invalidWrite);
    QCOMPARE(
        invalidWrite.error().code,
        Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        settings.get(fontSizeKey()).toInt(),
        4
        );
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
missingAndUnknownValuesDefaultToNormal()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerFontSizePreferencesPort port;

    settings.remove(fontSizeKey());
    QCOMPARE(
        port.read(),
        FontSize::Normal
        );

    settings.set(fontSizeKey(), -4);
    QCOMPARE(
        port.read(),
        FontSize::Normal
        );

    settings.set(fontSizeKey(), 1);
    QCOMPARE(
        port.read(),
        FontSize::Normal
        );

    settings.set(fontSizeKey(), 6);
    QCOMPARE(
        port.read(),
        FontSize::Normal
        );

    settings.set(fontSizeKey(), QStringLiteral("unknown"));
    QCOMPARE(
        port.read(),
        FontSize::Normal
        );
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
unavailableSettingsDefaultToNormal()
{
    SettingsManager::instance().set(
        fontSizeKey(),
        QVariant()
        );

    const SettingsManagerFontSizePreferencesPort port;
    QCOMPARE(
        port.read(),
        FontSize::Normal
        );
    QVERIFY(!SettingsManager::instance().get(fontSizeKey()).isValid());
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = fontSizeKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.set(key, 2);
    settings.set(unrelatedKey, 41);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerFontSizePreferencesPort faultablePort(
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

    const auto failedWrite = faultablePort.write(FontSize::ExtraLarge);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key).toInt(), 2);
    QCOMPARE(faultablePort.read(), FontSize::Large);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 2);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);

    injectAccessError = false;
    const auto retryResult = faultablePort.write(FontSize::ExtraLarge);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key).toInt(), 4);
    QCOMPARE(faultablePort.read(), FontSize::ExtraLarge);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 4);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);
}

void NextPlatformSettingsManagerFontSizePreferencesPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = fontSizeKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.remove(key);
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerFontSizePreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite = faultablePort.write(FontSize::Large);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QVERIFY(!settings.get(key).isValid());
    QCOMPARE(faultablePort.read(), FontSize::Normal);
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
    NextPlatformSettingsManagerFontSizePreferencesPortTests
    )

#include "next_platform_settings_manager_font_size_preferences_port_tests.moc"
