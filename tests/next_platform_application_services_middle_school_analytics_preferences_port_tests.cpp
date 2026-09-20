#include "core/application_services.h"
#include "data/data_service.h"
#include "next/application/middle_school_analytics_preferences.h"
#include "next/platform/application_services_middle_school_analytics_preferences_port.h"

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
    void unavailableSettingsDefaultFalseAndIgnoreSave();
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

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    QCOMPARE(port.load(), false);

    const auto stored = services.dataService()->loadSetting(preferenceKey());
    QVERIFY(stored);
    QVERIFY(stored->isValid());
    QCOMPARE(stored->toBool(), false);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
preservesLegacyQVariantCoercion()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

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
        QVERIFY(services.dataService()->saveSetting(preferenceKey(), value));
        QCOMPARE(port.load(), expected);
    }
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
unavailableSettingsDefaultFalseAndIgnoreSave()
{
    ApplicationServices services;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);

    QCOMPARE(port.load(), false);
    port.save(true);
    QCOMPARE(port.load(), false);
}

void NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests::
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

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort port(services);
    for (const bool expected : {true, false})
    {
        port.save(expected);
        QCOMPARE(port.load(), expected);

        const auto stored = services.dataService()->loadSetting(preferenceKey());
        QVERIFY(stored);
        QCOMPARE(stored->toBool(), expected);
    }

    const auto unrelated = services.dataService()->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests
    )

#include "next_platform_application_services_middle_school_analytics_preferences_port_tests.moc"
