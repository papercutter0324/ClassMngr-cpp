#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/sub_prep_personal_zoom_preferences.h"
#include "next/platform/application_services_sub_prep_personal_zoom_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto PrimaryLoginIdKey = "myInfo/zoomLoginId";
constexpr auto PrimaryPasswordKey = "myInfo/zoomPassword";
constexpr auto PrimaryUnavailableKey = "myInfo/zoomNotAvailable";
constexpr auto LegacyLoginIdKey = "subPrep/personalZoomEmail";
constexpr auto LegacyPasswordKey = "subPrep/personalZoomPassword";
constexpr auto LegacyUnavailableKey = "subPrep/personalZoomNotAvailable";
constexpr auto UnrelatedKey = "subPrep/unrelatedZoomPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-personal-zoom-%1.tps").arg(
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

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
}

std::string utf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

} // namespace

class NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void primaryValuesTakePrecedenceAndPreserveUtf8();
    void legacyValuesFallbackAndMigrate();
    void missingValuesUseNADefaults();
    void unavailableSettingsFailWithoutChangingCallerState();
    void migrationSaveFailureStillReturnsLegacyValues();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
primaryValuesTakePrecedenceAndPreserveUtf8()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(PrimaryLoginIdKey),
        QStringLiteral("  primary / 김 선생님  ")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(PrimaryPasswordKey),
        QStringLiteral("pässword / 비밀번호 📚")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(PrimaryUnavailableKey),
        QStringLiteral("false")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        QStringLiteral("legacy@example.com")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        QStringLiteral("legacy password")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyUnavailableKey),
        true
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));

    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (SubPrepPersonalZoomPreferences{
            .loginId = utf8(QStringLiteral("  primary / 김 선생님  ")),
            .password = utf8(QStringLiteral("pässword / 비밀번호 📚")),
            .unavailable = false
        })
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
legacyValuesFallbackAndMigrate()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        QStringLiteral(" legacy@example.com ")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        QStringLiteral(" legacy password ")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyUnavailableKey),
        false
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));

    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (SubPrepPersonalZoomPreferences{
            .loginId = utf8(QStringLiteral(" legacy@example.com ")),
            .password = utf8(QStringLiteral(" legacy password ")),
            .unavailable = false
        })
        );

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(PrimaryLoginIdKey))
            ->toString(),
        QStringLiteral(" legacy@example.com ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(PrimaryPasswordKey))
            ->toString(),
        QStringLiteral(" legacy password ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(PrimaryUnavailableKey))
            ->toBool(),
        false
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
missingValuesUseNADefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (SubPrepPersonalZoomPreferences{
            .loginId = "N/A",
            .password = "N/A",
            .unavailable = true
        })
        );

    for (const char* key : {
             PrimaryLoginIdKey,
             PrimaryPasswordKey,
             PrimaryUnavailableKey,
             LegacyLoginIdKey,
             LegacyPasswordKey,
             LegacyUnavailableKey
         })
    {
        const auto stored = services.dataService()->loadSetting(
            QString::fromUtf8(key)
            );
        QVERIFY(stored.has_value());
        QVERIFY(!stored->isValid());
    }
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
unavailableSettingsFailWithoutChangingCallerState()
{
    ApplicationServices services;
    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);

    QVERIFY(!port.load());

    ApplicationServicesSubPrepPersonalZoomPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(!nullPort.load());
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
migrationSaveFailureStillReturnsLegacyValues()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        QStringLiteral("legacy@example.com")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        QStringLiteral("legacy password")
        ));
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(LegacyUnavailableKey),
        false
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_sub_prep_zoom_migration
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/zoomPassword'
            BEGIN
                SELECT RAISE(ABORT, 'forced Sub Prep Zoom migration failure');
            END
        )")
        ));

    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (SubPrepPersonalZoomPreferences{
            .loginId = "legacy@example.com",
            .password = "legacy password",
            .unavailable = false
        })
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(PrimaryLoginIdKey))
            ->toString(),
        QStringLiteral("legacy@example.com")
        );
    const auto failedPrimaryPassword = services.dataService()->loadSetting(
        QString::fromUtf8(PrimaryPasswordKey)
        );
    QVERIFY(failedPrimaryPassword.has_value());
    QVERIFY(!failedPrimaryPassword->isValid());
}

QTEST_MAIN(NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests)

#include "next_platform_application_services_sub_prep_personal_zoom_preferences_port_tests.moc"
