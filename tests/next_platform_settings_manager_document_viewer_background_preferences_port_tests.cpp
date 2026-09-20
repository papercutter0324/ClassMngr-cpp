#include "core/settingsmanager.h"
#include "next/application/document_viewer_background_preferences.h"
#include "next/platform/settings_manager_document_viewer_background_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

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
    void missingInvalidAndUnknownValuesDefaultToDefault();
    void unavailableSettingsDefaultToDefault();

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

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPortTests
    )

#include "next_platform_settings_manager_document_viewer_background_preferences_port_tests.moc"
