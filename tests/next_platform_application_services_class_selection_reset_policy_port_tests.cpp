#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_selection_reset_policy.h"
#include "next/platform/application_services_class_selection_reset_policy_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-selection-reset-policy-%1.tps").arg(
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
    return QStringLiteral("classes_navigation_class_selection_reset_policy");
}

} // namespace

static_assert(
    !std::is_copy_constructible_v<
        ApplicationServicesClassSelectionResetPolicyPort
        >
    );
static_assert(
    !std::is_move_constructible_v<
        ApplicationServicesClassSelectionResetPolicyPort
        >
    );

class NextPlatformApplicationServicesClassSelectionResetPolicyPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingSettingDefaultsToApplicationCloseAndPersists();
    void validUnknownValuesDefaultWithoutRewrite();
    void unavailableSettingsReturnTechnicalFailure();
    void loadsPageLeaveValue();
    void roundTripsBothValuesUsingTheExactLegacyKey();
    void invalidEnumIsRejectedWithoutChangingStoredValue();
    void closedSessionReturnsTechnicalFailureAndPreservesSettings();
    void failedSaveReturnsTechnicalDiagnosticAndPreservesSettings();
    void repositoryReadFailureReturnsTechnicalErrorWithoutMaterializingDefault();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
missingSettingDefaultsToApplicationCloseAndPersists()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ClassSelectionResetPolicy::OnApplicationClose);

    const auto stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("on_application_close"));
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
validUnknownValuesDefaultWithoutRewrite()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    const std::array<QVariant, 3> unknownValues = {{
        QVariant(QStringLiteral("unsupported")),
        QVariant(QStringLiteral("")),
        QVariant(1)
    }};

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    for (const QVariant& value : unknownValues)
    {
        QVERIFY(repository->saveSetting(policyKey(), value));
        const auto loaded = port.load();
        QVERIFY(loaded);
        QCOMPARE(loaded.value(), ClassSelectionResetPolicy::OnApplicationClose);

        const auto stored = repository->loadSetting(policyKey());
        QVERIFY(stored);
        QVERIFY(stored->isValid());
        QCOMPARE(stored->toString(), value.toString());
    }
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
unavailableSettingsReturnTechnicalFailure()
{
    ApplicationServices services;
    ApplicationServicesClassSelectionResetPolicyPort port(services);
    QVERIFY(services.dataService());
    QVERIFY(!services.databaseSession()->isOpen());
    QVERIFY(!services.databaseSession()->settingsRepository());

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ClassSelectionResetPolicy::OnApplicationClose);
    const auto saved = port.save(ClassSelectionResetPolicy::OnPageLeave);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);
    QVERIFY(saved.error().message.find("no database session is open")
        != std::string::npos);
    const auto reloaded = port.load();
    QVERIFY(reloaded);
    QCOMPARE(reloaded.value(), ClassSelectionResetPolicy::OnApplicationClose);
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
loadsPageLeaveValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            policyKey(),
            QStringLiteral("  ON_PAGE_LEAVE ")
            )
        );

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ClassSelectionResetPolicy::OnPageLeave);
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
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

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    for (const auto expected : {
             ClassSelectionResetPolicy::OnApplicationClose,
             ClassSelectionResetPolicy::OnPageLeave
         })
    {
        const auto saved = port.save(expected);
        QVERIFY(saved);
        const auto loaded = port.load();
        QVERIFY(loaded);
        QCOMPARE(loaded.value(), expected);

        const auto stored = repository->loadSetting(policyKey());
        QVERIFY(stored);
        QCOMPARE(
            stored->toString(),
            expected == ClassSelectionResetPolicy::OnPageLeave
                ? QStringLiteral("on_page_leave")
                : QStringLiteral("on_application_close")
            );
    }

    const auto unrelated = repository->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
invalidEnumIsRejectedWithoutChangingStoredValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        policyKey(),
        QStringLiteral("on_page_leave")
        ));
    const auto original = repository->loadSetting(policyKey());
    QVERIFY(original);
    QCOMPARE(original->toString(), QStringLiteral("on_page_leave"));

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    const auto saved = port.save(
        static_cast<ClassSelectionResetPolicy>(17)
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!saved.error().recoverable);

    const auto stored = repository->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("on_page_leave"));
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
closedSessionReturnsTechnicalFailureAndPreservesSettings()
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
        QStringLiteral("on_page_leave")
        ));
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ClassSelectionResetPolicy::OnApplicationClose);
    const auto saved = port.save(
        ClassSelectionResetPolicy::OnApplicationClose
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);
    QVERIFY(saved.error().message.find("no database session is open")
        != std::string::npos);

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    repository = session->settingsRepository();
    QVERIFY(repository);
    const auto storedPolicy = repository->loadSetting(policyKey());
    QVERIFY(storedPolicy);
    QCOMPARE(storedPolicy->toString(), QStringLiteral("on_page_leave"));
    const auto unrelated = repository->loadSetting(unrelatedKey);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
failedSaveReturnsTechnicalDiagnosticAndPreservesSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        policyKey(),
        QStringLiteral("on_page_leave")
        ));
    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_class_selection_reset_policy_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_class_selection_reset_policy'
            BEGIN
                SELECT RAISE(ABORT, 'forced class-selection reset policy write failure');
            END
        )")
        ));

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    QTest::failOnWarning();
    const auto saved = port.save(
        ClassSelectionResetPolicy::OnApplicationClose
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);
    QVERIFY(saved.error().message.find(
        "forced class-selection reset policy write failure"
        ) != std::string::npos);

    const auto storedPolicy = repository->loadSetting(policyKey());
    QVERIFY(storedPolicy);
    QCOMPARE(storedPolicy->toString(), QStringLiteral("on_page_leave"));
    const auto unrelated = repository->loadSetting(unrelatedKey);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
repositoryReadFailureReturnsTechnicalErrorWithoutMaterializingDefault()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    QTest::failOnWarning();
    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
    QVERIFY(loaded.error().recoverable);
    QVERIFY(!loaded.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassSelectionResetPolicyPortTests
    )

#include "next_platform_application_services_class_selection_reset_policy_port_tests.moc"
