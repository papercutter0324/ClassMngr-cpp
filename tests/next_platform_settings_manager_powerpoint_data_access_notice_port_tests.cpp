#include "core/settingsmanager.h"
#include "next/application/powerpoint_data_access_notice_preferences.h"
#include "next/platform/settings_manager_powerpoint_data_access_notice_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString powerPointDataAccessNoticeKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::SHOW_POWERPOINT_DATA_ACCESS_NOTICE
        );
}

} // namespace

class NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void missingSettingUsesEnabledDefault();
    void preservesLegacySettingsKey();
    void preservesLegacyQVariantCoercion();
    void unavailableSettingUsesEnabledDefault();
    void roundTripsThroughSettingsManager();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::
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

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::
missingSettingUsesEnabledDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(powerPointDataAccessNoticeKey());

    const SettingsManagerPowerPointDataAccessNoticePort port;
    const PowerPointDataAccessNoticePreferences preferences = port.read();

    QVERIFY(preferences.showPowerPointDataAccessNotice);
    QVERIFY(!settings.get(powerPointDataAccessNoticeKey()).isValid());
}

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::
preservesLegacySettingsKey()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = powerPointDataAccessNoticeKey();
    const SettingsManagerPowerPointDataAccessNoticePort port;

    QCOMPARE(key, QStringLiteral("options/showPowerPointDataAccessNotice"));
    settings.set(QStringLiteral("options/unrelatedValue"), 17);

    port.write({.showPowerPointDataAccessNotice = false});

    QCOMPARE(settings.get(key).toBool(), false);
    QCOMPARE(
        settings.get(QStringLiteral("options/unrelatedValue")).toInt(),
        17
        );
}

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::
preservesLegacyQVariantCoercion()
{
    SettingsManager& settings = SettingsManager::instance();
    const std::array<std::pair<QVariant, bool>, 6> values = {{
        {QVariant(QStringLiteral("true")), true},
        {QVariant(QStringLiteral("false")), false},
        {QVariant(1), true},
        {QVariant(0), false},
        {QVariant(true), true},
        {QVariant(false), false}
    }};

    const SettingsManagerPowerPointDataAccessNoticePort port;
    for (const auto& [value, expected] : values)
    {
        settings.set(powerPointDataAccessNoticeKey(), value);
        QCOMPARE(port.read().showPowerPointDataAccessNotice, expected);
    }
}

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::
unavailableSettingUsesEnabledDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(powerPointDataAccessNoticeKey(), QVariant());

    const SettingsManagerPowerPointDataAccessNoticePort port;
    QVERIFY(port.read().showPowerPointDataAccessNotice);
}

void NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests::
roundTripsThroughSettingsManager()
{
    const SettingsManagerPowerPointDataAccessNoticePort port;

    for (const bool expected : {true, false})
    {
        port.write({.showPowerPointDataAccessNotice = expected});
        const PowerPointDataAccessNoticePreferences loaded = port.read();

        QCOMPARE(loaded.showPowerPointDataAccessNotice, expected);
        QCOMPARE(
            SettingsManager::instance()
                .get(powerPointDataAccessNoticeKey())
                .toBool(),
            expected
            );
    }
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerPowerPointDataAccessNoticePortTests
    )

#include "next_platform_settings_manager_powerpoint_data_access_notice_port_tests.moc"
