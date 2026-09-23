#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/custom_color_palette_preferences.h"
#include "next/platform/application_services_custom_color_palette_preferences_port.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto CustomColorsKey = "custom_colors";
constexpr auto UnrelatedKey = "custom_colors_unrelated";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("custom-color-palette-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
}

QString compactJson(const CustomColorPalette& palette)
{
    QJsonArray array;
    for (const std::string& color : palette.hexColors)
    {
        array.append(QString::fromStdString(color));
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact)
        );
}

CustomColorPalette paletteWithTwoColors()
{
    auto palette = defaultCustomColorPalette();
    palette.hexColors[0] = "#123456";
    palette.hexColors[1] = "#abcdef";
    return palette;
}

CustomColorPalette canonicalizedPalette(
    const CustomColorPalette& source
    )
{
    auto result = defaultCustomColorPalette();
    for (std::size_t index = 0;
         index < CustomColorPalette::EntryCount;
         ++index)
    {
        const QColor color(
            QString::fromStdString(source.hexColors[index])
            );
        if (color.isValid())
        {
            result.hexColors[index] = color.name(
                QColor::HexRgb
                ).toStdString();
        }
    }
    return result;
}

} // namespace

class NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingAndUnavailableReadDefaults();
    void compactJsonRoundTripUsesExactKey();
    void legacyQStringListJsonAndSeparatorPayloadsLoad();
    void invalidEntriesUseFallbackAndNormalizeToSixteenCanonicalColors();
    void saveFailurePreservesStoredPaletteAndWarning();
    void preservesUnrelatedSettings();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
missingAndUnavailableReadDefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    QVERIFY(port.read() == defaultCustomColorPalette());

    ApplicationServices unavailableServices;
    ApplicationServicesCustomColorPalettePreferencesPort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read() == defaultCustomColorPalette());

    unavailablePort.write(paletteWithTwoColors());
    QVERIFY(unavailablePort.read() == defaultCustomColorPalette());

    ApplicationServicesCustomColorPalettePreferencesPort nullSettingsPort(
        static_cast<SettingsService*>(nullptr)
        );
    QVERIFY(nullSettingsPort.read() == defaultCustomColorPalette());
    nullSettingsPort.write(paletteWithTwoColors());
    QVERIFY(nullSettingsPort.read() == defaultCustomColorPalette());
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
compactJsonRoundTripUsesExactKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const CustomColorPalette expected = paletteWithTwoColors();
    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    port.write(expected);

    const auto actual = port.read();
    const auto expectedRead = canonicalizedPalette(expected);
    QVERIFY(actual == expectedRead);

    const auto stored = services.dataService()->loadSetting(
        QString::fromUtf8(CustomColorsKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), compactJson(expected));

    const auto staleKey = services.dataService()->loadSetting(
        QStringLiteral("customColors")
        );
    QVERIFY(staleKey);
    QVERIFY(!staleKey->isValid());
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
legacyQStringListJsonAndSeparatorPayloadsLoad()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    const auto expected = paletteWithTwoColors();

    const QStringList legacyList = {
        QStringLiteral("#123456"),
        QStringLiteral("#abcdef")
    };
    const auto listActual =
        ApplicationServicesCustomColorPalettePreferencesPort::
            normalizeStoredValue(QVariant::fromValue(legacyList));
    QVERIFY(listActual == expected);

    const std::array<QString, 4> payloads = {{
        QStringLiteral("[\"#123456\", \"#abcdef\"]"),
        QStringLiteral("#123456\n#abcdef"),
        QStringLiteral("#123456;#abcdef"),
        QStringLiteral("#123456,#abcdef")
    }};
    for (const QString& payload : payloads)
    {
        QVERIFY(
            services.dataService()->saveSetting(
                QString::fromUtf8(CustomColorsKey),
                payload
                )
            );
        QVERIFY(port.read() == expected);
    }
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
invalidEntriesUseFallbackAndNormalizeToSixteenCanonicalColors()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    QJsonArray source;
    source.append(QStringLiteral("not-a-color"));
    source.append(QStringLiteral(" #abcdef "));
    for (int index = 2; index < 15; ++index)
    {
        source.append(QStringLiteral("#010203"));
    }
    source.append(QStringLiteral("#123456"));
    source.append(QStringLiteral("#654321"));

    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(CustomColorsKey),
            QString::fromUtf8(
                QJsonDocument(source).toJson(QJsonDocument::Compact)
                )
            )
        );

    auto expected = defaultCustomColorPalette();
    expected.hexColors[1] = "#abcdef";
    for (int index = 2; index < 15; ++index)
    {
        expected.hexColors[static_cast<std::size_t>(index)] = "#010203";
    }
    expected.hexColors[15] = "#123456";

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    const auto actual = port.read();
    QVERIFY(actual == expected);
    QCOMPARE(
        actual.hexColors.size(),
        CustomColorPalette::EntryCount
        );
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
saveFailurePreservesStoredPaletteAndWarning()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    const auto initial = paletteWithTwoColors();
    port.write(initial);
    QVERIFY(
        executeSql(
            services,
            QStringLiteral(R"(
                CREATE TRIGGER fail_custom_colors
                BEFORE INSERT ON app_settings
                WHEN NEW.key = 'custom_colors'
                BEGIN
                    SELECT RAISE(ABORT, 'forced custom colors failure');
                END
            )")
            )
        );

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral("Failed to save custom colors:.*"))
        );
    port.write(paletteWithTwoColors());

    const auto actual = port.read();
    const auto expectedRead = canonicalizedPalette(initial);
    QVERIFY(actual == expectedRead);
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
preservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    port.write(paletteWithTwoColors());

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests
    )

#include "next_platform_application_services_custom_color_palette_preferences_port_tests.moc"
