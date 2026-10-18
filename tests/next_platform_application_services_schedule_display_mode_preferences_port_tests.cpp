#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/schedule_display_mode_preferences.h"
#include "next/platform/application_services_schedule_display_mode_preferences_port.h"

#include <QApplication>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <string>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto CanonicalKey = "schedule_display_mode";
constexpr auto LegacyKey = "schedule_show_intensive";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-display-%1.tps").arg(
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

} // namespace

class NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsModernStringModes();
    void fallsBackToLegacyAndMigratesMissingMode();
    void invalidModernModeFallsBackWithoutOverwriting();
    void missingModeDefaultsAndMigratesRegular();
    void savesTypedModesAsCanonicalStrings();
    void invalidTypedModeIsRejectedWithoutOverwritingStoredValue();
    void unavailableServicesReturnTechnicalFailure();
    void closedSessionReturnsTechnicalFailureAndPreservesSettings();
    void failedSaveReturnsTechnicalDiagnosticAndPreservesSettings();
    void repositoryReadErrorsReturnTechnicalWithoutMigration();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::readsModernStringModes()
{
    const std::array<std::pair<QString, ScheduleDisplayMode>, 3> values = {{
        {
            QStringLiteral(" regular "),
            ScheduleDisplayMode::Regular
        },
        {
            QStringLiteral("INTENSIVE"),
            ScheduleDisplayMode::Intensive
        },
        {
            QStringLiteral("testing"),
            ScheduleDisplayMode::Testing
        }
    }};

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    ApplicationServicesScheduleDisplayModePreferencesPort port(services);

    for (const auto& [storedValue, expectedMode] : values)
    {
        QVERIFY(
            repository->saveSetting(
                QString::fromUtf8(CanonicalKey),
                storedValue
                )
            );

        const auto loaded = port.load();
        QVERIFY(loaded);
        QCOMPARE(loaded.value(), expectedMode);
    }
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::fallsBackToLegacyAndMigratesMissingMode()
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
            QString::fromUtf8(LegacyKey),
            true
            )
        );

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ScheduleDisplayMode::Intensive);

    const auto canonical = repository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("intensive"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::invalidModernModeFallsBackWithoutOverwriting()
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
            QString::fromUtf8(LegacyKey),
            false
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(CanonicalKey),
            QStringLiteral("unknown")
            )
        );

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ScheduleDisplayMode::Regular);

    const auto canonical = repository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("unknown"));

    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyKey),
        true
        ));
    const auto loadedLegacyIntensiveFallback = port.load();
    QVERIFY(loadedLegacyIntensiveFallback);
    QCOMPARE(
        loadedLegacyIntensiveFallback.value(),
        ScheduleDisplayMode::Intensive
        );
    const auto retainedCanonical = repository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(retainedCanonical);
    QCOMPARE(retainedCanonical->toString(), QStringLiteral("unknown"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::missingModeDefaultsAndMigratesRegular()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ScheduleDisplayMode::Regular);

    const auto canonical = repository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("regular"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::savesTypedModesAsCanonicalStrings()
{
    const std::array<std::pair<ScheduleDisplayMode, QString>, 3> values = {{
        {
            ScheduleDisplayMode::Regular,
            QStringLiteral("regular")
        },
        {
            ScheduleDisplayMode::Intensive,
            QStringLiteral("intensive")
        },
        {
            ScheduleDisplayMode::Testing,
            QStringLiteral("testing")
        }
    }};

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    ApplicationServicesScheduleDisplayModePreferencesPort port(services);

    for (const auto& [mode, expectedValue] : values)
    {
        const auto saved = port.save(mode);
        QVERIFY(saved);
        const auto canonical = repository->loadSetting(
            QString::fromUtf8(CanonicalKey)
            );
        QVERIFY(canonical);
        QCOMPARE(canonical->toString(), expectedValue);
    }
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::invalidTypedModeIsRejectedWithoutOverwritingStoredValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(CanonicalKey),
        QStringLiteral("intensive")
        ));

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    const auto saved = port.save(static_cast<ScheduleDisplayMode>(17));
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!saved.error().recoverable);

    const auto canonical = repository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("intensive"));
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ScheduleDisplayMode::Intensive);
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::unavailableServicesReturnTechnicalFailure()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    ApplicationServicesScheduleDisplayModePreferencesPort port(services);

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ScheduleDisplayMode::Regular);
    const auto saved = port.save(ScheduleDisplayMode::Intensive);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(saved.error().message.find("no database session is open")
        != std::string::npos);
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::closedSessionReturnsTechnicalFailureAndPreservesSettings()
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
        QString::fromUtf8(CanonicalKey),
        QStringLiteral("intensive")
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(LegacyKey),
        true
        ));

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), ScheduleDisplayMode::Regular);
    const auto saved = port.save(ScheduleDisplayMode::Testing);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(saved.error().message.find("no database session is open")
        != std::string::npos);

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    SettingsRepository* const reopenedRepository =
        session->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto canonical = reopenedRepository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("intensive"));
    const auto legacy = reopenedRepository->loadSetting(
        QString::fromUtf8(LegacyKey)
        );
    QVERIFY(legacy);
    QVERIFY(legacy->toBool());
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::failedSaveReturnsTechnicalDiagnosticAndPreservesSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(CanonicalKey),
        QStringLiteral("intensive")
        ));
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));
    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(R"(
        CREATE TRIGGER fail_schedule_display_mode_write
        BEFORE INSERT ON app_settings
        WHEN NEW.key = 'schedule_display_mode'
        BEGIN
            SELECT RAISE(ABORT, 'forced schedule display mode write failure');
        END
        )")));

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    QTest::failOnWarning();
    const auto saved = port.save(ScheduleDisplayMode::Regular);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);
    QVERIFY(saved.error().message.find(
        "forced schedule display mode write failure"
        ) != std::string::npos);

    const auto canonical = repository->loadSetting(
        QString::fromUtf8(CanonicalKey)
        );
    QVERIFY(canonical);
    QCOMPARE(canonical->toString(), QStringLiteral("intensive"));
    const auto unrelated = repository->loadSetting(unrelatedKey);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    ::repositoryReadErrorsReturnTechnicalWithoutMigration()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesScheduleDisplayModePreferencesPort port(services);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*schedule_display_mode.*")
            )
        );
    QTest::failOnWarning();
    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!loaded.error().recoverable);
    QVERIFY(!loaded.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests
    )

#include "next_platform_application_services_schedule_display_mode_preferences_port_tests.moc"
