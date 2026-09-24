#include "core/application_services.h"
#include "data/data_service.h"
#include "next/platform/application_services_personal_signature_preferences_port.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto SignatureModeKey = "myInfo/signatureMode";
constexpr auto TypedSignatureTextKey = "myInfo/typedSignatureText";
constexpr auto TypedSignatureFontKey = "myInfo/typedSignatureFont";
constexpr auto WrongModeKey = "myInfo/signature_mode";
constexpr auto UnrelatedKey = "myInfo/unrelatedSignaturePreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("personal-signature-preferences-%1.tps").arg(
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

QString expectedUtf8Text()
{
    return QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
}

} // namespace

class NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exactKeysRoundTripUtf8WhitespaceAndTypedValues();
    void missingValuesUseExistingDefaultsWithoutWriting();
    void invalidVariantsNormalizeLikeTheLegacyReader();
    void unavailableSettingsReturnFailure();
    void preservesUnrelatedSettingsAndDoesNotWrite();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
exactKeysRoundTripUtf8WhitespaceAndTypedValues()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const QString expectedText = expectedUtf8Text();
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            1
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureTextKey),
            expectedText
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureFontKey),
            2
            )
        );

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (PersonalSignaturePreferences{
            .mode = PersonalSignatureMode::Type,
            .typedSignatureText = utf8(expectedText),
            .typedSignatureFont = 2
        })
        );

    const auto wrongKey = services.dataService()->loadSetting(
        QString::fromUtf8(WrongModeKey)
        );
    QVERIFY(wrongKey);
    QVERIFY(!wrongKey->isValid());
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
missingValuesUseExistingDefaultsWithoutWriting()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (PersonalSignaturePreferences{
            .mode = PersonalSignatureMode::Image,
            .typedSignatureText = {},
            .typedSignatureFont = 0
        })
        );

    for (const char* key : {
             SignatureModeKey,
             TypedSignatureTextKey,
             TypedSignatureFontKey
         })
    {
        const auto stored = services.dataService()->loadSetting(
            QString::fromUtf8(key)
            );
        QVERIFY(stored);
        QVERIFY(!stored->isValid());
    }
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
invalidVariantsNormalizeLikeTheLegacyReader()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            QStringLiteral("not-a-mode")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureTextKey),
            QStringLiteral("  text with spaces  ")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureFontKey),
            QStringLiteral("not-a-font")
            )
        );

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(loaded.value().mode, PersonalSignatureMode::Image);
    QCOMPARE(
        loaded.value().typedSignatureText,
        std::string("  text with spaces  ")
        );
    QCOMPARE(loaded.value().typedSignatureFont, 0);

    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            9
            )
        );
    const auto unknownMode = port.load();
    QVERIFY(unknownMode);
    QCOMPARE(unknownMode.value().mode, PersonalSignatureMode::Image);
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
unavailableSettingsReturnFailure()
{
    ApplicationServices services;
    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    QVERIFY(!port.load());

    ApplicationServicesPersonalSignaturePreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(!nullPort.load());
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
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

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    QVERIFY(port.load());

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));

    for (const char* key : {
             SignatureModeKey,
             TypedSignatureTextKey,
             TypedSignatureFontKey
         })
    {
        const auto stored = services.dataService()->loadSetting(
            QString::fromUtf8(key)
            );
        QVERIFY(stored);
        QVERIFY(!stored->isValid());
    }
}

QTEST_MAIN(
    NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests
    )

#include "next_platform_application_services_personal_signature_preferences_port_tests.moc"
