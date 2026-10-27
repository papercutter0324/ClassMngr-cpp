#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/custom_color_palette_preferences.h"
#include "next/platform/custom_color_palette_preference_codec.h"
#include "next/platform/workspace_store_custom_color_palette_preferences_port.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Platform;

class WorkspaceStoreCustomColorPalettePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void missingPaletteUsesSixteenDefaultsAndRoundTrips();
    void legacyPayloadsAreNormalizedBySharedCodec();
    void repositoryReadAndWriteFailuresRemainVisible();
    void closedStoreKeepsLegacyNoopSemantics();

private:
    [[nodiscard]] QString databasePath() const
    {
        return m_directory.filePath(
            QStringLiteral("setup-palette-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
    }

    QTemporaryDir m_directory;
};

void WorkspaceStoreCustomColorPalettePreferencesPortTests::
missingPaletteUsesSixteenDefaultsAndRoundTrips()
{
    QVERIFY(m_directory.isValid());
    WorkspaceStore store;
    QVERIFY(store.open(databasePath()));
    WorkspaceStoreCustomColorPalettePreferencesPort port(store);

    const auto defaults = port.read();
    QVERIFY(defaults);
    QCOMPARE(
        defaults.value(),
        Application::defaultCustomColorPalette()
        );
    QCOMPARE(
        static_cast<int>(defaults.value().hexColors.size()),
        16
        );

    Application::CustomColorPalette source =
        Application::defaultCustomColorPalette();
    source.hexColors[0] = Application::CustomColorPaletteEntry(" #ABCDEF ");
    source.hexColors[1] = Application::CustomColorPaletteEntry("not-a-color");
    QVERIFY(port.write(source));

    SettingsRepository* const settings = store.settingsRepository();
    QVERIFY(settings);
    const auto stored = settings->loadSetting(
        CustomColorPalettePreferenceCodec::key()
        );
    QVERIFY(stored);
    const QJsonDocument encoded = QJsonDocument::fromJson(
        stored->toString().toUtf8()
        );
    QVERIFY(encoded.isArray());
    QCOMPARE(encoded.array().size(), 16);
    QCOMPARE(encoded.array().at(0).toString(), QStringLiteral(" #ABCDEF "));
    QCOMPARE(encoded.array().at(1).toString(), QStringLiteral("not-a-color"));

    const auto normalized = port.read();
    QVERIFY(normalized);
    QCOMPARE(
        normalized.value().hexColors[0].value(),
        std::string("#abcdef")
        );
    QCOMPARE(
        normalized.value().hexColors[1].value(),
        Application::defaultCustomColorPalette().hexColors[1].value()
        );
    QCOMPARE(normalized.value().hexColors.size(), std::size_t(16));
}

void WorkspaceStoreCustomColorPalettePreferencesPortTests::
legacyPayloadsAreNormalizedBySharedCodec()
{
    const auto listValue = CustomColorPalettePreferenceCodec::
        normalizeStoredValue(QStringList{QStringLiteral("#123456")});
    QCOMPARE(listValue.hexColors[0].value(), std::string("#123456"));
    QCOMPARE(listValue.hexColors[1].value(),
             Application::defaultCustomColorPalette().hexColors[1].value());

    const auto separatorValue = CustomColorPalettePreferenceCodec::
        normalizeStoredValue(QStringLiteral("#123456;#abcdef"));
    QCOMPARE(separatorValue.hexColors[0].value(), std::string("#123456"));
    QCOMPARE(separatorValue.hexColors[1].value(), std::string("#abcdef"));
    QCOMPARE(separatorValue.hexColors.size(), std::size_t(16));
}

void WorkspaceStoreCustomColorPalettePreferencesPortTests::
repositoryReadAndWriteFailuresRemainVisible()
{
    QVERIFY(m_directory.isValid());
    WorkspaceStore store;
    QVERIFY(store.open(databasePath()));
    WorkspaceStoreCustomColorPalettePreferencesPort port(store);
    SettingsRepository* const settings = store.settingsRepository();
    QVERIFY(settings);

    QSqlQuery dropTable(store.database());
    QVERIFY(dropTable.exec(QStringLiteral("DROP TABLE app_settings")));

    const auto failedRead = port.read();
    QVERIFY(!failedRead);
    QCOMPARE(failedRead.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(failedRead.error().message).contains(
        QStringLiteral("custom colors")
        ));

    const auto failedWrite = port.write(
        Application::defaultCustomColorPalette()
        );
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
}

void WorkspaceStoreCustomColorPalettePreferencesPortTests::
closedStoreKeepsLegacyNoopSemantics()
{
    WorkspaceStore store;
    WorkspaceStoreCustomColorPalettePreferencesPort port(store);
    const auto read = port.read();
    QVERIFY(read);
    QCOMPARE(read.value(), Application::defaultCustomColorPalette());
    QVERIFY(port.write(Application::defaultCustomColorPalette()));
}

QTEST_GUILESS_MAIN(WorkspaceStoreCustomColorPalettePreferencesPortTests)

#include "next_platform_workspace_store_custom_color_palette_preferences_port_tests.moc"
