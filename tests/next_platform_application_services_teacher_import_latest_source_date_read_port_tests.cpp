#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "next/platform/application_services_teacher_import_latest_source_date_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("teacher-import-latest-date-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

QString settingKey()
{
    return QString::fromLatin1(
        TeacherImportRepository::LatestSourceDateSetting);
}

Application::TeacherImportLatestSourceDateReadResult readDate(
    Platform::ApplicationServicesTeacherImportLatestSourceDateReadPort& port
    )
{
    return port.readLatestTeacherImportSourceDate();
}

}

class NextPlatformApplicationServicesTeacherImportLatestSourceDateReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsCanonicalDateAndTreatsMissingOrMalformedValuesAsAbsent();
    void reportsUnavailableSessionAndPersistenceErrors();
};

void NextPlatformApplicationServicesTeacherImportLatestSourceDateReadPortTests::
readsCanonicalDateAndTreatsMissingOrMalformedValuesAsAbsent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTeacherImportLatestSourceDateReadPort port(
        &services);

    auto result = readDate(port);
    QVERIFY(result);
    QVERIFY(!result.value().has_value());

    QSqlQuery insertNull(services.databaseSession()->database());
    insertNull.prepare(QStringLiteral(
        "INSERT INTO app_settings (key, value) VALUES (?, NULL) "
        "ON CONFLICT(key) DO UPDATE SET value=NULL"));
    insertNull.addBindValue(settingKey());
    QVERIFY2(insertNull.exec(), qPrintable(insertNull.lastError().text()));
    result = readDate(port);
    QVERIFY(result);
    QVERIFY(!result.value().has_value());

    QVERIFY(services.databaseSession()->settingsRepository()->saveSetting(
        settingKey(), QString()));
    result = readDate(port);
    QVERIFY(result);
    QVERIFY(!result.value().has_value());

    QVERIFY(services.databaseSession()->settingsRepository()->saveSetting(
        settingKey(), QStringLiteral("not-a-date")));
    result = readDate(port);
    QVERIFY(result);
    QVERIFY(!result.value().has_value());

    QVERIFY(services.databaseSession()->settingsRepository()->saveSetting(
        settingKey(), QStringLiteral("2026-02-30")));
    result = readDate(port);
    QVERIFY(result);
    QVERIFY(!result.value().has_value());

    QVERIFY(services.databaseSession()->settingsRepository()->saveSetting(
        settingKey(), QStringLiteral("2026-09-01")));
    result = readDate(port);
    QVERIFY(result);
    QVERIFY(result.value().has_value());
    QCOMPARE(*result.value(), std::string("2026-09-01"));
}

void NextPlatformApplicationServicesTeacherImportLatestSourceDateReadPortTests::
reportsUnavailableSessionAndPersistenceErrors()
{
    ApplicationServices unopenedServices;
    QVERIFY(unopenedServices.dataService());
    Platform::ApplicationServicesTeacherImportLatestSourceDateReadPort
        unopenedPort(&unopenedServices);
    auto unavailable = readDate(unopenedPort);
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    Platform::ApplicationServicesTeacherImportLatestSourceDateReadPort
        nullServicesPort(nullptr);
    unavailable = readDate(nullServicesPort);
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTeacherImportLatestSourceDateReadPort port(
        &services);

    QSqlQuery dropSettings(services.databaseSession()->database());
    QVERIFY2(dropSettings.exec(QStringLiteral("DROP TABLE app_settings")),
        qPrintable(dropSettings.lastError().text()));

    const auto failed = readDate(port);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
    QVERIFY(!failed.error().recoverable);
}

QTEST_MAIN(
    NextPlatformApplicationServicesTeacherImportLatestSourceDateReadPortTests)

#include "next_platform_application_services_teacher_import_latest_source_date_read_port_tests.moc"
