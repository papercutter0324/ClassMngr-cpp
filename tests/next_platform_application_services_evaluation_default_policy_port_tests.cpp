#include "core/application_services.h"
#include "data/data_service.h"
#include "next/application/evaluation_default_policy_preferences.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"

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
    void invalidSettingDefaultsToAll();
    void unavailableSettingsDefaultToAll();
    void loadsCurrentOrPreviousTermValue();
    void savesBothValuesUsingTheExactLegacyKey();

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

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);

    const auto stored = services.dataService()->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("all"));
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
invalidSettingDefaultsToAll()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const std::array<QVariant, 4> invalidValues = {{
        QVariant(QStringLiteral("unsupported")),
        QVariant(QStringLiteral("")),
        QVariant(1),
        QVariant()
    }};

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    for (const QVariant& value : invalidValues)
    {
        QVERIFY(services.dataService()->saveSetting(policyKey(), value));
        QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
    }
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
unavailableSettingsDefaultToAll()
{
    ApplicationServices services;
    ApplicationServicesEvaluationDefaultPolicyPort port(services);

    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
    port.save(EvaluationDefaultPolicy::CurrentOrPreviousTerm);
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);
}

void NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests::
loadsCurrentOrPreviousTermValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(
        services.dataService()->saveSetting(
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
    QVERIFY(
        services.dataService()->saveSetting(
            QStringLiteral("classes_navigation_unrelated_preference"),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesEvaluationDefaultPolicyPort port(services);
    port.save(EvaluationDefaultPolicy::CurrentOrPreviousTerm);
    auto stored = services.dataService()->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("current_or_previous_term"));
    QCOMPARE(
        port.load(),
        EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );

    port.save(EvaluationDefaultPolicy::All);
    stored = services.dataService()->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("all"));
    QCOMPARE(port.load(), EvaluationDefaultPolicy::All);

    const auto unrelated = services.dataService()->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(NextPlatformApplicationServicesEvaluationDefaultPolicyPortTests)

#include "next_platform_application_services_evaluation_default_policy_port_tests.moc"
