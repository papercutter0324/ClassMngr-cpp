#include "core/settingsmanager.h"
#include "next/application/document_viewer_background_preferences.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_document_viewer_background_preferences_port.h"
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

QString documentViewerBackgroundKey()
{
    return QString::fromUtf8(
        OptionKeys::DocumentViewerBackground
        );
}

} // namespace

class NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalKey();
    void mapsAllStoredBackgroundValues();
    void writesAllBackgroundValuesForRoundTrip();
    void missingInvalidAndUnknownValuesDefaultToDefault();
    void unavailableSettingsDefaultToDefault();
    void failedWriteRestoresExactPreviousValueAndAllowsRetry();
    void failedWriteRestoresAbsentKeyAndPreservesUnrelatedSetting();
    void invalidEnumIsRejectedWithoutWriting();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
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

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
usesTheExactCanonicalKey()
{
    QCOMPARE(
        documentViewerBackgroundKey(),
        QStringLiteral("options/documentViewerBackground")
        );

    SettingsManager::instance().set(
        documentViewerBackgroundKey(),
        2
        );

    const SettingsManagerDocumentViewerBackgroundPreferencesPort port;
    QCOMPARE(
        port.read(),
        DocumentViewerBackground::Black
        );
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
mapsAllStoredBackgroundValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentViewerBackgroundPreferencesPort port;

    const int storedValues[] = {0, 1, 2};
    const DocumentViewerBackground expectedValues[] = {
        DocumentViewerBackground::Default,
        DocumentViewerBackground::White,
        DocumentViewerBackground::Black
    };

    for (int index = 0; index < 3; ++index)
    {
        settings.set(
            documentViewerBackgroundKey(),
            storedValues[index]
            );
        QCOMPARE(
            port.read(),
            expectedValues[index]
            );
    }
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
writesAllBackgroundValuesForRoundTrip()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentViewerBackgroundPreferencesPort port;

    const DocumentViewerBackground expectedValues[] = {
        DocumentViewerBackground::Default,
        DocumentViewerBackground::White,
        DocumentViewerBackground::Black
    };

    for (int backgroundIndex = 0; backgroundIndex < 3; ++backgroundIndex)
    {
        QVERIFY(port.write(expectedValues[backgroundIndex]));
        QCOMPARE(
            settings.get(documentViewerBackgroundKey()).toInt(),
            backgroundIndex
            );
        QCOMPARE(
            port.read(),
            expectedValues[backgroundIndex]
            );
    }

    const auto invalidWrite = port.write(
        static_cast<DocumentViewerBackground>(99)
        );
    QVERIFY(!invalidWrite);
    QCOMPARE(invalidWrite.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        settings.get(documentViewerBackgroundKey()).toInt(),
        2
        );
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
missingInvalidAndUnknownValuesDefaultToDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDocumentViewerBackgroundPreferencesPort port;

    settings.remove(documentViewerBackgroundKey());
    QCOMPARE(
        port.read(),
        DocumentViewerBackground::Default
        );

    settings.set(documentViewerBackgroundKey(), -1);
    QCOMPARE(
        port.read(),
        DocumentViewerBackground::Default
        );

    settings.set(documentViewerBackgroundKey(), 3);
    QCOMPARE(
        port.read(),
        DocumentViewerBackground::Default
        );

    settings.set(
        documentViewerBackgroundKey(),
        QStringLiteral("unknown")
        );
    QCOMPARE(
        port.read(),
        DocumentViewerBackground::Default
        );
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
unavailableSettingsDefaultToDefault()
{
    SettingsManager::instance().set(
        documentViewerBackgroundKey(),
        QVariant()
        );

    const SettingsManagerDocumentViewerBackgroundPreferencesPort port;
    QCOMPARE(
        port.read(),
        DocumentViewerBackground::Default
        );
    QVERIFY(
        !SettingsManager::instance().get(
            documentViewerBackgroundKey()
            ).isValid()
        );
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
failedWriteRestoresExactPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString backgroundKey = documentViewerBackgroundKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant previousValue = QStringLiteral("1");
    const QVariant unrelatedValue = QStringLiteral("leave-unchanged");
    settings.set(backgroundKey, previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    SettingsManagerDocumentViewerBackgroundPreferencesPort faultablePort(
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
        DocumentViewerBackground::Black
        );
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(backgroundKey), previousValue);
    QCOMPARE(
        settings.get(backgroundKey).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(faultablePort.read(), DocumentViewerBackground::White);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(backgroundKey), previousValue);
    QCOMPARE(
        persistedSettings.value(backgroundKey).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(DocumentViewerBackground::Black);
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(backgroundKey).toInt(), 2);
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(faultablePort.read(), DocumentViewerBackground::Black);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(backgroundKey).toInt(), 2);
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
failedWriteRestoresAbsentKeyAndPreservesUnrelatedSetting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString backgroundKey = documentViewerBackgroundKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant unrelatedValue = 17;
    settings.remove(backgroundKey);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    int synchronizationAttempts = 0;
    bool injectAccessError = true;
    SettingsManagerDocumentViewerBackgroundPreferencesPort faultablePort(
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
        DocumentViewerBackground::White
        );
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(backgroundKey).isValid());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(faultablePort.read(), DocumentViewerBackground::Default);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(backgroundKey));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(DocumentViewerBackground::White);
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(backgroundKey).toInt(), 1);
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(backgroundKey).toInt(), 1);
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests::
invalidEnumIsRejectedWithoutWriting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString backgroundKey = documentViewerBackgroundKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant previousValue = QStringLiteral("1");
    const QVariant unrelatedValue = QStringLiteral("preserve-this");
    settings.set(backgroundKey, previousValue);
    settings.set(unrelatedKey, unrelatedValue);

    int synchronizationAttempts = 0;
    SettingsManagerDocumentViewerBackgroundPreferencesPort port(
        [&synchronizationAttempts](QSettings& settings)
        {
            ++synchronizationAttempts;
            settings.sync();
            return settings.status();
        }
        );
    const auto result = port.write(
        static_cast<DocumentViewerBackground>(99)
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(settings.get(backgroundKey), previousValue);
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests
    )

#include "next_platform_settings_manager_document_viewer_background_preferences_port_tests.moc"
