#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/sub_prep_personal_zoom_preferences.h"
#include "next/platform/application_services_sub_prep_personal_zoom_preferences_port.h"

#include <QRegularExpression>
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

bool installFailingSettingReadView(
    ApplicationServices& services,
    const QString& failingKey
    )
{
    if (!executeSql(
            services,
            QStringLiteral(
                "ALTER TABLE app_settings RENAME TO app_settings_preserved"
                )
            ))
    {
        return false;
    }

    return executeSql(
        services,
        QStringLiteral(R"(
            CREATE VIEW app_settings AS
            SELECT
                key,
                CASE
                    WHEN key = '%1'
                    THEN abs(-9223372036854775807 - 1)
                    ELSE value
                END AS value
            FROM app_settings_preserved
        )").arg(failingKey)
        );
}

bool rawSettingExists(
    ApplicationServices& services,
    const QString& key
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM app_settings_preserved WHERE key = ?"
        ));
    query.addBindValue(key);
    return query.exec() && query.next() && query.value(0).toInt() > 0;
}

bool rawSettingEquals(
    ApplicationServices& services,
    const QString& key,
    const QVariant& expected
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    query.prepare(QStringLiteral(
        "SELECT value FROM app_settings_preserved WHERE key = ?"
        ));
    query.addBindValue(key);
    return query.exec() && query.next() && query.value(0) == expected;
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
    void closedSessionFailsWithoutDataServiceFallback();
    void migrationSaveFailureStillReturnsLegacyValues();
    void repositoryReadFailureReturnsTechnicalError();
    void technicalReadFailuresDoNotFallBackOrStartMigration();

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
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(PrimaryLoginIdKey),
        QStringLiteral("  primary / 김 선생님  ")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(PrimaryPasswordKey),
        QStringLiteral("pässword / 비밀번호 📚")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(PrimaryUnavailableKey),
        QStringLiteral("false")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        QStringLiteral("legacy@example.com")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        QStringLiteral("legacy password")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyUnavailableKey),
        true
        ));
    QVERIFY(repository->saveSetting(
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
        repository->loadSetting(QString::fromUtf8(UnrelatedKey))
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
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        QStringLiteral(" legacy@example.com ")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        QStringLiteral(" legacy password ")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyUnavailableKey),
        false
        ));
    QVERIFY(repository->saveSetting(
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
        repository->loadSetting(QString::fromUtf8(PrimaryLoginIdKey))
            ->toString(),
        QStringLiteral(" legacy@example.com ")
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(PrimaryPasswordKey))
            ->toString(),
        QStringLiteral(" legacy password ")
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(PrimaryUnavailableKey))
            ->toBool(),
        false
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
missingValuesUseNADefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

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
        const auto stored = repository->loadSetting(
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

    const auto unavailable = port.load();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!unavailable.error().recoverable);

    ApplicationServicesSubPrepPersonalZoomPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(!nullPort.load());
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
closedSessionFailsWithoutDataServiceFallback()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    SettingsRepository* repository = session->settingsRepository();
    QVERIFY(repository);

    const QString legacyLogin = QStringLiteral("legacy@example.com");
    const QString legacyPassword = QStringLiteral("legacy password");
    constexpr bool legacyUnavailable = false;
    const QString unrelated = QStringLiteral("preserved");
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        legacyLogin
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        legacyPassword
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyUnavailableKey),
        legacyUnavailable
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        unrelated
        ));

    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());

    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!loaded.error().recoverable);

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    repository = session->settingsRepository();
    QVERIFY(repository);

    for (const char* key : {
             PrimaryLoginIdKey,
             PrimaryPasswordKey,
             PrimaryUnavailableKey
         })
    {
        const auto stored = repository->loadSetting(QString::fromUtf8(key));
        QVERIFY(stored);
        QVERIFY(!stored->isValid());
    }

    const auto storedLegacyLogin = repository->loadSetting(
        QString::fromUtf8(LegacyLoginIdKey)
        );
    QVERIFY(storedLegacyLogin);
    QCOMPARE(storedLegacyLogin->toString(), legacyLogin);
    const auto storedLegacyPassword = repository->loadSetting(
        QString::fromUtf8(LegacyPasswordKey)
        );
    QVERIFY(storedLegacyPassword);
    QCOMPARE(storedLegacyPassword->toString(), legacyPassword);
    const auto storedLegacyUnavailable = repository->loadSetting(
        QString::fromUtf8(LegacyUnavailableKey)
        );
    QVERIFY(storedLegacyUnavailable);
    QCOMPARE(storedLegacyUnavailable->toBool(), legacyUnavailable);
    const auto storedUnrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(storedUnrelated);
    QCOMPARE(storedUnrelated->toString(), unrelated);
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
migrationSaveFailureStillReturnsLegacyValues()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyLoginIdKey),
        QStringLiteral("legacy@example.com")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyPasswordKey),
        QStringLiteral("legacy password")
        ));
    QVERIFY(repository->saveSetting(
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
        repository->loadSetting(QString::fromUtf8(PrimaryLoginIdKey))
            ->toString(),
        QStringLiteral("legacy@example.com")
        );
    const auto failedPrimaryPassword = repository->loadSetting(
        QString::fromUtf8(PrimaryPasswordKey)
        );
    QVERIFY(failedPrimaryPassword.has_value());
    QVERIFY(!failedPrimaryPassword->isValid());
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
repositoryReadFailureReturnsTechnicalError()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*myInfo/zoomLoginId.*")
            )
        );

    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
}

void NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests::
technicalReadFailuresDoNotFallBackOrStartMigration()
{
    const QVariantMap legacyValues = {
        {QString::fromUtf8(LegacyLoginIdKey), QStringLiteral("legacy login")},
        {QString::fromUtf8(LegacyPasswordKey), QStringLiteral("legacy pass")},
        {QString::fromUtf8(LegacyUnavailableKey), false}
    };

    for (const QString& failingKey : {
             QString::fromUtf8(PrimaryLoginIdKey),
             QString::fromUtf8(PrimaryPasswordKey),
             QString::fromUtf8(PrimaryUnavailableKey),
             QString::fromUtf8(LegacyLoginIdKey),
             QString::fromUtf8(LegacyPasswordKey),
             QString::fromUtf8(LegacyUnavailableKey)
         })
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));
        QVERIFY(services.dataService());
        SettingsRepository* const repository = settingsRepository(services);
        QVERIFY(repository);
        QVERIFY(repository->saveSettings(legacyValues));

        QVariant seededPrimaryValue;
        if (failingKey == QString::fromUtf8(PrimaryLoginIdKey))
        {
            seededPrimaryValue = QStringLiteral("primary login");
            QVERIFY(repository->saveSetting(
                failingKey,
                seededPrimaryValue
                ));
        }
        else if (failingKey == QString::fromUtf8(PrimaryPasswordKey))
        {
            seededPrimaryValue = QStringLiteral("primary password");
            QVERIFY(repository->saveSetting(
                failingKey,
                seededPrimaryValue
                ));
        }
        else if (failingKey == QString::fromUtf8(PrimaryUnavailableKey))
        {
            seededPrimaryValue = false;
            QVERIFY(repository->saveSetting(failingKey, seededPrimaryValue));
        }

        QVERIFY(installFailingSettingReadView(services, failingKey));
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(
                QStringLiteral("Failed to load setting.*%1.*")
                    .arg(QRegularExpression::escape(failingKey))
                )
            );

        ApplicationServicesSubPrepPersonalZoomPreferencesPort port(services);
        const auto loaded = port.load();
        QVERIFY(!loaded);
        QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
        QVERIFY(loaded.error().message.find(failingKey.toStdString())
            != std::string::npos);

        for (const QString& primaryKey : {
                 QString::fromUtf8(PrimaryLoginIdKey),
                 QString::fromUtf8(PrimaryPasswordKey),
                 QString::fromUtf8(PrimaryUnavailableKey)
             })
        {
            if (primaryKey == failingKey)
            {
                QVERIFY(rawSettingEquals(
                    services,
                    primaryKey,
                    seededPrimaryValue
                    ));
            }
            else
            {
                QVERIFY(!rawSettingExists(services, primaryKey));
            }
        }

        for (auto setting = legacyValues.cbegin();
             setting != legacyValues.cend();
             ++setting)
        {
            QVERIFY(rawSettingExists(services, setting.key()));
        }
    }
}

QTEST_MAIN(NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests)

#include "next_platform_application_services_sub_prep_personal_zoom_preferences_port_tests.moc"
