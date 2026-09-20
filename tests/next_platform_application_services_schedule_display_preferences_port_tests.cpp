#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/schedule_display_preferences.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-display-%1.tps").arg(
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

    const QSqlDatabase database =
        dataService->databaseSession()->database();
    QSqlQuery query(database);
    return query.exec(statement);
}

} // namespace

class NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsTrueSetting();
    void readsFalseSetting();
    void missingSettingDefaultsFalse();
    void unavailableSettingsDefaultFalse();
    void savesAllValuesAndUsesLegacyKeys();
    void preservesLegacyQVariantCoercion();
    void atomicSaveFailureReturnsTypedErrorAndRollsBack();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
readsTrueSetting()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QStringLiteral("schedule_use_24h"),
            QStringLiteral("true")
            )
        );

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(result.value().use24HourTime);
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
readsFalseSetting()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QStringLiteral("schedule_use_24h"),
            QStringLiteral("false")
            )
        );

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(!result.value().use24HourTime);
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
missingSettingDefaultsFalse()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(!result.value().use24HourTime);
    QVERIFY(!result.value().showEnglishNames);
    QVERIFY(!result.value().showWeekends);
    QVERIFY(!result.value().showAllIntensiveHours);
    QVERIFY(!result.value().testingAffectsM1);
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
unavailableSettingsDefaultFalse()
{
    ApplicationServices services;

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(!result.value().use24HourTime);
    QVERIFY(!result.value().showEnglishNames);
    QVERIFY(!result.value().showWeekends);
    QVERIFY(!result.value().showAllIntensiveHours);
    QVERIFY(!result.value().testingAffectsM1);

    const auto saved = port.save({
        .use24HourTime = true,
        .showEnglishNames = true,
        .showWeekends = true,
        .showAllIntensiveHours = true,
        .testingAffectsM1 = true
    });
    QVERIFY(saved);
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
savesAllValuesAndUsesLegacyKeys()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const ScheduleDisplayPreferences expected{
        .use24HourTime = true,
        .showEnglishNames = false,
        .showWeekends = true,
        .showAllIntensiveHours = false,
        .testingAffectsM1 = true
    };

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    QVERIFY(port.save(expected));

    const auto loaded = port.load();
    QVERIFY(loaded);
    QVERIFY(loaded.value() == expected);

    const std::array<std::pair<QString, QString>, 5> storedValues = {{
        {
            QStringLiteral("schedule_use_24h"),
            QStringLiteral("true")
        },
        {
            QStringLiteral(
                "schedule_show_korean_teacher_english_names"
                ),
            QStringLiteral("false")
        },
        {
            QStringLiteral("schedule_show_weekends"),
            QStringLiteral("true")
        },
        {
            QStringLiteral("schedule_show_all_hours_v2"),
            QStringLiteral("false")
        },
        {
            QStringLiteral("schedule_testing_affects_m1"),
            QStringLiteral("true")
        }
    }};
    for (const auto& [key, expectedValue] : storedValues)
    {
        const auto stored = services.dataService()->loadSetting(key);
        QVERIFY(stored);
        QCOMPARE(stored->toString(), expectedValue);
    }
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
preservesLegacyQVariantCoercion()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const std::array<std::pair<QVariant, bool>, 6> values = {{
        {QVariant(QStringLiteral(" TRUE ")), true},
        {QVariant(QStringLiteral(" 0 ")), false},
        {QVariant(1), true},
        {QVariant(0), false},
        {QVariant(true), true},
        {QVariant(false), false}
    }};

    const std::array<QString, 5> keys = {{
        QStringLiteral("schedule_use_24h"),
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("schedule_show_weekends"),
        QStringLiteral("schedule_show_all_hours_v2"),
        QStringLiteral("schedule_testing_affects_m1")
    }};

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    for (const QString& key : keys)
    {
        for (const auto& [value, expected] : values)
        {
            for (const QString& resetKey : keys)
            {
                QVERIFY(
                    services.dataService()->saveSetting(
                        resetKey,
                        false
                        )
                    );
            }
            QVERIFY(
                services.dataService()->saveSetting(
                    key,
                    value
                    )
                );

            const auto result = port.load();
            QVERIFY(result);
            ScheduleDisplayPreferences expectedPreferences;
            if (key == keys.at(0))
            {
                expectedPreferences.use24HourTime = expected;
            }
            else if (key == keys.at(1))
            {
                expectedPreferences.showEnglishNames = expected;
            }
            else if (key == keys.at(2))
            {
                expectedPreferences.showWeekends = expected;
            }
            else if (key == keys.at(3))
            {
                expectedPreferences.showAllIntensiveHours = expected;
            }
            else
            {
                expectedPreferences.testingAffectsM1 = expected;
            }
            QVERIFY(result.value() == expectedPreferences);
        }
    }
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
atomicSaveFailureReturnsTypedErrorAndRollsBack()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const ScheduleDisplayPreferences initial{
        .use24HourTime = false,
        .showEnglishNames = true,
        .showWeekends = false,
        .showAllIntensiveHours = true,
        .testingAffectsM1 = false
    };
    const ScheduleDisplayPreferences replacement{
        .use24HourTime = true,
        .showEnglishNames = false,
        .showWeekends = true,
        .showAllIntensiveHours = false,
        .testingAffectsM1 = true
    };

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    QVERIFY(port.save(initial));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_schedule_preferences
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'schedule_show_weekends'
            BEGIN
                SELECT RAISE(ABORT, 'forced schedule preference failure');
            END
        )")
        ));

    const auto saved = port.save(replacement);
    QVERIFY(!saved);
    QCOMPARE(
        saved.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );

    const auto loaded = port.load();
    QVERIFY(loaded);
    QVERIFY(loaded.value() == initial);
}

QTEST_MAIN(NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests)

#include "next_platform_application_services_schedule_display_preferences_port_tests.moc"
