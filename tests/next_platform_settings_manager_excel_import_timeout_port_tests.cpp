#include "core/settingsmanager.h"
#include "next/application/excel_import_timeout_preferences.h"
#include "next/platform/settings_manager_excel_import_timeout_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString timeoutSettingsKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::EXCEL_IMPORT_TIMEOUT_SECONDS
        );
}

} // namespace

class NextPlatformSettingsManagerExcelImportTimeoutPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void missingSettingUsesDefault();
    void supportedValuesAreAcceptedByContract();
    void invalidInputNormalizesToDefault();
    void preservesLegacySettingsKey();
    void supportedValuesRoundTripThroughSettingsManager();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::initTestCase()
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

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
missingSettingUsesDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(timeoutSettingsKey());

    const SettingsManagerExcelImportTimeoutPort port;
    const ExcelImportTimeoutPreferences preferences = port.read();

    QCOMPARE(preferences.seconds, kDefaultExcelImportTimeoutSeconds);
    QVERIFY(!settings.get(timeoutSettingsKey()).isValid());
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
supportedValuesAreAcceptedByContract()
{
    for (const int seconds : {30, 60, 120, 300})
    {
        QVERIFY(isSupportedExcelImportTimeoutSeconds(seconds));
        QCOMPARE(
            normalizeExcelImportTimeoutSeconds(seconds),
            seconds
            );

        const ExcelImportTimeoutPreferences preferences{.seconds = seconds};
        QCOMPARE(preferences.normalizedSeconds(), seconds);
    }

    QCOMPARE(
        normalizeExcelImportTimeoutSeconds(90),
        kDefaultExcelImportTimeoutSeconds
        );
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
invalidInputNormalizesToDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = timeoutSettingsKey();
    const SettingsManagerExcelImportTimeoutPort port;

    settings.set(key, 90);
    QCOMPARE(port.read().seconds, kDefaultExcelImportTimeoutSeconds);
    QCOMPARE(settings.get(key).toInt(), 90);

    port.write({.seconds = 90});
    QCOMPARE(settings.get(key).toInt(), kDefaultExcelImportTimeoutSeconds);
    QCOMPARE(port.read().seconds, kDefaultExcelImportTimeoutSeconds);
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
preservesLegacySettingsKey()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = timeoutSettingsKey();
    const SettingsManagerExcelImportTimeoutPort port;

    QCOMPARE(key, QStringLiteral("imports/excelTimeoutSeconds"));
    settings.set(QStringLiteral("imports/unrelatedValue"), 17);

    port.write({.seconds = 300});

    QCOMPARE(settings.get(key).toInt(), 300);
    QCOMPARE(
        settings.get(QStringLiteral("imports/unrelatedValue")).toInt(),
        17
        );
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
supportedValuesRoundTripThroughSettingsManager()
{
    const SettingsManagerExcelImportTimeoutPort port;
    const std::array<int, 4> supportedValues = {30, 60, 120, 300};

    for (const int seconds : supportedValues)
    {
        port.write({.seconds = seconds});
        QCOMPARE(port.read().seconds, seconds);
        QCOMPARE(
            SettingsManager::instance()
                .get(timeoutSettingsKey())
                .toInt(),
            seconds
            );
    }
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerExcelImportTimeoutPortTests)

#include "next_platform_settings_manager_excel_import_timeout_port_tests.moc"
