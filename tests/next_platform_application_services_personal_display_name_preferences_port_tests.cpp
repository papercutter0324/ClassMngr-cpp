#include "core/application_services.h"
#include "data/data_service.h"
#include "next/platform/application_services_personal_display_name_preferences_port.h"

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
    void missingAndUnavailableReadEmpty();
    void preservesUnrelatedSettingsAndDoesNotWrite();

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

QTEST_MAIN(
    NextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests
    )

#include "next_platform_application_services_personal_display_name_preferences_port_tests.moc"
