#include "core/settingsmanager.h"
#include "next/application/skipped_update_version_preferences.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QVariant>

#include <optional>
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
    void failedClearRestoresExactPreviousValueAndAllowsRetry();
    void failedClearRestoresAbsentKeyAndPreservesUnrelatedSetting();
    void failedWriteRestoresExactPreviousValueAndAllowsRetry();
    void failedWriteRestoresAbsentKeyAndPreservesUnrelatedSetting();

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

    const auto writeResult = port.write({
        .skippedVersion = std::string(" 2.3.4 ")
    });
    QVERIFY(writeResult);
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

    const auto writeResult = port.write({.skippedVersion = expected});
    QVERIFY(writeResult);

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

    const auto writeResult = port.write({.skippedVersion = std::string("1.2.3")});
    QVERIFY(writeResult);
    QVERIFY(settings.get(key).isValid());

    const auto clearResult = port.clear();
    QVERIFY(clearResult);

    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!port.read().skippedVersion.has_value());
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
failedClearRestoresExactPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    const QVariant previousValue = QStringLiteral(" 1.2.3 ");
    const QVariant unrelatedValue = QStringLiteral("leave-unchanged");
    settings.set(key, previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerSkippedUpdateVersionPort faultablePort(
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

    const auto failedClear = faultablePort.clear();
    QVERIFY(!failedClear);
    QCOMPARE(failedClear.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedClear.error().recoverable);
    QVERIFY(
        failedClear.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key), previousValue);
    QCOMPARE(settings.get(key).metaType().id(), previousValue.metaType().id());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(
        faultablePort.read().skippedVersion,
        std::optional<std::string>("1.2.3")
        );

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), previousValue);
    QCOMPARE(
        persistedSettings.value(key).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.clear();
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!faultablePort.read().skippedVersion.has_value());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
failedClearRestoresAbsentKeyAndPreservesUnrelatedSetting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    const QVariant unrelatedValue = 29;
    settings.remove(key);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerSkippedUpdateVersionPort faultablePort(
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

    const auto failedClear = faultablePort.clear();
    QVERIFY(!failedClear);
    QCOMPARE(failedClear.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedClear.error().recoverable);
    QVERIFY(
        failedClear.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!faultablePort.read().skippedVersion.has_value());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.clear();
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!faultablePort.read().skippedVersion.has_value());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
failedWriteRestoresExactPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    const QVariant previousValue = QStringLiteral(" 1.2.3 ");
    const QVariant unrelatedValue = QStringLiteral("leave-unchanged");
    settings.set(key, previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerSkippedUpdateVersionPort faultablePort(
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

    const auto failedWrite = faultablePort.write({
        .skippedVersion = std::string("2.3.4")
    });
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key), previousValue);
    QCOMPARE(settings.get(key).metaType().id(), previousValue.metaType().id());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(
        faultablePort.read().skippedVersion,
        std::optional<std::string>("1.2.3")
        );

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), previousValue);
    QCOMPARE(
        persistedSettings.value(key).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.write({
        .skippedVersion = std::string("2.3.4")
    });
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key), QVariant(QStringLiteral("2.3.4")));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(
        faultablePort.read().skippedVersion,
        std::optional<std::string>("2.3.4")
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), QVariant(QStringLiteral("2.3.4")));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerSkippedUpdateVersionPortTests::
failedWriteRestoresAbsentKeyAndPreservesUnrelatedSetting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = skippedUpdateVersionKey();
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    const QVariant unrelatedValue = 29;
    settings.remove(key);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerSkippedUpdateVersionPort faultablePort(
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

    const auto failedWrite = faultablePort.write({
        .skippedVersion = std::string("3.4.5")
    });
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!faultablePort.read().skippedVersion.has_value());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.write({
        .skippedVersion = std::string("3.4.5")
    });
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key), QVariant(QStringLiteral("3.4.5")));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(
        faultablePort.read().skippedVersion,
        std::optional<std::string>("3.4.5")
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), QVariant(QStringLiteral("3.4.5")));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerSkippedUpdateVersionPortTests
    )

#include "next_platform_settings_manager_skipped_update_version_port_tests.moc"
