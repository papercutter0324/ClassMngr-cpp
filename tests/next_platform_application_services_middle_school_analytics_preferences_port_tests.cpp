#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/workspace_store.h"
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
    void readFailureReturnsTechnicalFailureWithoutFalseFallback();
    void saveFailureReturnsTechnicalDiagnosticAndPreservesStoredValue();
    void unavailableSettingsDefaultFalseAndReturnsSaveFailure();
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
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    const auto missing = port.load();
    QVERIFY(missing);
    QCOMPARE(missing.value(), false);

    auto stored = repository->loadSetting(preferenceKey());
    QVERIFY(stored);
    QVERIFY(stored->isValid());
    QCOMPARE(stored->toBool(), false);

    QVERIFY(repository->saveSetting(preferenceKey(), QVariant()));
    const auto invalidValue = port.load();
    QVERIFY(invalidValue);
    QCOMPARE(invalidValue.value(), false);
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
    WorkspaceStore* const session = services.workspaceStore();
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
        const auto loaded = port.load();
        QVERIFY(loaded);
        QCOMPARE(loaded.value(), expected);
    }
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
readFailureReturnsTechnicalFailureWithoutFalseFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!loaded.error().message.empty());

    QSqlQuery tableLookup(session->database());
    QVERIFY(tableLookup.exec(QStringLiteral(R"(
        SELECT COUNT(*)
        FROM sqlite_master
        WHERE type = 'table' AND name = 'app_settings'
    )")));
    QVERIFY(tableLookup.next());
    QCOMPARE(tableLookup.value(0).toInt(), 0);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
saveFailureReturnsTechnicalDiagnosticAndPreservesStoredValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(preferenceKey(), true));
    const QString unrelatedPreferenceKey = QStringLiteral(
        "classes_navigation_unrelated_preference"
        );
    QVERIFY(repository->saveSetting(
        unrelatedPreferenceKey,
        QStringLiteral("preserve after failed write")
        ));

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

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    const auto rejectedSave = port.save(false);
    QVERIFY(!rejectedSave);
    QCOMPARE(rejectedSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(rejectedSave.error().message).contains(
        QStringLiteral("forced preference save failure")
        ));
    QVERIFY(session->isOpen());

    const auto persisted = repository->loadSetting(preferenceKey());
    QVERIFY(persisted);
    QCOMPARE(persisted->toBool(), true);

    const auto unrelated = repository->loadSetting(unrelatedPreferenceKey);
    QVERIFY(unrelated);
    QCOMPARE(
        unrelated->toString(),
        QStringLiteral("preserve after failed write")
        );
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
unavailableSettingsDefaultFalseAndReturnsSaveFailure()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    QVERIFY(!session->settingsRepository());
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);

    const auto unloaded = port.load();
    QVERIFY(unloaded);
    QCOMPARE(unloaded.value(), false);
    const auto saveFailure = port.save(true);
    QVERIFY(!saveFailure);
    QCOMPARE(saveFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(saveFailure.error().message).contains(
        QStringLiteral("no database session is open")
        ));
    const auto stillUnavailable = port.load();
    QVERIFY(stillUnavailable);
    QCOMPARE(stillUnavailable.value(), false);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
closedSessionDefaultsFalseAndDoesNotOverwritePersistedValue()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(preferenceKey(), true));

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    const auto closed = port.load();
    QVERIFY(closed);
    QCOMPARE(closed.value(), false);
    const auto saveFailure = port.save(false);
    QVERIFY(!saveFailure);
    QCOMPARE(saveFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(saveFailure.error().message).contains(
        QStringLiteral("no database session is open")
        ));

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
    WorkspaceStore* const session = services.workspaceStore();
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
        const auto saved = port.save(expected);
        QVERIFY(saved);
        const auto loaded = port.load();
        QVERIFY(loaded);
        QCOMPARE(loaded.value(), expected);

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
