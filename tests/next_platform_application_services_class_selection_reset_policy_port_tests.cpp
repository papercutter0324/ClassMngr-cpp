#include "core/application_services.h"
#include "data/data_service.h"
#include "next/application/class_selection_reset_policy.h"
#include "next/platform/application_services_class_selection_reset_policy_port.h"

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
    void invalidSettingDefaultsToApplicationClose();
    void unavailableSettingsDefaultToApplicationCloseAndIgnoreSave();
    void loadsPageLeaveValue();
    void roundTripsBothValuesUsingTheExactLegacyKey();

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

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    QCOMPARE(
        port.load(),
        ClassSelectionResetPolicy::OnApplicationClose
        );

    const auto stored = services.dataService()->loadSetting(policyKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("on_application_close"));
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
invalidSettingDefaultsToApplicationClose()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const std::array<QVariant, 4> invalidValues = {{
        QVariant(QStringLiteral("unsupported")),
        QVariant(QStringLiteral("")),
        QVariant(1),
        QVariant()
    }};

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    for (const QVariant& value : invalidValues)
    {
        QVERIFY(services.dataService()->saveSetting(policyKey(), value));
        QCOMPARE(
            port.load(),
            ClassSelectionResetPolicy::OnApplicationClose
            );
    }
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
unavailableSettingsDefaultToApplicationCloseAndIgnoreSave()
{
    ApplicationServices services;
    ApplicationServicesClassSelectionResetPolicyPort port(services);

    QCOMPARE(
        port.load(),
        ClassSelectionResetPolicy::OnApplicationClose
        );
    port.save(ClassSelectionResetPolicy::OnPageLeave);
    QCOMPARE(
        port.load(),
        ClassSelectionResetPolicy::OnApplicationClose
        );
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
loadsPageLeaveValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(
        services.dataService()->saveSetting(
            policyKey(),
            QStringLiteral("  ON_PAGE_LEAVE ")
            )
        );

    ApplicationServicesClassSelectionResetPolicyPort port(services);
    QCOMPARE(port.load(), ClassSelectionResetPolicy::OnPageLeave);
}

void NextPlatformApplicationServicesClassSelectionResetPolicyPortTests::
roundTripsBothValuesUsingTheExactLegacyKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(
        services.dataService()->saveSetting(
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
        port.save(expected);
        QCOMPARE(port.load(), expected);

        const auto stored = services.dataService()->loadSetting(policyKey());
        QVERIFY(stored);
        QCOMPARE(
            stored->toString(),
            expected == ClassSelectionResetPolicy::OnPageLeave
                ? QStringLiteral("on_page_leave")
                : QStringLiteral("on_application_close")
            );
    }

    const auto unrelated = services.dataService()->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassSelectionResetPolicyPortTests
    )

#include "next_platform_application_services_class_selection_reset_policy_port_tests.moc"
