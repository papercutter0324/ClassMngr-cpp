#include "core/settingsmanager.h"
#include "next/application/document_page_spacing_preferences.h"
#include "next/platform/settings_manager_document_page_spacing_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

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
        port.write(expectedValues[spacingIndex]);
        QCOMPARE(
            settings.get(documentPageSpacingKey()).toInt(),
            spacingIndex
            );
        QCOMPARE(
            port.read(),
            expectedValues[spacingIndex]
            );
    }

    port.write(
        static_cast<DocumentPageSpacing>(99)
        );
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

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests
    )

#include "next_platform_settings_manager_document_page_spacing_preferences_port_tests.moc"
