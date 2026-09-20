#include "core/application_services.h"
#include "data/data_service.h"
#include "next/application/class_visibility_preferences.h"
#include "next/platform/application_services_class_visibility_preferences_port.h"

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
    void invalidSettingDefaultsToActiveSchedule();
    void unavailableSettingsDefaultToActiveScheduleAndIgnoreSave();
    void loadsAllClassesValue();
    void roundTripsBothValuesUsingTheExactLegacyKey();

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

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    QCOMPARE(
        port.load(),
        ClassVisibilityScope::ActiveSchedule
        );

    const auto stored = services.dataService()->loadSetting(preferenceKey());
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("active_schedule"));
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
invalidSettingDefaultsToActiveSchedule()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const std::array<QVariant, 4> invalidValues = {{
        QVariant(QStringLiteral("unsupported")),
        QVariant(QStringLiteral("")),
        QVariant(1),
        QVariant()
    }};

    ApplicationServicesClassVisibilityPreferencesPort port(services);
    for (const QVariant& value : invalidValues)
    {
        QVERIFY(services.dataService()->saveSetting(preferenceKey(), value));
        QCOMPARE(
            port.load(),
            ClassVisibilityScope::ActiveSchedule
            );
    }
}

void NextPlatformApplicationServicesClassVisibilityPreferencesPortTests::
unavailableSettingsDefaultToActiveScheduleAndIgnoreSave()
{
    ApplicationServices services;
    ApplicationServicesClassVisibilityPreferencesPort port(services);

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
    QVERIFY(
        services.dataService()->saveSetting(
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
    QVERIFY(
        services.dataService()->saveSetting(
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

        const auto stored = services.dataService()->loadSetting(preferenceKey());
        QVERIFY(stored);
        QCOMPARE(
            stored->toString(),
            expected == ClassVisibilityScope::AllClasses
                ? QStringLiteral("all_classes")
                : QStringLiteral("active_schedule")
            );
    }

    const auto unrelated = services.dataService()->loadSetting(
        QStringLiteral("classes_navigation_unrelated_preference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassVisibilityPreferencesPortTests
    )

#include "next_platform_application_services_class_visibility_preferences_port_tests.moc"
