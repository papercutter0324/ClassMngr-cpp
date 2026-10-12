#include "core/settingsmanager.h"
#include "next/application/excel_import_timeout_preferences.h"
#include "next/domain/operation_result.h"
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
    void invalidInputReturnsErrorAndPreservesStoredValue();
    void preservesLegacySettingsKey();
    void supportedValuesRoundTripThroughSettingsManager();
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();

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
invalidInputReturnsErrorAndPreservesStoredValue()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = timeoutSettingsKey();
    const SettingsManagerExcelImportTimeoutPort port;

    settings.set(key, 90);
    QCOMPARE(port.read().seconds, kDefaultExcelImportTimeoutSeconds);
    QCOMPARE(settings.get(key).toInt(), 90);

    const auto invalidWrite = port.write({.seconds = 90});
    QVERIFY(!invalidWrite);
    QCOMPARE(
        invalidWrite.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QVERIFY(!invalidWrite.error().recoverable);
    QCOMPARE(settings.get(key).toInt(), 90);
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

    QVERIFY(port.write({.seconds = 300}));

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
        QVERIFY(port.write({.seconds = seconds}));
        QCOMPARE(port.read().seconds, seconds);
        QCOMPARE(
            SettingsManager::instance()
                .get(timeoutSettingsKey())
                .toInt(),
            seconds
            );
    }
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = timeoutSettingsKey();
    const QString unrelatedKey = QStringLiteral("imports/unrelatedValue");
    settings.set(key, 60);
    settings.set(unrelatedKey, 17);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerExcelImportTimeoutPort faultablePort(
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

    const auto failedWrite = faultablePort.write({.seconds = 300});
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
    QCOMPARE(settings.get(key).toInt(), 60);
    QCOMPARE(faultablePort.read().seconds, 60);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 17);

    injectAccessError = false;
    const auto retryResult = faultablePort.write({.seconds = 300});
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key).toInt(), 300);
    QCOMPARE(faultablePort.read().seconds, 300);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 17);
}

void NextPlatformSettingsManagerExcelImportTimeoutPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = timeoutSettingsKey();
    const QString unrelatedKey = QStringLiteral("imports/unrelatedValue");
    settings.remove(key);
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerExcelImportTimeoutPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite = faultablePort.write({.seconds = 30});
    QVERIFY(!failedWrite);
    QCOMPARE(
        failedWrite.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(!settings.get(key).isValid());
    QCOMPARE(
        faultablePort.read().seconds,
        kDefaultExcelImportTimeoutSeconds
        );
    QCOMPARE(settings.get(unrelatedKey).toInt(), 29);
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerExcelImportTimeoutPortTests)

#include "next_platform_settings_manager_excel_import_timeout_port_tests.moc"
