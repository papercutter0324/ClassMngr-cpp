#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/platform/application_services_personal_display_name_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto PersonalDisplayNameKey = "myInfo/name";
constexpr auto UnrelatedKey = "myInfo/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("personal-display-name-%1.tps").arg(
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

class NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exactKeyAndUtf8WhitespaceRoundTrip();
    void writePersistsExactKeyAndPreservesUnrelatedSettings();
    void missingAndUnavailableReadEmpty();
    void preservesUnrelatedSettingsAndDoesNotWrite();
    void writeFailureMapsToTechnicalError();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests::
exactKeyAndUtf8WhitespaceRoundTrip()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const QString expected = QStringLiteral("  김 선생님 / 🧭  ");
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(PersonalDisplayNameKey),
            expected
            )
        );

    ApplicationServicesPersonalDisplayNamePreferencesPort port(services);
    QCOMPARE(port.read(), utf8(expected));

    const auto stored = services.dataService()->loadSetting(
        QString::fromUtf8(PersonalDisplayNameKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), expected);

    const auto wrongKey = services.dataService()->loadSetting(
        QStringLiteral("myInfo/displayName")
        );
    QVERIFY(wrongKey);
    QVERIFY(!wrongKey->isValid());
}

void NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests::
writePersistsExactKeyAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    const QString expected = QStringLiteral("  김 선생님 / 🧭  ");
    ApplicationServicesPersonalDisplayNamePreferencesPort port(services);
    QVERIFY(port.write(utf8(expected)));
    QCOMPARE(port.read(), utf8(expected));

    const auto stored = services.dataService()->loadSetting(
        QString::fromUtf8(PersonalDisplayNameKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), expected);

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests::
missingAndUnavailableReadEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesPersonalDisplayNamePreferencesPort port(services);
    QVERIFY(port.read().empty());

    const auto missing = services.dataService()->loadSetting(
        QString::fromUtf8(PersonalDisplayNameKey)
        );
    QVERIFY(missing);
    QVERIFY(!missing->isValid());

    ApplicationServices unavailableServices;
    ApplicationServicesPersonalDisplayNamePreferencesPort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read().empty());
    QVERIFY(unavailablePort.write("ignored"));
}

void NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests::
preservesUnrelatedSettingsAndDoesNotWrite()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesPersonalDisplayNamePreferencesPort port(services);
    QVERIFY(port.read().empty());

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));

    const auto stillMissing = services.dataService()->loadSetting(
        QString::fromUtf8(PersonalDisplayNameKey)
        );
    QVERIFY(stillMissing);
    QVERIFY(!stillMissing->isValid());
}

void NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests::
writeFailureMapsToTechnicalError()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_personal_display_name_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/name'
            BEGIN
                SELECT RAISE(ABORT, 'forced personal display name failure');
            END
        )")
        ));

    ApplicationServicesPersonalDisplayNamePreferencesPort port(services);
    const auto saved = port.write("Jamie");

    QVERIFY(!saved);
    QCOMPARE(
        saved.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!saved.error().message.empty());
    QVERIFY(
        saved.error().message.find("Saving application setting")
            != std::string::npos
        || saved.error().message.find(
            "forced personal display name failure"
            ) != std::string::npos
        );

    const auto stored = services.dataService()->loadSetting(
        QString::fromUtf8(PersonalDisplayNameKey)
        );
    QVERIFY(stored);
    QVERIFY(!stored->isValid());
}

QTEST_MAIN(
    NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests
    )

#include "next_platform_application_services_personal_display_name_preferences_port_tests.moc"
