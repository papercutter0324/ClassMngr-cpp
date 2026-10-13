#include "core/settingsmanager.h"
#include "next/application/last_selected_campus_port.h"
#include "next/platform/settings_manager_last_selected_campus_port.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <optional>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString campusKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::LAST_CAMPUS_JSON_ID
        );
}

QString integerCampusKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::LAST_CAMPUS_ID
        );
}

Domain::CampusId campusId(
    const std::string& value
    )
{
    return *Domain::CampusId::fromString(value);
}

} // namespace

class NextPlatformSettingsManagerLastSelectedCampusPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void readsAndWritesTheExactLegacyKey();
    void missingAndUnavailableValuesReadAsNoSelection();
    void blankAndInvalidValuesReadAsNoSelectionWithoutRewriting();
    void validReadPreservesExactStoredIdText();
    void optionalRoundTripSupportsSetAndClear();
    void blankPresentIdIsRejectedBeforeSettingsAccess();
    void oversizedPresentIdIsRejectedBeforeSettingsAccess();
    void maximumLengthPresentIdWritesExactText();
    void failedWriteRestoresExactPreviousValueAndAllowsRetry();
    void failedWriteRestoresAbsentKeyAndPreservesOtherCampusSettings();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerLastSelectedCampusPortTests::initTestCase()
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

void NextPlatformSettingsManagerLastSelectedCampusPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
readsAndWritesTheExactLegacyKey()
{
    QCOMPARE(
        campusKey(),
        QStringLiteral("campus/lastSelectedJsonId")
        );

    SettingsManager& settings = SettingsManager::instance();
    settings.set(campusKey(), QStringLiteral("legacy-campus"));

    const SettingsManagerLastSelectedCampusPort port;
    const auto loaded = port.read();

    QVERIFY(loaded.has_value());
    QCOMPARE(
        QString::fromStdString(loaded->value()),
        QStringLiteral("legacy-campus")
        );

    const auto writeResult = port.write(campusId("written-campus"));
    QVERIFY(writeResult);
    QCOMPARE(
        settings.get(campusKey()).toString(),
        QStringLiteral("written-campus")
        );
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
missingAndUnavailableValuesReadAsNoSelection()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(campusKey());

    const SettingsManagerLastSelectedCampusPort port;
    QVERIFY(!port.read().has_value());
    QVERIFY(!settings.get(campusKey()).isValid());

    settings.set(campusKey(), QVariant());
    QVERIFY(!port.read().has_value());
    QVERIFY(!settings.get(campusKey()).isValid());
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
blankAndInvalidValuesReadAsNoSelectionWithoutRewriting()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerLastSelectedCampusPort port;

    const QString blankValue = QStringLiteral(" \t ");
    settings.set(campusKey(), blankValue);
    QVERIFY(!port.read().has_value());
    QCOMPARE(settings.get(campusKey()).toString(), blankValue);

    settings.set(campusKey(), QVariant());
    QVERIFY(!port.read().has_value());
    QVERIFY(!settings.get(campusKey()).isValid());
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
validReadPreservesExactStoredIdText()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString storedText = QStringLiteral("J-01");
    settings.set(campusKey(), storedText);

    const SettingsManagerLastSelectedCampusPort port;
    const auto loaded = port.read();

    QVERIFY(loaded.has_value());
    QCOMPARE(
        QString::fromStdString(loaded->value()),
        storedText
        );
    QCOMPARE(settings.get(campusKey()).toString(), storedText);
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
optionalRoundTripSupportsSetAndClear()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(integerCampusKey(), 27);

    SettingsManagerLastSelectedCampusPort port;
    const std::optional<Domain::CampusId> expected =
        campusId("j");

    const auto setResult = port.write(expected);
    QVERIFY(setResult);
    const auto loaded = port.read();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->value(), expected->value());
    QCOMPARE(settings.get(campusKey()).toString(), QStringLiteral("j"));

    const auto clearResult = port.write(std::nullopt);
    QVERIFY(clearResult);
    QVERIFY(!port.read().has_value());
    QCOMPARE(settings.get(campusKey()).toString(), QString());
    QCOMPARE(settings.get(integerCampusKey()).toInt(), 27);
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
blankPresentIdIsRejectedBeforeSettingsAccess()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = campusKey();
    const QVariant previousValue = QStringLiteral("preserved-campus");
    settings.set(key, previousValue);
    settings.sync();

    int synchronizationAttempts = 0;
    const SettingsManagerLastSelectedCampusPort port(
        [&synchronizationAttempts](QSettings& settings)
        {
            ++synchronizationAttempts;
            settings.sync();
            return settings.status();
        }
        );

    const auto blankId = Domain::CampusId::fromString(" \t\r\n");
    QVERIFY(blankId.has_value());
    const auto blankResult = port.write(blankId);
    QVERIFY(!blankResult);
    QCOMPARE(blankResult.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!blankResult.error().recoverable);
    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(settings.get(key), previousValue);
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
oversizedPresentIdIsRejectedBeforeSettingsAccess()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = campusKey();
    const QVariant previousValue = QStringLiteral("preserved-campus");
    settings.set(key, previousValue);
    settings.sync();

    int synchronizationAttempts = 0;
    const SettingsManagerLastSelectedCampusPort port(
        [&synchronizationAttempts](QSettings& settings)
        {
            ++synchronizationAttempts;
            settings.sync();
            return settings.status();
        }
        );

    const auto oversizedId = Domain::CampusId::fromString(
        std::string(257, 'x')
        );
    QVERIFY(oversizedId.has_value());
    const auto oversizedResult = port.write(oversizedId);
    QVERIFY(!oversizedResult);
    QCOMPARE(oversizedResult.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!oversizedResult.error().recoverable);
    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(settings.get(key), previousValue);
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
maximumLengthPresentIdWritesExactText()
{
    const std::string expectedValue(256, 'x');
    const auto campusId = Domain::CampusId::fromString(expectedValue);
    QVERIFY(campusId.has_value());

    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerLastSelectedCampusPort port;
    const auto writeResult = port.write(campusId);

    QVERIFY(writeResult);
    QCOMPARE(
        settings.get(campusKey()),
        QVariant(QString::fromStdString(expectedValue))
        );
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
failedWriteRestoresExactPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = campusKey();
    const QString unrelatedKey = QStringLiteral("campus/unrelatedSetting");
    const QVariant previousValue = QStringLiteral("  preserved-campus  ");
    const QVariant unrelatedValue = QStringLiteral("leave-unchanged");
    const QVariant integerCampusValue = 27;
    settings.set(key, previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.set(integerCampusKey(), integerCampusValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerLastSelectedCampusPort faultablePort(
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

    const auto failedWrite = faultablePort.write(
        campusId("replacement-campus")
        );
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(!failedWrite.error().message.empty());
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key), previousValue);
    QCOMPARE(settings.get(key).metaType().id(), previousValue.metaType().id());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(integerCampusKey()), integerCampusValue);
    QCOMPARE(
        settings.get(integerCampusKey()).metaType().id(),
        integerCampusValue.metaType().id()
        );
    const auto restoredRead = faultablePort.read();
    QVERIFY(restoredRead.has_value());
    QCOMPARE(restoredRead->value(), std::string("  preserved-campus  "));

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
    QCOMPARE(persistedSettings.value(integerCampusKey()), integerCampusValue);
    QCOMPARE(
        persistedSettings.value(integerCampusKey()).metaType().id(),
        integerCampusValue.metaType().id()
        );

    injectAccessError = false;
    const auto retry = faultablePort.write(campusId("replacement-campus"));
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key), QVariant(QStringLiteral("replacement-campus")));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(integerCampusKey()), integerCampusValue);
    QCOMPARE(
        settings.get(integerCampusKey()).metaType().id(),
        integerCampusValue.metaType().id()
        );
    const auto retriedRead = faultablePort.read();
    QVERIFY(retriedRead.has_value());
    QCOMPARE(retriedRead->value(), std::string("replacement-campus"));
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(
        persistedSettings.value(key),
        QVariant(QStringLiteral("replacement-campus"))
        );
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(integerCampusKey()), integerCampusValue);
    QCOMPARE(
        persistedSettings.value(integerCampusKey()).metaType().id(),
        integerCampusValue.metaType().id()
        );
}

void NextPlatformSettingsManagerLastSelectedCampusPortTests::
failedWriteRestoresAbsentKeyAndPreservesOtherCampusSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = campusKey();
    const QString unrelatedKey = QStringLiteral("campus/unrelatedSetting");
    const QVariant unrelatedValue = 83;
    const QVariant integerCampusValue = 27;
    settings.remove(key);
    settings.set(unrelatedKey, unrelatedValue);
    settings.set(integerCampusKey(), integerCampusValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerLastSelectedCampusPort faultablePort(
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

    const auto failedWrite = faultablePort.write(campusId("new-campus"));
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(!faultablePort.read().has_value());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(integerCampusKey()), integerCampusValue);
    QCOMPARE(
        settings.get(integerCampusKey()).metaType().id(),
        integerCampusValue.metaType().id()
        );

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(integerCampusKey()), integerCampusValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(campusId("new-campus"));
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key), QVariant(QStringLiteral("new-campus")));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(integerCampusKey()), integerCampusValue);
    const auto retriedRead = faultablePort.read();
    QVERIFY(retriedRead.has_value());
    QCOMPARE(retriedRead->value(), std::string("new-campus"));
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), QVariant(QStringLiteral("new-campus")));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(integerCampusKey()), integerCampusValue);
    QCOMPARE(
        persistedSettings.value(integerCampusKey()).metaType().id(),
        integerCampusValue.metaType().id()
        );
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerLastSelectedCampusPortTests)

#include "next_platform_settings_manager_last_selected_campus_port_tests.moc"
