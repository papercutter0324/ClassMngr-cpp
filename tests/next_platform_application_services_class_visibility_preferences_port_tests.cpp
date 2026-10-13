#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_visibility_preferences.h"
#include "next/platform/application_services_class_visibility_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-visibility-preferences-%1.tps").arg(
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

QString preferenceKey()
{
    return QStringLiteral("classes_navigation_visibility_scope");
}

} // namespace

static_assert(
    !std::is_copy_constructible_v<
        ApplicationServicesClassVisibilityPreferencesPort
        >
    );
static_assert(
    !std::is_move_constructible_v<
        ApplicationServicesClassVisibilityPreferencesPort
        >
    );

class NextPlatformApplicationServicesClassVisibilityPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingSettingDefaultsToActiveScheduleAndPersists();
    void unsupportedSettingDefaultsToActiveScheduleWithoutRewrite();
    void unavailableSettingsDefaultToActiveScheduleAndReportSaveFailure();
    void loadsAllClassesValue();
    void roundTripsBothValuesUsingTheExactLegacyKey();
    void invalidScopeReturnsInvalidInputWithoutChangingSettings();
    void closedSessionDefaultsAndReportsSaveFailureWithoutChangingSettings();
    void failedSaveReturnsTechnicalFailureAndPreservesSettings();
    void repositoryReadFailureDefaultsAndRemainsSilent();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
missingSettingDefaultsToActiveScheduleAndPersists()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    QCOMPARE(
        port.load(),
        ClassVisibilityScope::ActiveSchedule
        );

    const auto stored = repository->loadSetting(preferenceKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("active_schedule"));
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
unsupportedSettingDefaultsToActiveScheduleWithoutRewrite()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    const std::array<QVariant, 3> unsupportedValues = {{
        QVariant(QStringLiteral("unsupported")),
        QVariant(QStringLiteral("")),
        QVariant(1)
    }};

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    for (const QVariant& value : unsupportedValues)
    {
        QVERIFY(repository->saveSetting(preferenceKey(), value));
        QCOMPARE(
            port.load(),
            ClassVisibilityScope::ActiveSchedule
            );

        const auto stored = repository->loadSetting(preferenceKey());
        QVERIFY(stored);
        QVERIFY(stored->isValid());
        QCOMPARE(stored->toString(), value.toString());
    }

}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
unavailableSettingsDefaultToActiveScheduleAndReportSaveFailure()
{
    ApplicationServices services;
    ApplicationServicesClassVisibilityPreferencesPort port(services);
    QVERIFY(services.dataService());
    QVERIFY(!services.databaseSession()->isOpen());
    QVERIFY(!services.databaseSession()->settingsRepository());

    QCOMPARE(
        port.load(),
        ClassVisibilityScope::ActiveSchedule
        );
    const auto saved = port.save(ClassVisibilityScope::AllClasses);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().message.empty());
    QCOMPARE(
        port.load(),
        ClassVisibilityScope::ActiveSchedule
        );
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
loadsAllClassesValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            preferenceKey(),
            QStringLiteral("  ALL_CLASSES ")
            )
        );

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    QCOMPARE(port.load(), ClassVisibilityScope::AllClasses);
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
roundTripsBothValuesUsingTheExactLegacyKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QStringLiteral("classes_navigation_unrelated_preference"),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    for (const auto expected : {
             ClassVisibilityScope::ActiveSchedule,
             ClassVisibilityScope::AllClasses
         })
    {
        QVERIFY(port.save(expected));
        QCOMPARE(port.load(), expected);

        const auto stored = repository->loadSetting(preferenceKey());
        QVERIFY(stored);
        QCOMPARE(
            stored->toString(),
            expected == ClassVisibilityScope::AllClasses
                ? QStringLiteral("all_classes")
                : QStringLiteral("active_schedule")
            );
    }

    const auto unrelated = repository->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
invalidScopeReturnsInvalidInputWithoutChangingSettings()
{
    const auto invalidScope = static_cast<ClassVisibilityScope>(17);

    ApplicationServices unavailableServices;
    ApplicationServicesClassVisibilityPreferencesPort unavailablePort(
        unavailableServices
        );
    const auto unavailableSave = unavailablePort.save(invalidScope);
    QVERIFY(!unavailableSave);
    QCOMPARE(unavailableSave.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!unavailableSave.error().message.empty());
    QVERIFY(!unavailableSave.error().recoverable);

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        preferenceKey(),
        QStringLiteral("all_classes")
        ));

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    const auto saved = port.save(invalidScope);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!saved.error().message.empty());
    QVERIFY(!saved.error().recoverable);

    const auto stored = repository->loadSetting(preferenceKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("all_classes"));
    QCOMPARE(port.load(), ClassVisibilityScope::AllClasses);
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
closedSessionDefaultsAndReportsSaveFailureWithoutChangingSettings()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    SettingsRepository* repository = session->settingsRepository();
    QVERIFY(repository);

    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(
        preferenceKey(),
        QStringLiteral("all_classes")
        ));
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    QCOMPARE(port.load(), ClassVisibilityScope::ActiveSchedule);
    const auto saved = port.save(ClassVisibilityScope::ActiveSchedule);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().message.empty());

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    repository = session->settingsRepository();
    QVERIFY(repository);
    const auto storedScope = repository->loadSetting(preferenceKey());
    QVERIFY(storedScope);
    QCOMPARE(storedScope->toString(), QStringLiteral("all_classes"));
    const auto unrelated = repository->loadSetting(unrelatedKey);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
failedSaveReturnsTechnicalFailureAndPreservesSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(
        preferenceKey(),
        QStringLiteral("all_classes")
        ));
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_class_visibility_preference_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_visibility_scope'
            BEGIN
                SELECT RAISE(ABORT, 'forced class visibility write failure');
            END
        )")
        ));

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    QTest::failOnWarning();
    const auto saved = port.save(ClassVisibilityScope::ActiveSchedule);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().message.empty());

    const auto storedScope = repository->loadSetting(preferenceKey());
    QVERIFY(storedScope);
    QCOMPARE(storedScope->toString(), QStringLiteral("all_classes"));
    const auto unrelated = repository->loadSetting(unrelatedKey);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
repositoryReadFailureDefaultsAndRemainsSilent()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    QTest::failOnWarning();
    QCOMPARE(port.load(), ClassVisibilityScope::ActiveSchedule);
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassVisibilityPreferencesPortTests
    )

#include "next_platform_application_services_class_visibility_preferences_port_tests.moc"
