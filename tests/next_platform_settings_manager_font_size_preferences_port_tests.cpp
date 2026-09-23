#include "core/settingsmanager.h"
#include "next/application/font_size_preferences.h"
#include "next/platform/settings_manager_font_size_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

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
        port.write(expectedFontSizes[index]);
        QCOMPARE(
            settings.get(fontSizeKey()).toInt(),
            expectedStoredValues[index]
            );
        QCOMPARE(
            port.read(),
            expectedFontSizes[index]
            );
    }

    port.write(
        static_cast<FontSize>(99)
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

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerFontSizePreferencesPortTests
    )

#include "next_platform_settings_manager_font_size_preferences_port_tests.moc"
