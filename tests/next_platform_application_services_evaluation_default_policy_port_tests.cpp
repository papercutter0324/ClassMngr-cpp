#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/evaluation_default_policy_preferences.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <string>

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
    WorkspaceStore* const session = services.workspaceStore();
    return session && session->isOpen()
        ? session->settingsRepository()
        : nullptr;
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    WorkspaceStore* const session = services.workspaceStore();
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
    void invalidEnumSaveReturnsInvalidInputAndPreservesSettings();
    void closedSessionDefaultsAndIgnoresSaveWithoutChangingSettings();
    void failedSaveReturnsTechnicalDiagnosticAndPreservesSettings();
    void repositoryReadFailureReturnsTechnicalWithoutMaterializingFallback();

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
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), EvaluationDefaultPolicy::All);

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
        const auto loaded = port.load();
        QVERIFY(loaded);
        QCOMPARE(loaded.value(), EvaluationDefaultPolicy::All);

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
    QVERIFY(!services.workspaceStore()->isOpen());
    QVERIFY(!services.workspaceStore()->settingsRepository());

    const auto unavailablePolicy = port.load();
    QVERIFY(unavailablePolicy);
    QCOMPARE(unavailablePolicy.value(), EvaluationDefaultPolicy::All);
    const auto saved = port.save(
        EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(saved.error().message.find("no database session is open")
        != std::string::npos);
    const auto stillUnavailablePolicy = port.load();
    QVERIFY(stillUnavailablePolicy);
    QCOMPARE(stillUnavailablePolicy.value(), EvaluationDefaultPolicy::All);
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
    const auto currentOrPreviousTermLoaded = port.load();
    QVERIFY(currentOrPreviousTermLoaded);
    QCOMPARE(
        currentOrPreviousTermLoaded.value(),
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
    const auto currentOrPreviousTermSaved = port.save(
        EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );
    QVERIFY(currentOrPreviousTermSaved);
    auto stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("current_or_previous_term"));
    const auto currentOrPreviousTermLoaded = port.load();
    QVERIFY(currentOrPreviousTermLoaded);
    QCOMPARE(
        currentOrPreviousTermLoaded.value(),
        EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );

    const auto allSaved = port.save(EvaluationDefaultPolicy::All);
    QVERIFY(allSaved);
    stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("all"));
    const auto allLoaded = port.load();
    QVERIFY(allLoaded);
    QCOMPARE(allLoaded.value(), EvaluationDefaultPolicy::All);

    const auto unrelated = repository->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
invalidEnumSaveReturnsInvalidInputAndPreservesSettings()
{
    const auto invalidPolicy = static_cast<EvaluationDefaultPolicy>(99);
    ApplicationServices unavailableServices;
    ApplicationServicesEvaluationDefaultPolicyPort unavailablePort(
        unavailableServices
        );
    const auto unavailableSave = unavailablePort.save(invalidPolicy);
    QVERIFY(!unavailableSave);
    QCOMPARE(unavailableSave.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!unavailableSave.error().recoverable);

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        policyKey(),
        QStringLiteral("current_or_previous_term")
        ));

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    const auto saved = port.save(invalidPolicy);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!saved.error().recoverable);

    const auto stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("current_or_previous_term"));
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), EvaluationDefaultPolicy::CurrentOrPreviousTerm);
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
closedSessionDefaultsAndIgnoresSaveWithoutChangingSettings()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto closedPolicy = port.load();
    QVERIFY(closedPolicy);
    QCOMPARE(closedPolicy.value(), EvaluationDefaultPolicy::All);
    const auto closedSessionSave = port.save(EvaluationDefaultPolicy::All);
    QVERIFY(!closedSessionSave);
    QCOMPARE(closedSessionSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(closedSessionSave.error().message.find(
        "no database session is open"
        ) != std::string::npos);

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
failedSaveReturnsTechnicalDiagnosticAndPreservesSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        policyKey(),
        QStringLiteral("current_or_previous_term")
        ));
    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(unrelatedKey, QStringLiteral("preserved")));
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
    const auto saved = port.save(EvaluationDefaultPolicy::All);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);
    QVERIFY(saved.error().message.find(
        "forced evaluation policy write failure"
        ) != std::string::npos);

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
repositoryReadFailureReturnsTechnicalWithoutMaterializingFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QTest::failOnWarning();
    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!loaded.error().recoverable);
    QVERIFY(!loaded.error().message.empty());

    QSqlQuery tables(services.workspaceStore()->database());
    QVERIFY(tables.exec(QStringLiteral(
        "SELECT name FROM sqlite_master "
        "WHERE type = 'table' AND name = 'app_settings'"
        )));
    QVERIFY(!tables.next());
}

QTEST_MAIN(NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests)

#include "next_platform_application_services_evaluation_default_policy_port_tests.moc"
