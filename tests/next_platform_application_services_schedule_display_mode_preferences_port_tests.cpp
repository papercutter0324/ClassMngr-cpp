#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/schedule_display_mode_preferences.h"
#include "next/platform/application_services_schedule_display_mode_preferences_port.h"

#include <QSqlQuery>
#include <QApplication>
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

constexpr auto CanonicalKey = "schedule_display_mode";
constexpr auto LegacyKey = "schedule_show_intensive";

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

class NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsModernStringModes();
    void fallsBackToLegacyAndMigratesMissingMode();
    void invalidModernModeFallsBackWithoutOverwriting();
    void missingModeDefaultsAndMigratesRegular();
    void savesTypedModesAsCanonicalStrings();
    void unavailableServicesDefaultAndIgnoreSaves();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::readsModernStringModes()
{
    const std::array<std::pair<QString, ScheduleDisplayMode>, 3> values = {{
        {
            QStringLiteral(" regular "),
            ScheduleDisplayMode::Regular
        },
        {
            QStringLiteral("INTENSIVE"),
            ScheduleDisplayMode::Intensive
        },
        {
            QStringLiteral("testing"),
            ScheduleDisplayMode::Testing
        }
    }};

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    ApplicationServicesScheduleDisplayModePreferencesPort port(services);

    for (const auto& [storedValue, expectedMode] : values)
    {
        QVERIFY(
            services.dataService()->saveSetting(
                QString::fromUtf8(CanonicalKey),
                storedValue
                )
            );

        QCOMPARE(port.load(), expectedMode);
    }
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::fallsBackToLegacyAndMigratesMissingMode()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(LegacyKey),
            true
            )
        );

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    QCOMPARE(port.load(), ScheduleDisplayMode::Intensive);

    const auto canonical = services.dataService()->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("intensive"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::invalidModernModeFallsBackWithoutOverwriting()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(LegacyKey),
            false
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(CanonicalKey),
            QStringLiteral("unknown")
            )
        );

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    QCOMPARE(port.load(), ScheduleDisplayMode::Regular);

    const auto canonical = services.dataService()->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("unknown"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::missingModeDefaultsAndMigratesRegular()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    QCOMPARE(port.load(), ScheduleDisplayMode::Regular);

    const auto canonical = services.dataService()->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("regular"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::savesTypedModesAsCanonicalStrings()
{
    const std::array<std::pair<ScheduleDisplayMode, QString>, 3> values = {{
        {
            ScheduleDisplayMode::Regular,
            QStringLiteral("regular")
        },
        {
            ScheduleDisplayMode::Intensive,
            QStringLiteral("intensive")
        },
        {
            ScheduleDisplayMode::Testing,
            QStringLiteral("testing")
        }
    }};

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    ApplicationServicesScheduleDisplayModePreferencesPort port(services);

    for (const auto& [mode, expectedValue] : values)
    {
        port.save(mode);
        const auto canonical = services.dataService()->loadSetting(
            QString::fromUtf8(CanonicalKey)
            );
        QVERIFY(canonical);
        QCOMPARE(canonical->toString(), expectedValue);
    }
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::unavailableServicesDefaultAndIgnoreSaves()
{
    ApplicationServices services;
    ApplicationServicesScheduleDisplayModePreferencesPort port(services);

    QCOMPARE(port.load(), ScheduleDisplayMode::Regular);
    port.save(ScheduleDisplayMode::Intensive);

    QVERIFY(services.dataService());
    const auto canonical = services.dataService()->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(!canonical);
}

QTEST_MAIN(
    NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    )

#include "next_platform_application_services_schedule_display_mode_preferences_port_tests.moc"
