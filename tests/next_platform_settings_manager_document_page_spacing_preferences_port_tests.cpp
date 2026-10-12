#include "core/settingsmanager.h"
#include "next/application/document_page_spacing_preferences.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_document_page_spacing_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString documentPageSpacingKey()
{
    return QString::fromUtf8(
        OptionKeys::DocumentPageSpacing
        );
}

} // namespace

class NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalKey();
    void mapsAllStoredSpacingValues();
    void writesAllSpacingValuesForRoundTrip();
    void missingAndUnavailableValuesDefaultToSmall();
    void unknownNumericValuesDefaultToSmall();
    void malformedStoredTextBecomesNoneViaQVariantToIntParity();
    void failedWriteRestoresExactPreviousValueAndAllowsRetry();
    void failedWriteRestoresAbsentKeyAndPreservesUnrelatedSetting();
    void invalidEnumIsRejectedWithoutWriting();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
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

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
usesTheExactCanonicalKey()
{
    QCOMPARE(
        documentPageSpacingKey(),
        QStringLiteral("options/documentPageSpacing")
        );

    SettingsManager::instance().set(
        documentPageSpacingKey(),
        3
        );

    const SettingsManagerDocumentPageSpacingPreferencesPort port;
    QCOMPARE(
        port.read(),
        DocumentPageSpacing::Large
        );
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
mapsAllStoredSpacingValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentPageSpacingPreferencesPort port;

    const int storedValues[] = {0, 1, 2, 3};
    const DocumentPageSpacing expectedValues[] = {
        DocumentPageSpacing::None,
        DocumentPageSpacing::Small,
        DocumentPageSpacing::Medium,
        DocumentPageSpacing::Large
    };

    for (int index = 0; index < 4; ++index)
    {
        settings.set(
            documentPageSpacingKey(),
            storedValues[index]
            );
        QCOMPARE(
            port.read(),
            expectedValues[index]
            );
    }
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
writesAllSpacingValuesForRoundTrip()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentPageSpacingPreferencesPort port;

    const DocumentPageSpacing expectedValues[] = {
        DocumentPageSpacing::None,
        DocumentPageSpacing::Small,
        DocumentPageSpacing::Medium,
        DocumentPageSpacing::Large
    };

    for (int spacingIndex = 0; spacingIndex < 4; ++spacingIndex)
    {
        QVERIFY(port.write(expectedValues[spacingIndex]));
        QCOMPARE(
            settings.get(documentPageSpacingKey()).toInt(),
            spacingIndex
            );
        QCOMPARE(
            port.read(),
            expectedValues[spacingIndex]
            );
    }

    const auto invalidWrite = port.write(
        static_cast<DocumentPageSpacing>(99)
        );
    QVERIFY(!invalidWrite);
    QCOMPARE(invalidWrite.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        settings.get(documentPageSpacingKey()).toInt(),
        3
        );
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
missingAndUnavailableValuesDefaultToSmall()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentPageSpacingPreferencesPort port;

    settings.remove(documentPageSpacingKey());
    QCOMPARE(
        port.read(),
        DocumentPageSpacing::Small
        );

    settings.set(
        documentPageSpacingKey(),
        QVariant()
        );
    QCOMPARE(
        port.read(),
        DocumentPageSpacing::Small
        );
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
unknownNumericValuesDefaultToSmall()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentPageSpacingPreferencesPort port;

    for (const int value : {-1, 4, 99})
    {
        settings.set(
            documentPageSpacingKey(),
            value
            );
        QCOMPARE(
            port.read(),
            DocumentPageSpacing::Small
            );
    }
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
malformedStoredTextBecomesNoneViaQVariantToIntParity()
{
    SettingsManager::instance().set(
        documentPageSpacingKey(),
        QStringLiteral("not-a-number")
        );

    const SettingsManagerDocumentPageSpacingPreferencesPort port;
    QCOMPARE(
        port.read(),
        DocumentPageSpacing::None
        );
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
failedWriteRestoresExactPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString spacingKey = documentPageSpacingKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant previousValue = QStringLiteral("2");
    const QVariant unrelatedValue = QStringLiteral("leave-unchanged");
    settings.set(spacingKey, previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    SettingsManagerDocumentPageSpacingPreferencesPort faultablePort(
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

    const auto failedWrite = faultablePort.write(DocumentPageSpacing::Large);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(spacingKey), previousValue);
    QCOMPARE(
        settings.get(spacingKey).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(faultablePort.read(), DocumentPageSpacing::Medium);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(spacingKey), previousValue);
    QCOMPARE(
        persistedSettings.value(spacingKey).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(DocumentPageSpacing::Large);
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(spacingKey), QVariant(3));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(faultablePort.read(), DocumentPageSpacing::Large);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(spacingKey), QVariant(3));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
failedWriteRestoresAbsentKeyAndPreservesUnrelatedSetting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString spacingKey = documentPageSpacingKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant unrelatedValue = 17;
    settings.remove(spacingKey);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    SettingsManagerDocumentPageSpacingPreferencesPort faultablePort(
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

    const auto failedWrite = faultablePort.write(DocumentPageSpacing::None);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(spacingKey).isValid());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(faultablePort.read(), DocumentPageSpacing::Small);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(spacingKey));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(DocumentPageSpacing::None);
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(spacingKey), QVariant(0));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(spacingKey), QVariant(0));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests::
invalidEnumIsRejectedWithoutWriting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString spacingKey = documentPageSpacingKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant previousValue = QStringLiteral("2");
    const QVariant unrelatedValue = QStringLiteral("preserve-this");
    settings.set(spacingKey, previousValue);
    settings.set(unrelatedKey, unrelatedValue);

    int synchronizationAttempts = 0;
    SettingsManagerDocumentPageSpacingPreferencesPort port(
        [&synchronizationAttempts](QSettings& settings)
        {
            ++synchronizationAttempts;
            settings.sync();
            return settings.status();
        }
        );
    const auto result = port.write(
        static_cast<DocumentPageSpacing>(99)
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(settings.get(spacingKey), previousValue);
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests
    )

#include "next_platform_settings_manager_document_page_spacing_preferences_port_tests.moc"
