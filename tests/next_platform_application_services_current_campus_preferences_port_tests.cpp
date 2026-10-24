#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/platform/application_services_current_campus_preferences_port.h"

#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next::Platform;

namespace
{

using CurrentCampusPreferenceValue =
    ClassMngr::Next::Application::CurrentCampusPreferenceValue;
using CurrentCampusPreferencesPort =
    ClassMngr::Next::Application::CurrentCampusPreferencesPort;

static_assert(std::is_same_v<
              decltype(std::declval<const CurrentCampusPreferencesPort&>().read()),
              ClassMngr::Next::Domain::Result<CurrentCampusPreferenceValue>
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const CurrentCampusPreferencesPort&>().write(
                  std::declval<const CurrentCampusPreferenceValue&>()
                  )),
              ClassMngr::Next::Domain::Result<void>
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const CurrentCampusPreferenceValue&>().value()),
              const std::string&
              >);

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
    WorkspaceStore* const session = services.workspaceStore();
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
    void preservesEmbeddedNulBytes();
    void writePersistsExactKeyAndPreservesUnrelatedSettings();
    void missingAndUnavailableReadEmpty();
    void reportsSettingsAvailability();
    void closedSessionDefaultsAndIgnoresWrite();
    void repositoryReadErrorsReturnTechnicalFailure();
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
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto loaded = port.read();
    QVERIFY(loaded);
    QCOMPARE(loaded.value().value(), utf8(expected));

    const auto stored = repository->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), expected);
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
preservesEmbeddedNulBytes()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    std::string expected("legacy\0 campus / ", 17);
    expected.append("\xF0\x9F\xA7\xAD", 4);
    const CurrentCampusPreferenceValue opaqueValue(expected);
    QCOMPARE(opaqueValue.value().size(), expected.size());
    QCOMPARE(opaqueValue.value(), expected);

    ApplicationServicesCurrentCampusPreferencesPort port(services);
    QVERIFY(port.write(opaqueValue));

    const auto loaded = port.read();
    QVERIFY(loaded);
    QCOMPARE(loaded.value().value().size(), expected.size());
    QCOMPARE(loaded.value().value(), expected);
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
writePersistsExactKeyAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    QVERIFY(port.write(CurrentCampusPreferenceValue(utf8(expected))));
    const auto loaded = port.read();
    QVERIFY(loaded);
    QCOMPARE(loaded.value().value(), utf8(expected));

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
    const auto missing = port.read();
    QVERIFY(missing);
    QVERIFY(missing.value().value().empty());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    WorkspaceStore* const unavailableSession =
        unavailableServices.workspaceStore();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesCurrentCampusPreferencesPort unavailablePort(
        unavailableServices
        );
    const auto unavailable = unavailablePort.read();
    QVERIFY(unavailable);
    QVERIFY(unavailable.value().value().empty());
    QVERIFY(unavailablePort.write(CurrentCampusPreferenceValue("ignored")));

    ApplicationServicesCurrentCampusPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult = nullPort.read();
    QVERIFY(nullResult);
    QVERIFY(nullResult.value().value().empty());
    QVERIFY(nullPort.write(CurrentCampusPreferenceValue("ignored")));
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
reportsSettingsAvailability()
{
    ApplicationServices availableServices;
    QVERIFY(openDatabase(availableServices, m_directory));
    QVERIFY(availableServices.dataService());
    QVERIFY(availableServices.workspaceStore());
    QVERIFY(availableServices.workspaceStore()->isOpen());
    ApplicationServicesCurrentCampusPreferencesPort availablePort(
        &availableServices
        );
    QVERIFY(availablePort.isAvailable());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    QVERIFY(unavailableServices.workspaceStore());
    QVERIFY(!unavailableServices.workspaceStore()->isOpen());
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
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto closed = port.read();
    QVERIFY(closed);
    QVERIFY(closed.value().value().empty());
    QVERIFY(port.write(CurrentCampusPreferenceValue(
        utf8(QStringLiteral("Campus After Close"))
        )));

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
repositoryReadErrorsReturnTechnicalFailure()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto read = port.read();

    QVERIFY(!read);
    QCOMPARE(
        read.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(read.error().recoverable);
    QVERIFY(
        read.error().message.find("Loading application setting")
            != std::string::npos
        );
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
preservesQVariantToStringConversion()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto integerValue = port.read();
    QVERIFY(integerValue);
    QCOMPARE(integerValue.value().value(), utf8(QStringLiteral("42")));

    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            QString()
            )
        );
    const auto emptyValue = port.read();
    QVERIFY(emptyValue);
    QVERIFY(emptyValue.value().value().empty());
}

void NextPlatformApplicationServicesCurrentCampusPreferencesPortTests::
preservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto campus = port.read();
    QVERIFY(campus);
    QCOMPARE(campus.value().value(), utf8(QStringLiteral("Campus B")));

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
    WorkspaceStore* const session = services.workspaceStore();
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
    const auto saved = port.write(CurrentCampusPreferenceValue(
        utf8(QStringLiteral("Campus A"))
        ));

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
