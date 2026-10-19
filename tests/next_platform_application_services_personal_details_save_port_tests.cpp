#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/application/personal_details_save.h"
#include "next/platform/application_services_personal_details_save_port.h"

#include <QBuffer>
#include <QImage>
#include <QMap>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto NameKey = "myInfo/name";
constexpr auto CampusKey = "myInfo/campus";
constexpr auto ZoomLoginIdKey = "myInfo/zoomLoginId";
constexpr auto ZoomPasswordKey = "myInfo/zoomPassword";
constexpr auto ZoomNotAvailableKey = "myInfo/zoomNotAvailable";
constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto SignatureModeKey = "myInfo/signatureMode";
constexpr auto TypedSignatureTextKey = "myInfo/typedSignatureText";
constexpr auto TypedSignatureFontKey = "myInfo/typedSignatureFont";
constexpr auto WrongNameKey = "myInfo/displayName";
constexpr auto UnrelatedKey = "myInfo/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("personal-details-save-%1.tps").arg(
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

QByteArray sourcePng()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, Qt::black);

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !source.save(&buffer, "PNG"))
    {
        return {};
    }

    return encoded;
}

std::string bytes(const QByteArray& value)
{
    if (value.isEmpty())
    {
        return {};
    }

    return std::string(
        value.constData(),
        static_cast<std::size_t>(value.size())
        );
}

std::string utf8(const QString& value)
{
    return bytes(value.toUtf8());
}

static_assert(std::is_same_v<
              decltype(PersonalDetailsSaveRequest::typedSignatureFont),
              PersonalSignatureFontChoice
              >);
static_assert(std::is_same_v<
              decltype(PersonalDetailsSaveRequest::campus),
              CurrentCampusPreferenceValue
              >);
static_assert(std::is_same_v<
              decltype(PersonalDetailsSaveRequest::signatureImage),
              ClassMngr::Next::Domain::PersonalSignatureImageSaveBytes
              >);
static_assert(!std::is_convertible_v<
              std::string,
              ClassMngr::Next::Domain::PersonalSignatureImageSaveBytes
              >);
static_assert(std::is_constructible_v<
              ClassMngr::Next::Domain::PersonalSignatureImageSaveBytes,
              std::string
              >);
static_assert(!std::is_same_v<
              decltype(PersonalDetailsSaveRequest::typedSignatureFont),
              int
              >);
static_assert(!std::is_convertible_v<PersonalSignatureFontChoice, int>);

QString expectedUtf8Text()
{
    return QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
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

PersonalDetailsSaveRequest replacementRequest(
    const QByteArray& image
    )
{
    return {
        .name = utf8(QString::fromUtf8("  \xEA\xB0\x80\xEB\x82\x98  ")),
        .campus = CurrentCampusPreferenceValue(
            utf8(QStringLiteral("  Jeongja  "))
            ),
        .zoomLoginId = utf8(QStringLiteral(" teacher@example.com ")),
        .zoomPassword = utf8(QStringLiteral(" secret ")),
        .zoomNotAvailable = false,
        .signatureImage = ClassMngr::Next::Domain::PersonalSignatureImageSaveBytes(
            bytes(image)
            ),
        .signatureMode = PersonalSignatureMode::Type,
        .typedSignatureText = utf8(expectedUtf8Text()),
        .typedSignatureFont = PersonalSignatureFontChoice::GreatVibes
    };
}

} // namespace

class NextPlatformApplicationServicesPersonalDetailsSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void savesAllNineExactKeysWithUtf8AndPreparedImage();
    void savesOpaqueCampusBytesIncludingEmbeddedNul();
    void savesAllFourFontChoicesWithTheirExactStoredValues();
    void normalizesModeAndFontValues();
    void saveFailureRollsBackAllKeysAndPreservesUnrelatedSettings();
    void unavailableSettingsReturnFailureWithoutPartialWrites();
    void closedSessionRefusesSaveAndPreservesAllSettingsOnReopen();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
savesAllNineExactKeysWithUtf8AndPreparedImage()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    using SignatureImageSaveBytes =
        ClassMngr::Next::Domain::PersonalSignatureImageSaveBytes;
    const SignatureImageSaveBytes emptyImage;
    QVERIFY(emptyImage.empty());
    QVERIFY(emptyImage.value().empty());
    QVERIFY(emptyImage == SignatureImageSaveBytes{});

    const std::string exactSourceBytes = bytes(source);
    QVERIFY(exactSourceBytes.find('\0') != std::string::npos);
    const SignatureImageSaveBytes ownedImage(exactSourceBytes);
    QVERIFY(!ownedImage.empty());
    QCOMPARE(ownedImage.value(), exactSourceBytes);
    QVERIFY(ownedImage == SignatureImageSaveBytes(exactSourceBytes));

    ApplicationServicesPersonalDetailsSavePort port(services);
    const auto request = replacementRequest(source);
    QCOMPARE(request.campus.value(), utf8(QStringLiteral("  Jeongja  ")));
    QCOMPARE(request.signatureImage.value(), exactSourceBytes);
    const auto saved = port.save(request);
    QVERIFY(saved);

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(NameKey))
            ->toString(),
        QString::fromUtf8("  \xEA\xB0\x80\xEB\x82\x98  ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(CampusKey))
            ->toString(),
        QStringLiteral("  Jeongja  ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomLoginIdKey))
            ->toString(),
        QStringLiteral(" teacher@example.com ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomPasswordKey))
            ->toString(),
        QStringLiteral(" secret ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomNotAvailableKey))
            ->toBool(),
        false
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureImageKey))
            ->toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(source).toBase64()
            )
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureModeKey))
            ->toInt(),
        1
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureTextKey))
            ->toString(),
        expectedUtf8Text()
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureFontKey))
            ->toInt(),
        2
        );

    const auto wrongKey = services.dataService()->loadSetting(
        QString::fromUtf8(WrongNameKey)
        );
    QVERIFY(wrongKey);
    QVERIFY(!wrongKey->isValid());

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
savesOpaqueCampusBytesIncludingEmbeddedNul()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const std::string expectedCampus("legacy\0campus alias", 19);
    PersonalDetailsSaveRequest request;
    request.campus = CurrentCampusPreferenceValue(expectedCampus);
    QCOMPARE(request.campus.value().size(), expectedCampus.size());
    QCOMPARE(request.campus.value(), expectedCampus);

    ApplicationServicesPersonalDetailsSavePort port(services);
    QVERIFY(port.save(request));

    const auto stored = services.dataService()->loadSetting(
        QString::fromUtf8(CampusKey)
        );
    QVERIFY(stored);
    QVERIFY(stored->isValid());
    const QByteArray encoded = stored->toString().toUtf8();
    QCOMPARE(
        std::string(encoded.constData(), static_cast<std::size_t>(encoded.size())),
        expectedCampus
        );
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
normalizesModeAndFontValues()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    PersonalDetailsSaveRequest request;
    request.signatureMode = static_cast<PersonalSignatureMode>(99);
    request.typedSignatureFont = static_cast<PersonalSignatureFontChoice>(99);

    ApplicationServicesPersonalDetailsSavePort port(services);
    QVERIFY(port.save(request));

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureModeKey))
            ->toInt(),
        0
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureFontKey))
            ->toInt(),
        0
        );
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
savesAllFourFontChoicesWithTheirExactStoredValues()
{
    constexpr std::array choices = {
        std::pair{0, PersonalSignatureFontChoice::JustAnotherHand},
        std::pair{1, PersonalSignatureFontChoice::DancingScript},
        std::pair{2, PersonalSignatureFontChoice::GreatVibes},
        std::pair{3, PersonalSignatureFontChoice::Caveat}
    };

    for (const auto& [expectedValue, choice] : choices)
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));
        QVERIFY(services.dataService());

        PersonalDetailsSaveRequest request;
        request.typedSignatureFont = choice;
        ApplicationServicesPersonalDetailsSavePort port(services);
        QVERIFY(port.save(request));

        const auto stored = services.dataService()->loadSetting(
            QString::fromUtf8(TypedSignatureFontKey)
            );
        QVERIFY(stored);
        QCOMPARE(stored->toInt(), expectedValue);
    }
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
saveFailureRollsBackAllKeysAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const QMap<QString, QVariant> initialValues = {
        {QString::fromUtf8(NameKey), QStringLiteral("old name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("old campus")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old password")},
        {QString::fromUtf8(ZoomNotAvailableKey), true},
        {QString::fromUtf8(SignatureImageKey), QStringLiteral("old-image")},
        {QString::fromUtf8(SignatureModeKey), 0},
        {QString::fromUtf8(TypedSignatureTextKey), QStringLiteral("old text")},
        {QString::fromUtf8(TypedSignatureFontKey), 1},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.dataService()->saveSettings(initialValues));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_personal_details_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureFont'
            BEGIN
                SELECT RAISE(ABORT, 'forced personal details save failure');
            END
        )")
        ));

    ApplicationServicesPersonalDetailsSavePort port(services);
    const auto saved = port.save(replacementRequest(sourcePng()));

    QVERIFY(!saved);
    QCOMPARE(
        saved.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );

    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        const auto stored = services.dataService()->loadSetting(setting.key());
        QVERIFY(stored);
        QCOMPARE(*stored, setting.value());
    }
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
unavailableSettingsReturnFailureWithoutPartialWrites()
{
    ApplicationServices services;
    ApplicationServicesPersonalDetailsSavePort port(services);
    const auto saved = port.save({});

    QVERIFY(!saved);
    QCOMPARE(
        saved.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(
        saved.error().message
            == "Personal details settings service is unavailable."
        );

    ApplicationServicesPersonalDetailsSavePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(!nullPort.save({}));
}

void NextPlatformApplicationServicesPersonalDetailsSavePortTests::
closedSessionRefusesSaveAndPreservesAllSettingsOnReopen()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());

    const QMap<QString, QVariant> initialValues = {
        {QString::fromUtf8(NameKey), QStringLiteral("old name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("old campus")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old password")},
        {QString::fromUtf8(ZoomNotAvailableKey), true},
        {QString::fromUtf8(SignatureImageKey), QStringLiteral("old-image")},
        {QString::fromUtf8(SignatureModeKey), 0},
        {QString::fromUtf8(TypedSignatureTextKey), QStringLiteral("old text")},
        {QString::fromUtf8(TypedSignatureFontKey), 1},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.dataService()->saveSettings(initialValues));

    ApplicationServicesPersonalDetailsSavePort port(services);
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!services.databaseSession()->isOpen());

    const auto saved = port.save(replacementRequest(sourcePng()));
    QVERIFY(!saved);
    QCOMPARE(
        saved.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );

    QVERIFY(services.openDatabase(path));
    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        const auto stored = services.dataService()->loadSetting(setting.key());
        QVERIFY(stored);
        QCOMPARE(*stored, setting.value());
    }
}

QTEST_MAIN(NextPlatformApplicationServicesPersonalDetailsSavePortTests)

#include "next_platform_application_services_personal_details_save_port_tests.moc"
