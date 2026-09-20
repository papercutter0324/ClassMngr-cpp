#include "core/application_services.h"
#include "data/data_service.h"
#include "next/application/schedule_display_preferences.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"

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
    void preservesLegacyQVariantCoercion();

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
}

void NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests::
unavailableSettingsDefaultFalse()
{
    ApplicationServices services;

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(!result.value().use24HourTime);
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

    ApplicationServicesScheduleDisplayPreferencesPort port(services);
    for (const auto& [value, expected] : values)
    {
        QVERIFY(
            services.dataService()->saveSetting(
                QStringLiteral("schedule_use_24h"),
                value
                )
            );

        const auto result = port.load();
        QVERIFY(result);
        QCOMPARE(result.value().use24HourTime, expected);
    }
}

QTEST_MAIN(NextPlatformApplicationServicesScheduleDisplayPreferencesPortTests)

#include "next_platform_application_services_schedule_display_preferences_port_tests.moc"
