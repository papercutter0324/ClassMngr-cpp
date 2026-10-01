#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/personal_details_save.h"
#include "next/platform/application_services_personal_details_save_port.h"
#include "next/platform/application_services_personal_signature_preferences_port.h"

#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariantMap>
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
    void repositoryReadFailuresWarnAndUseDefaults();
    void closedSessionRefusesReadAndPreservesSettingsOnReopen();
    void readsValuesWrittenByPersonalDetailsSavePort();

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
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    const QString expectedText = expectedUtf8Text();
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            1
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(TypedSignatureTextKey),
            expectedText
            )
        );
    QVERIFY(
        repository->saveSetting(
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

    const auto wrongKey = repository->loadSetting(
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
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

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
        const auto stored = repository->loadSetting(
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
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    const QString invalidMode = QStringLiteral("not-a-mode");
    const QString typedText = QStringLiteral("  text with spaces  ");
    const QString invalidFont = QStringLiteral("not-a-font");
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            invalidMode
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(TypedSignatureTextKey),
            typedText
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(TypedSignatureFontKey),
            invalidFont
            )
        );

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QCOMPARE(loaded.value().mode, PersonalSignatureMode::Image);
    QCOMPARE(
        loaded.value().typedSignatureText,
        utf8(typedText)
        );
    QCOMPARE(loaded.value().typedSignatureFont, 0);

    const auto originalMode = repository->loadSetting(
        QString::fromUtf8(SignatureModeKey)
        );
    QVERIFY(originalMode);
    QCOMPARE(originalMode->toString(), invalidMode);
    const auto storedText = repository->loadSetting(
        QString::fromUtf8(TypedSignatureTextKey)
        );
    QVERIFY(storedText);
    QCOMPARE(storedText->toString(), typedText);
    const auto storedFont = repository->loadSetting(
        QString::fromUtf8(TypedSignatureFontKey)
        );
    QVERIFY(storedFont);
    QCOMPARE(storedFont->toString(), invalidFont);

    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            9
            )
        );
    const auto unknownMode = port.load();
    QVERIFY(unknownMode);
    QCOMPARE(unknownMode.value().mode, PersonalSignatureMode::Image);

    const auto changedMode = repository->loadSetting(
        QString::fromUtf8(SignatureModeKey)
        );
    QVERIFY(changedMode);
    QCOMPARE(changedMode->toInt(), 9);
    const auto stillStoredText = repository->loadSetting(
        QString::fromUtf8(TypedSignatureTextKey)
        );
    QVERIFY(stillStoredText);
    QCOMPARE(stillStoredText->toString(), typedText);
    const auto stillStoredFont = repository->loadSetting(
        QString::fromUtf8(TypedSignatureFontKey)
        );
    QVERIFY(stillStoredFont);
    QCOMPARE(stillStoredFont->toString(), invalidFont);
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
    const auto nullLoad = nullPort.load();
    QVERIFY(!nullLoad);
    QCOMPARE(
        nullLoad.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
preservesUnrelatedSettingsAndDoesNotWrite()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    QVERIFY(port.load());

    const auto unrelated = repository->loadSetting(
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
        const auto stored = repository->loadSetting(
            QString::fromUtf8(key)
            );
        QVERIFY(stored);
        QVERIFY(!stored->isValid());
    }
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
repositoryReadFailuresWarnAndUseDefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(settingsRepository(services));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    for (const char* key : {
             SignatureModeKey,
             TypedSignatureFontKey,
             TypedSignatureTextKey
         })
    {
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(
                QStringLiteral("Failed to load setting.*%1.*")
                    .arg(QString::fromUtf8(key))
                )
            );
    }

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
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
closedSessionRefusesReadAndPreservesSettingsOnReopen()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    SettingsRepository* repository = settingsRepository(services);
    QVERIFY(repository);

    const QString expectedText = expectedUtf8Text();
    const QVariantMap initialValues = {
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), expectedText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(repository->saveSettings(initialValues));

    ApplicationServicesPersonalSignaturePreferencesPort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!services.databaseSession()->isOpen());

    const auto closedLoad = port.load();
    QVERIFY(!closedLoad);
    QCOMPARE(
        closedLoad.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(
        closedLoad.error().message
            == "Personal signature preferences service is unavailable."
        );

    QVERIFY(services.openDatabase(path));
    repository = settingsRepository(services);
    QVERIFY(repository);
    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        const auto stored = repository->loadSetting(setting.key());
        QVERIFY(stored);
        QCOMPARE(*stored, setting.value());
    }
}

void NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests::
readsValuesWrittenByPersonalDetailsSavePort()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const QString expectedText = expectedUtf8Text();
    PersonalDetailsSaveRequest request;
    request.signatureMode = PersonalSignatureMode::Type;
    request.typedSignatureText = utf8(expectedText);
    request.typedSignatureFont = 2;

    ApplicationServicesPersonalDetailsSavePort savePort(services);
    QVERIFY(savePort.save(request));

    ApplicationServicesPersonalSignaturePreferencesPort readPort(services);
    const auto loaded = readPort.load();

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (PersonalSignaturePreferences{
            .mode = PersonalSignatureMode::Type,
            .typedSignatureText = utf8(expectedText),
            .typedSignatureFont = 2
        })
        );
}

QTEST_MAIN(
    NextPlatformApplicationServicesPersonalSignaturePreferencesPortTests
    )

#include "next_platform_application_services_personal_signature_preferences_port_tests.moc"
