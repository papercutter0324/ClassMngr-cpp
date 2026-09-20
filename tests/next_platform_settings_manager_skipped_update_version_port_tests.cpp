#include "core/settingsmanager.h"
#include "next/application/skipped_update_version_preferences.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString skippedUpdateVersionKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::SKIPPED_UPDATE_VERSION
        );
}

} // namespace

class NextPlatformSettingsManagerSkippedUpdateVersionPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void missingAndEmptySettingsReadAsEmpty();
    void trimsOnReadAndWrite();
    void roundTripsThroughSettingsManager();
    void clearRemovesSetting();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::initTestCase()
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

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
missingAndEmptySettingsReadAsEmpty()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const SettingsManagerSkippedUpdateVersionPort port;

    settings.remove(key);
    QVERIFY(!port.read().skippedVersion.has_value());
    QVERIFY(!settings.get(key).isValid());

    settings.set(key, QStringLiteral("   "));
    QVERIFY(!port.read().skippedVersion.has_value());
    QVERIFY(settings.get(key).isValid());
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
trimsOnReadAndWrite()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const SettingsManagerSkippedUpdateVersionPort port;

    settings.set(key, QStringLiteral(" 1.2.3 "));
    const auto loaded = port.read();
    QVERIFY(loaded.skippedVersion.has_value());
    QCOMPARE(*loaded.skippedVersion, std::string("1.2.3"));

    port.write({
        .skippedVersion = std::string(" 2.3.4 ")
    });
    QCOMPARE(settings.get(key).toString(), QStringLiteral("2.3.4"));
    QCOMPARE(
        *port.read().skippedVersion,
        std::string("2.3.4")
        );
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
roundTripsThroughSettingsManager()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const SettingsManagerSkippedUpdateVersionPort port;
    const std::string expected = "9.10.11";

    port.write({.skippedVersion = expected});

    const auto loaded = port.read();
    QVERIFY(loaded.skippedVersion.has_value());
    QCOMPARE(*loaded.skippedVersion, expected);
    QCOMPARE(settings.get(key).toString(), QStringLiteral("9.10.11"));
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
clearRemovesSetting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const SettingsManagerSkippedUpdateVersionPort port;

    port.write({.skippedVersion = std::string("1.2.3")});
    QVERIFY(settings.get(key).isValid());

    port.clear();

    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!port.read().skippedVersion.has_value());
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerSkippedUpdateVersionPortTests
    )

#include "next_platform_settings_manager_skipped_update_version_port_tests.moc"
