#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next::Platform;
using namespace ClassMngr::Next;
using ClassMngr::Next::Application::AcademicCalendarScheduleJsonText;

static_assert(!std::is_convertible_v<
    AcademicCalendarScheduleJsonText,
    std::string
    >);
static_assert(!std::is_convertible_v<
    std::string,
    AcademicCalendarScheduleJsonText
    >);

namespace
{

constexpr auto AcademicCalendarScheduleKey =
    "calendar/academicSchedule/v1";
constexpr auto UnrelatedKey = "calendar/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("academic-calendar-schedule-%1.tps").arg(
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

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

} // namespace

class NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingAndUnavailableReadEmptyAndReportWriteFailure();
    void readErrorsReturnTechnicalFailure();
    void roundTripsThroughTheExactKey();
    void roundTripsNonAsciiUtf8JsonTextVerbatim();
    void preservesStoredPayloadAndUnrelatedSettings();
    void saveFailureReturnsTechnicalErrorAndPreservesStoredPayload();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
missingAndUnavailableReadEmptyAndReportWriteFailure()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    const auto missing = port.read();
    QVERIFY(missing);
    QVERIFY(missing.value().empty());
    QVERIFY(session->settingsRepository()->saveSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey),
        QStringLiteral("")
        ));
    const auto storedEmpty = port.read();
    QVERIFY(storedEmpty);
    QVERIFY(storedEmpty.value().empty());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    WorkspaceStore* const unavailableSession =
        unavailableServices.workspaceStore();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesAcademicCalendarSchedulePreferencesPort unavailablePort(
        unavailableServices
        );
    const auto unavailable = unavailablePort.read();
    QVERIFY(unavailable);
    QVERIFY(unavailable.value().empty());
    const auto unavailableSave = unavailablePort.write(
        AcademicCalendarScheduleJsonText{R"({"version":1})"}
        );
    QVERIFY(!unavailableSave);
    QCOMPARE(unavailableSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!unavailableSave.error().message.empty());
    const auto unavailableAfterWrite = unavailablePort.read();
    QVERIFY(unavailableAfterWrite);
    QVERIFY(unavailableAfterWrite.value().empty());

    ApplicationServices closedServices;
    const QString closedDatabasePath = databasePath(m_directory);
    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedServices.dataService());
    WorkspaceStore* const closedSession = closedServices.workspaceStore();
    QVERIFY(closedSession);
    QVERIFY(closedSession->isOpen());
    SettingsRepository* const closedRepository =
        closedSession->settingsRepository();
    QVERIFY(closedRepository);
    const QString oldPayload = QStringLiteral(
        R"({"version":1,"profiles":{"old":true}})"
        );
    QVERIFY(closedRepository->saveSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey),
        oldPayload
        ));

    ApplicationServicesAcademicCalendarSchedulePreferencesPort closedPort(
        closedServices
        );
    closedServices.closeDatabase();
    QVERIFY(closedServices.dataService());
    QVERIFY(!closedSession->isOpen());
    const auto closed = closedPort.read();
    QVERIFY(closed);
    QVERIFY(closed.value().empty());
    const auto closedSave = closedPort.write(
        AcademicCalendarScheduleJsonText{
            R"({"version":1,"profiles":{"new":true}})"
        }
        );
    QVERIFY(!closedSave);
    QCOMPARE(closedSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!closedSave.error().message.empty());
    const auto closedAfterWrite = closedPort.read();
    QVERIFY(closedAfterWrite);
    QVERIFY(closedAfterWrite.value().empty());

    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedSession->isOpen());
    SettingsRepository* const reopenedRepository =
        closedSession->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto unchanged = reopenedRepository->loadSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey)
        );
    QVERIFY(unchanged);
    QCOMPARE(unchanged->toString(), oldPayload);

    ApplicationServicesAcademicCalendarSchedulePreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullRead = nullPort.read();
    QVERIFY(nullRead);
    QVERIFY(nullRead.value().empty());
    const auto nullSave = nullPort.write(
        AcademicCalendarScheduleJsonText{R"({"version":1})"}
        );
    QVERIFY(!nullSave);
    QCOMPARE(nullSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!nullSave.error().message.empty());
    const auto nullReadAfterWrite = nullPort.read();
    QVERIFY(nullReadAfterWrite);
    QVERIFY(nullReadAfterWrite.value().empty());
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
readErrorsReturnTechnicalFailure()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    const auto failed = port.read();
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
    QVERIFY(QString::fromStdString(failed.error().message).contains(
        QStringLiteral("Loading application setting")
        ));
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
roundTripsThroughTheExactKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const std::string expected =
        R"({"version":1,"profiles":{"elementary":{},"middle":{}}})";
    ApplicationServices* servicesPointer = &services;
    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(
        servicesPointer
        );
    QVERIFY(port.write(
        AcademicCalendarScheduleJsonText{expected}
        ));

    const auto readBack = port.read();
    QVERIFY(readBack);
    QCOMPARE(readBack.value().value(), expected);

    const auto stored = repository->loadSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), fromUtf8(expected));
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
roundTripsNonAsciiUtf8JsonTextVerbatim()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const std::string expected =
        R"({"version":1,"profiles":{"elementary":{},"middle":{}},"note":")"
        "\xED\x95\x99\xEA\xB5\x90 \xEC\x84\x9C\xEC\x9A\xB8"
        R"("})";
    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    const auto saved = port.write(
        AcademicCalendarScheduleJsonText{expected}
        );
    QVERIFY(saved);

    const auto readBack = port.read();
    QVERIFY(readBack);
    QCOMPARE(readBack.value().value(), expected);

    const auto stored = repository->loadSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), fromUtf8(expected));
    QCOMPARE(stored->toString().toUtf8().toStdString(), expected);
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
preservesStoredPayloadAndUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const QString storedPayload = QString::fromUtf8(
        "{\"version\":1,\"profiles\":{\"elementary\":{\"2026\":{\"termYear\":2026}},\"middle\":{}}}"
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(AcademicCalendarScheduleKey),
            storedPayload
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    const auto stored = port.read();
    QVERIFY(stored);
    QCOMPARE(fromUtf8(stored.value().value()), storedPayload);

    const std::string replacement =
        R"({"version":1,"profiles":{"replacement":true}})";
    QVERIFY(port.write(
        AcademicCalendarScheduleJsonText{replacement}
        ));
    const auto replacementRead = port.read();
    QVERIFY(replacementRead);
    QCOMPARE(replacementRead.value().value(), replacement);

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
saveFailureReturnsTechnicalErrorAndPreservesStoredPayload()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    const std::string initial = R"({"version":1,"profiles":{}})";
    const std::string replacement = R"({"version":1,"profiles":{"changed":true}})";
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    QVERIFY(port.write(
        AcademicCalendarScheduleJsonText{initial}
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_academic_calendar_schedule
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/academicSchedule/v1'
            BEGIN
                SELECT RAISE(ABORT, 'forced academic calendar schedule failure');
            END
        )")
        ));

    const auto failed = port.write(
        AcademicCalendarScheduleJsonText{replacement}
        );
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(failed.error().message).contains(
        QStringLiteral("forced academic calendar schedule failure")
        ));

    const auto persisted = port.read();
    QVERIFY(persisted);
    QCOMPARE(persisted.value().value(), initial);
    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests
    )

#include "next_platform_application_services_academic_calendar_schedule_preferences_port_tests.moc"
