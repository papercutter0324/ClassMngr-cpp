#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/platform/application_services_current_campus_preferences_port.h"

#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto CurrentCampusKey = "myInfo/campus";
constexpr auto UnrelatedKey = "myInfo/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("current-campus-preferences-%1.tps").arg(
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
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
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

class NextPlatformApplicationServicesCurrentCampusPreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exactKeyAndVerbatimStringRoundTrip();
    void writePersistsExactKeyAndPreservesUnrelatedSettings();
    void missingAndUnavailableReadEmpty();
    void reportsSettingsAvailability();
    void closedSessionDefaultsAndIgnoresWrite();
    void repositoryReadErrorsWarnAndReturnEmpty();
    void preservesQVariantToStringConversion();
    void preservesUnrelatedSettings();
    void writeFailureMapsToTechnicalError();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
exactKeyAndVerbatimStringRoundTrip()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const QString expected = QStringLiteral("  Campus A / Main  ");
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            expected
            )
        );

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QCOMPARE(port.read(), utf8(expected));

    const auto stored = repository->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), expected);
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
writePersistsExactKeyAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    const QString expected = QStringLiteral("  캠퍼스 A / 🧭  ");
    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QVERIFY(port.write(utf8(expected)));
    QCOMPARE(port.read(), utf8(expected));

    const auto stored = repository->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), expected);

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
missingAndUnavailableReadEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QVERIFY(port.read().empty());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    DatabaseSession* const unavailableSession =
        unavailableServices.databaseSession();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesCurrentCampusPreferencesPort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read().empty());
    QVERIFY(unavailablePort.write("ignored"));

    ApplicationServicesCurrentCampusPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(nullPort.read().empty());
    QVERIFY(nullPort.write("ignored"));
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
reportsSettingsAvailability()
{
    ApplicationServices availableServices;
    QVERIFY(openDatabase(availableServices, m_directory));
    QVERIFY(availableServices.dataService());
    QVERIFY(availableServices.databaseSession());
    QVERIFY(availableServices.databaseSession()->isOpen());
    ApplicationServicesCurrentCampusPreferencesPort availablePort(
        &availableServices
        );
    QVERIFY(availablePort.isAvailable());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    QVERIFY(unavailableServices.databaseSession());
    QVERIFY(!unavailableServices.databaseSession()->isOpen());
    ApplicationServicesCurrentCampusPreferencesPort unavailablePort(
        &unavailableServices
        );
    QVERIFY(!unavailablePort.isAvailable());

    ApplicationServicesCurrentCampusPreferencesPort nullApplicationPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(!nullApplicationPort.isAvailable());
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
closedSessionDefaultsAndIgnoresWrite()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        QStringLiteral("Campus Before Close")
        ));

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!port.isAvailable());
    QVERIFY(port.read().empty());
    QVERIFY(port.write(utf8(QStringLiteral("Campus After Close"))));

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    SettingsRepository* const reopenedRepository =
        session->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto stored = reopenedRepository->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("Campus Before Close"));
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
repositoryReadErrorsWarnAndReturnEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*myInfo/campus.*")
            )
        );

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QVERIFY(port.read().empty());
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
preservesQVariantToStringConversion()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            42
            )
        );
    QCOMPARE(port.read(), utf8(QStringLiteral("42")));

    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            QString()
            )
        );
    QVERIFY(port.read().empty());
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
preservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            QStringLiteral("Campus B")
            )
        );

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QCOMPARE(port.read(), utf8(QStringLiteral("Campus B")));

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
writeFailureMapsToTechnicalError()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_current_campus_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/campus'
            BEGIN
                SELECT RAISE(ABORT, 'forced current campus failure');
            END
        )")
        ));

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    const auto saved = port.write(utf8(QStringLiteral("Campus A")));

    QVERIFY(!saved);
    QCOMPARE(
        saved.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!saved.error().message.empty());
    QVERIFY(
        saved.error().message.find("Saving application setting")
            != std::string::npos
        || saved.error().message.find("forced current campus failure")
            != std::string::npos
        );

    const auto stored = repository->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(stored);
    QVERIFY(!stored->isValid());
}

QTEST_MAIN(
    NextPlatformApplicationServicesCurrentCampusPreferencesPortTests
    )

#include "next_platform_application_services_current_campus_preferences_port_tests.moc"
