#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
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
#include <string>
#include <type_traits>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

static_assert(!std::is_convertible_v<
    std::string,
    CustomColorPaletteEntry
    >);
static_assert(!std::is_convertible_v<
    CustomColorPaletteEntry,
    std::string
    >);

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

SettingsRepository* settingsRepository(ApplicationServices& services)
{
    DatabaseSession* const session = services.databaseSession();
    return session && session->isOpen()
        ? session->settingsRepository()
        : nullptr;
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    return query.exec(statement);
}

QString compactJson(const CustomColorPalette& palette)
{
    QJsonArray array;
    for (const CustomColorPaletteEntry& color : palette.hexColors)
    {
        array.append(QString::fromStdString(color.value()));
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact)
        );
}

CustomColorPalette paletteWithTwoColors()
{
    auto palette = defaultCustomColorPalette();
    palette.hexColors[0] = CustomColorPaletteEntry("#123456");
    palette.hexColors[1] = CustomColorPaletteEntry("#abcdef");
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
            QString::fromStdString(source.hexColors[index].value()).trimmed()
            );
        if (color.isValid())
        {
            result.hexColors[index] = CustomColorPaletteEntry(
                color.name(QColor::HexRgb).toStdString()
                );
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
    void invalidSettingValueDefaultsSuccessfully();
    void compactJsonRoundTripUsesExactKey();
    void writePreservesExactTextAndReadCanonicalizes();
    void legacyQStringListJsonAndSeparatorPayloadsLoad();
    void invalidEntriesUseFallbackAndNormalizeToSixteenCanonicalColors();
    void saveFailurePreservesStoredPaletteAndWarning();
    void preservesUnrelatedSettings();
    void closedSessionDefaultsAndIgnoresWritesWithoutFallback();
    void repositoryReadFailureReturnsTechnicalError();

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
    QVERIFY(services.dataService());
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    const auto missingPalette = port.read();
    QVERIFY(missingPalette);
    QVERIFY(missingPalette.value() == defaultCustomColorPalette());
    const auto missing = repository->loadSetting(
        QString::fromUtf8(CustomColorsKey)
        );
    QVERIFY(missing);
    QVERIFY(!missing->isValid());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    QVERIFY(!unavailableServices.databaseSession()->isOpen());
    ApplicationServicesCustomColorPalettePreferencesPort unavailablePort(
        unavailableServices
        );
    const auto unavailableRead = unavailablePort.read();
    QVERIFY(unavailableRead);
    QVERIFY(unavailableRead.value() == defaultCustomColorPalette());

    QVERIFY(unavailablePort.write(paletteWithTwoColors()));
    const auto unavailableReadAfterWrite = unavailablePort.read();
    QVERIFY(unavailableReadAfterWrite);
    QVERIFY(
        unavailableReadAfterWrite.value() == defaultCustomColorPalette()
        );

    ApplicationServicesCustomColorPalettePreferencesPort nullSettingsPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullSettingsRead = nullSettingsPort.read();
    QVERIFY(nullSettingsRead);
    QVERIFY(nullSettingsRead.value() == defaultCustomColorPalette());
    QVERIFY(nullSettingsPort.write(paletteWithTwoColors()));
    const auto nullSettingsReadAfterWrite = nullSettingsPort.read();
    QVERIFY(nullSettingsReadAfterWrite);
    QVERIFY(
        nullSettingsReadAfterWrite.value() == defaultCustomColorPalette()
        );
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
invalidSettingValueDefaultsSuccessfully()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(CustomColorsKey),
        QStringLiteral("not a custom-color palette")
        ));

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    const auto read = port.read();
    QVERIFY(read);
    QVERIFY(read.value() == defaultCustomColorPalette());
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
compactJsonRoundTripUsesExactKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    const CustomColorPalette expected = paletteWithTwoColors();
    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    QVERIFY(port.write(expected));

    const auto actual = port.read();
    const auto expectedRead = canonicalizedPalette(expected);
    QVERIFY(actual);
    QVERIFY(actual.value() == expectedRead);

    const auto stored = repository->loadSetting(
        QString::fromUtf8(CustomColorsKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), compactJson(expected));

    const auto staleKey = repository->loadSetting(
        QStringLiteral("customColors")
        );
    QVERIFY(staleKey);
    QVERIFY(!staleKey->isValid());
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
writePreservesExactTextAndReadCanonicalizes()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    auto suppliedPalette = defaultCustomColorPalette();
    const std::string suppliedText = "  #abcdef  ";
    suppliedPalette.hexColors[0] = CustomColorPaletteEntry(suppliedText);

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    QVERIFY(port.write(suppliedPalette));

    const auto stored = repository->loadSetting(
        QString::fromUtf8(CustomColorsKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), compactJson(suppliedPalette));
    const QJsonDocument storedJson = QJsonDocument::fromJson(
        stored->toString().toUtf8()
        );
    QVERIFY(storedJson.isArray());
    QCOMPARE(
        storedJson.array().size(),
        static_cast<int>(CustomColorPalette::EntryCount)
        );
    QCOMPARE(
        storedJson.array().at(0).toString().toStdString(),
        suppliedText
        );

    ApplicationServicesCustomColorPalettePreferencesPort reader(services);
    const auto loaded = reader.read();
    QVERIFY(loaded);
    const auto expectedRead = canonicalizedPalette(suppliedPalette);
    QVERIFY(loaded.value() == expectedRead);
    QCOMPARE(
        loaded.value().hexColors[0].value(),
        QColor(QStringLiteral("#abcdef"))
            .name(QColor::HexRgb)
            .toStdString()
        );
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
legacyQStringListJsonAndSeparatorPayloadsLoad()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

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
            repository->saveSetting(
                QString::fromUtf8(CustomColorsKey),
                payload
                )
            );
        const auto read = port.read();
        QVERIFY(read);
        QVERIFY(read.value() == expected);
    }
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
invalidEntriesUseFallbackAndNormalizeToSixteenCanonicalColors()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

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
        repository->saveSetting(
            QString::fromUtf8(CustomColorsKey),
            QString::fromUtf8(
                QJsonDocument(source).toJson(QJsonDocument::Compact)
                )
            )
        );

    auto expected = defaultCustomColorPalette();
    expected.hexColors[1] = CustomColorPaletteEntry("#abcdef");
    for (int index = 2; index < 15; ++index)
    {
        expected.hexColors[static_cast<std::size_t>(index)] =
            CustomColorPaletteEntry("#010203");
    }
    expected.hexColors[15] = CustomColorPaletteEntry("#123456");

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    const auto actual = port.read();
    QVERIFY(actual);
    QVERIFY(actual.value() == expected);
    QCOMPARE(
        actual.value().hexColors.size(),
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
    QVERIFY(port.write(initial));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    const QString unrelatedValue = QStringLiteral("preserved");
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        unrelatedValue
        ));
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
    auto attempted = paletteWithTwoColors();
    attempted.hexColors[0] = CustomColorPaletteEntry("#fedcba");
    const auto saveResult = port.write(attempted);
    QVERIFY(!saveResult);
    QCOMPARE(
        saveResult.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!saveResult.error().recoverable);
    QVERIFY(
        QString::fromStdString(saveResult.error().message).contains(
            QStringLiteral("forced custom colors failure")
            )
        );

    const auto actual = port.read();
    const auto expectedRead = canonicalizedPalette(initial);
    QVERIFY(actual);
    QVERIFY(actual.value() == expectedRead);
    const auto stored = repository->loadSetting(
        QString::fromUtf8(CustomColorsKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), compactJson(initial));
    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), unrelatedValue);
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
preservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    QVERIFY(port.write(paletteWithTwoColors()));

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
closedSessionDefaultsAndIgnoresWritesWithoutFallback()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    SettingsRepository* repository = session->settingsRepository();
    QVERIFY(repository);

    const QString initialPayload = compactJson(paletteWithTwoColors());
    const QString unrelatedValue = QStringLiteral("preserved");
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(CustomColorsKey),
        initialPayload
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        unrelatedValue
        ));

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    const auto read = port.read();
    QVERIFY(read);
    QVERIFY(read.value() == defaultCustomColorPalette());
    auto attempted = paletteWithTwoColors();
    attempted.hexColors[0] = CustomColorPaletteEntry("#fedcba");
    QVERIFY(port.write(attempted));

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    repository = session->settingsRepository();
    QVERIFY(repository);
    const auto storedPalette = repository->loadSetting(
        QString::fromUtf8(CustomColorsKey)
        );
    QVERIFY(storedPalette);
    QCOMPARE(storedPalette->toString(), initialPayload);
    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), unrelatedValue);
}

void NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests::
repositoryReadFailureReturnsTechnicalError()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesCustomColorPalettePreferencesPort port(services);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*custom_colors.*")
            )
        );
    const auto read = port.read();
    QVERIFY(!read);
    QCOMPARE(
        read.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!read.error().recoverable);
    QVERIFY(
        QString::fromStdString(read.error().message).contains(
            QStringLiteral("Failed to load custom colors")
            )
        );
}

QTEST_MAIN(
    NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests
    )

#include "next_platform_application_services_custom_color_palette_preferences_port_tests.moc"
