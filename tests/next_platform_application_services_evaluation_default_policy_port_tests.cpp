#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/evaluation_default_policy_preferences.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("evaluation-default-policy-%1.tps").arg(
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

QString policyKey()
{
    return QStringLiteral("classes_navigation_evaluation_default_policy");
}

} // namespace

class NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingSettingDefaultsToAll();
    void validUnsupportedValuesDefaultWithoutRewrite();
    void unavailableSettingsDefaultToAll();
    void loadsCurrentOrPreviousTermValue();
    void savesBothValuesUsingTheExactLegacyKey();
    void closedSessionDefaultsAndIgnoresSaveWithoutChangingSettings();
    void failedSaveIsSilentAndPreservesStoredValue();
    void repositoryReadFailureDefaultsAndRemainsSilent();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
missingSettingDefaultsToAll()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);

    const auto stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("all"));
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
validUnsupportedValuesDefaultWithoutRewrite()
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

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    for (const QVariant& value : unsupportedValues)
    {
        QVERIFY(repository->saveSetting(policyKey(), value));
        QCOMPARE(port.load(), EvaluationDefaultPolicy::All);

        const auto stored = repository->loadSetting(policyKey());
        QVERIFY(stored);
        QVERIFY(stored->isValid());
        QCOMPARE(stored->toString(), value.toString());
    }
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
unavailableSettingsDefaultToAll()
{
    ApplicationServices services;
    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QVERIFY(services.dataService());
    QVERIFY(!services.databaseSession()->isOpen());
    QVERIFY(!services.databaseSession()->settingsRepository());

    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
    port.save(EvaluationDefaultPolicy::CurrentOrPreviousTerm);
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
loadsCurrentOrPreviousTermValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            policyKey(),
            QStringLiteral("  CURRENT_OR_PREVIOUS_TERM ")
            )
        );

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QCOMPARE(
        port.load(),
        EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
savesBothValuesUsingTheExactLegacyKey()
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

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    port.save(EvaluationDefaultPolicy::CurrentOrPreviousTerm);
    auto stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("current_or_previous_term"));
    QCOMPARE(
        port.load(),
        EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );

    port.save(EvaluationDefaultPolicy::All);
    stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("all"));
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);

    const auto unrelated = repository->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
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
        policyKey(),
        QStringLiteral("current_or_previous_term")
        ));
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
    port.save(EvaluationDefaultPolicy::All);

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    repository = session->settingsRepository();
    QVERIFY(repository);
    const auto storedPolicy = repository->loadSetting(policyKey());
    QVERIFY(storedPolicy);
    QCOMPARE(
        storedPolicy->toString(),
        QStringLiteral("current_or_previous_term")
        );
    const auto unrelated = repository->loadSetting(unrelatedKey);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
failedSaveIsSilentAndPreservesStoredValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        policyKey(),
        QStringLiteral("current_or_previous_term")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_evaluation_default_policy_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_evaluation_default_policy'
            BEGIN
                SELECT RAISE(ABORT, 'forced evaluation policy write failure');
            END
        )")
        ));

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QTest::failOnWarning();
    port.save(EvaluationDefaultPolicy::All);

    const auto storedPolicy = repository->loadSetting(policyKey());
    QVERIFY(storedPolicy);
    QCOMPARE(
        storedPolicy->toString(),
        QStringLiteral("current_or_previous_term")
        );
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
repositoryReadFailureDefaultsAndRemainsSilent()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QTest::failOnWarning();
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
}

QTEST_MAIN(NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests)

#include "next_platform_application_services_evaluation_default_policy_port_tests.moc"
