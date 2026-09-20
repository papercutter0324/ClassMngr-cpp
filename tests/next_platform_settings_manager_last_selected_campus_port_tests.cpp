#include "core/settingsmanager.h"
#include "next/application/last_selected_campus_port.h"
#include "next/platform/settings_manager_last_selected_campus_port.h"

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

    port.write(campusId("written-campus"));
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

    port.write(expected);
    const auto loaded = port.read();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->value(), expected->value());
    QCOMPARE(settings.get(campusKey()).toString(), QStringLiteral("j"));

    port.write(std::nullopt);
    QVERIFY(!port.read().has_value());
    QCOMPARE(settings.get(campusKey()).toString(), QString());
    QCOMPARE(settings.get(integerCampusKey()).toInt(), 27);
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerLastSelectedCampusPortTests)

#include "next_platform_settings_manager_last_selected_campus_port_tests.moc"
