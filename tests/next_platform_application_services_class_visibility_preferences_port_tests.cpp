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
    void unavailableSettingsDefaultToActiveScheduleAndIgnoreSave();
    void loadsAllClassesValue();
    void roundTripsBothValuesUsingTheExactLegacyKey();
    void closedSessionDefaultsAndIgnoresSaveWithoutChangingSettings();
    void failedSaveIsSilentAndPreservesStoredValue();
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
unavailableSettingsDefaultToActiveScheduleAndIgnoreSave()
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
    port.save(ClassVisibilityScope::AllClasses);
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
        port.save(expected);
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
closedSessionDefaultsAndIgnoresSaveWithoutChangingSettings()
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
    port.save(ClassVisibilityScope::ActiveSchedule);

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
failedSaveIsSilentAndPreservesStoredValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        preferenceKey(),
        QStringLiteral("all_classes")
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
    port.save(ClassVisibilityScope::ActiveSchedule);

    const auto storedScope = repository->loadSetting(preferenceKey());
    QVERIFY(storedScope);
    QCOMPARE(storedScope->toString(), QStringLiteral("all_classes"));
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
