#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/middle_school_analytics_preferences.h"
#include "next/platform/application_services_middle_school_analytics_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Platform;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("middle-school-analytics-preferences-%1.tps").arg(
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

QString preferenceKey()
{
    return QStringLiteral(
        "classes_navigation_show_middle_school_analytics_and_evaluations"
        );
}

} // namespace

class NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingSettingDefaultsFalseAndPersists();
    void preservesLegacyQVariantCoercion();
    void readFailureDefaultsFalse();
    void saveFailureIsSilentAndPreservesStoredValue();
    void unavailableSettingsDefaultFalseAndIgnoreSave();
    void closedSessionDefaultsFalseAndDoesNotOverwritePersistedValue();
    void roundTripsBothValuesUsingTheExactLegacyKey();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
missingSettingDefaultsFalseAndPersists()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    QCOMPARE(port.load(), false);

    auto stored = repository->loadSetting(preferenceKey());
    QVERIFY(stored);
    QVERIFY(stored->isValid());
    QCOMPARE(stored->toBool(), false);

    QVERIFY(repository->saveSetting(preferenceKey(), QVariant()));
    QCOMPARE(port.load(), false);
    stored = repository->loadSetting(preferenceKey());
    QVERIFY(stored);
    QVERIFY(stored->isValid());
    QCOMPARE(stored->toBool(), false);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
preservesLegacyQVariantCoercion()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const std::array<std::pair<QVariant, bool>, 6> values = {{
        {QVariant(QStringLiteral("true")), true},
        {QVariant(QStringLiteral("false")), false},
        {QVariant(1), true},
        {QVariant(0), false},
        {QVariant(true), true},
        {QVariant(false), false}
    }};

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    for (const auto& [value, expected] : values)
    {
        QVERIFY(repository->saveSetting(preferenceKey(), value));
        QCOMPARE(port.load(), expected);
    }
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
readFailureDefaultsFalse()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    QCOMPARE(port.load(), false);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
saveFailureIsSilentAndPreservesStoredValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(preferenceKey(), true));

    QSqlQuery query(session->database());
    const QString rejectPreferenceWrite = QStringLiteral(R"(
        CREATE TRIGGER fail_middle_school_analytics_setting_write
        BEFORE INSERT ON app_settings
        WHEN NEW.key = '%1'
        BEGIN
            SELECT RAISE(ABORT, 'forced preference save failure');
        END
    )").arg(preferenceKey());
    QVERIFY(query.exec(rejectPreferenceWrite));

    const Status rejectedWrite = repository->saveSetting(
        preferenceKey(),
        false
        );
    QVERIFY(!rejectedWrite);

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    QTest::failOnWarning();
    port.save(false);
    QVERIFY(session->isOpen());

    const auto persisted = repository->loadSetting(preferenceKey());
    QVERIFY(persisted);
    QCOMPARE(persisted->toBool(), true);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
unavailableSettingsDefaultFalseAndIgnoreSave()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    QVERIFY(!session->settingsRepository());
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);

    QCOMPARE(port.load(), false);
    port.save(true);
    QCOMPARE(port.load(), false);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
closedSessionDefaultsFalseAndDoesNotOverwritePersistedValue()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(preferenceKey(), true));

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    QCOMPARE(port.load(), false);
    port.save(false);

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    SettingsRepository* const reopenedRepository =
        session->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto persisted = reopenedRepository->loadSetting(preferenceKey());
    QVERIFY(persisted);
    QCOMPARE(persisted->toBool(), true);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
roundTripsBothValuesUsingTheExactLegacyKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QStringLiteral("classes_navigation_unrelated_preference"),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    for (const bool expected : {true, false})
    {
        port.save(expected);
        QCOMPARE(port.load(), expected);

        const auto stored = repository->loadSetting(preferenceKey());
        QVERIFY(stored);
        QCOMPARE(stored->toBool(), expected);
    }

    const auto unrelated = repository->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests
    )

#include "next_platform_application_services_middle_school_analytics_preferences_port_tests.moc"
