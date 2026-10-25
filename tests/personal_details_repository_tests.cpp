#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/repository_error.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/data/signature_image_processor.h"

#include <QBuffer>
#include <QImage>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <expected>
#include <type_traits>

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
constexpr auto LegacyZoomEmailKey = "subPrep/personalZoomEmail";
constexpr auto LegacyZoomPasswordKey = "subPrep/personalZoomPassword";
constexpr auto LegacyZoomNotAvailableKey = "subPrep/personalZoomNotAvailable";
constexpr auto UnavailableDiagnostic = "No Teacher Profile service is available.";

static_assert(std::is_same_v<
              decltype(std::declval<const PersonalDetailsRepository&>().load()),
              RepositoryResult<PersonalDetails>
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const PersonalDetailsRepository&>().save(
                  std::declval<const PersonalDetails&>()
                  )),
              RepositoryStatus
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const PersonalDetailsRepository&>().loadSetting(
                  std::declval<const QString&>(),
                  std::declval<const QVariant&>()
                  )),
              RepositoryResult<QVariant>
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const PersonalDetailsRepository&>().saveCampus(
                  std::declval<const QString&>()
                  )),
              RepositoryStatus
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const PersonalDetailsRepository&>().saveSetting(
                  std::declval<const QString&>(),
                  std::declval<const QVariant&>()
                  )),
              RepositoryStatus
              >);
static_assert(std::is_same_v<
              decltype(std::declval<const PersonalDetailsRepository&>().isAvailable()),
              bool
              >);

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("personal-details-repository-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(ApplicationServices& services, QTemporaryDir& directory)
{
    return services.openDatabase(databasePath(directory)).has_value();
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store || !store->isOpen())
    {
        return false;
    }

    QSqlQuery query(store->database());
    return query.exec(statement);
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

} // namespace

class PersonalDetailsRepositoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingSettingsReturnExistingDefaults();
    void invalidStoredSignatureImageReturnsEmptyWithoutFailingLoad();
    void aggregateSaveWritesNineExactKeysAndPreparedSignatureImage();
    void aggregateSaveFailureRollsBackAllKeys();
    void zoomMigrationUsesLegacyValueAndIgnoresWriteFailure();
    void zoomPrimaryReadFailureDoesNotUseLegacyValue();
    void settingReadFailurePropagatesAsTechnicalWithOriginalDiagnostic();
    void unavailableServicesReturnTypedTechnicalErrors();

private:
    QTemporaryDir m_directory;
};

void PersonalDetailsRepositoryTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void PersonalDetailsRepositoryTests::missingSettingsReturnExistingDefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    PersonalDetailsRepository repository(services.settingsService());

    QVERIFY(repository.isAvailable());
    const RepositoryResult<QVariant> missing = repository.loadSetting(
        QStringLiteral("myInfo/missing"),
        QStringLiteral("caller default")
        );
    QVERIFY(missing);
    QCOMPARE(missing->toString(), QStringLiteral("caller default"));

    const RepositoryResult<PersonalDetails> loaded = repository.load();
    QVERIFY(loaded);
    QCOMPARE(loaded->name, QString());
    QCOMPARE(loaded->campus, QString());
    QCOMPARE(loaded->zoomLoginId, QStringLiteral("N/A"));
    QCOMPARE(loaded->zoomPassword, QStringLiteral("N/A"));
    QVERIFY(loaded->zoomNotAvailable);
    QVERIFY(loaded->signatureImage.isEmpty());
    QCOMPARE(loaded->signatureMode, SignatureMode::Image);
    QCOMPARE(loaded->typedSignatureText, QString());
    QCOMPARE(loaded->typedSignatureFont, 0);

    QVERIFY(repository.saveSetting(
        QString::fromUtf8(SignatureModeKey),
        99
        ));
    QVERIFY(repository.saveSetting(
        QString::fromUtf8(TypedSignatureFontKey),
        QStringLiteral("3")
        ));
    const RepositoryResult<PersonalDetails> normalized = repository.load();
    QVERIFY(normalized);
    QCOMPARE(normalized->signatureMode, SignatureMode::Image);
    QCOMPARE(normalized->typedSignatureFont, 3);
}

void PersonalDetailsRepositoryTests::
invalidStoredSignatureImageReturnsEmptyWithoutFailingLoad()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    PersonalDetailsRepository repository(services.settingsService());

    const QString malformedBase64 =
        QStringLiteral("%%% definitely not base64 %%%");
    QVERIFY(repository.saveSetting(
        QString::fromUtf8(SignatureImageKey),
        malformedBase64
        ));

    const RepositoryResult<PersonalDetails> malformedLoad = repository.load();
    QVERIFY(malformedLoad);
    QVERIFY(malformedLoad->signatureImage.isEmpty());

    const QByteArray unsupportedImageBytes = QByteArrayLiteral(
        "valid base64 that is not an image"
        );
    const QString unsupportedBase64 = QString::fromLatin1(
        unsupportedImageBytes.toBase64()
        );
    QVERIFY(repository.saveSetting(
        QString::fromUtf8(SignatureImageKey),
        unsupportedBase64
        ));

    const RepositoryResult<PersonalDetails> unsupportedLoad = repository.load();
    QVERIFY(unsupportedLoad);
    QVERIFY(unsupportedLoad->signatureImage.isEmpty());
}

void PersonalDetailsRepositoryTests::
aggregateSaveWritesNineExactKeysAndPreparedSignatureImage()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    PersonalDetailsRepository repository(services.settingsService());

    const QByteArray image = sourcePng();
    QVERIFY(!image.isEmpty());

    PersonalDetails details;
    details.name = QStringLiteral("Teacher 김");
    details.campus = QStringLiteral("Campus");
    details.zoomLoginId = QStringLiteral("teacher@example.com");
    details.zoomPassword = QStringLiteral("secret");
    details.zoomNotAvailable = false;
    details.signatureImage = image;
    details.signatureMode = SignatureMode::Type;
    details.typedSignatureText = QStringLiteral("Typed signature");
    details.typedSignatureFont = 2;

    const RepositoryStatus saved = repository.save(details);
    QVERIFY(saved);

    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT key FROM app_settings ORDER BY key"
        )));
    QStringList actualKeys;
    while (query.next())
    {
        actualKeys.append(query.value(0).toString());
    }
    const QStringList expectedKeys{
        QString::fromUtf8(CampusKey),
        QString::fromUtf8(NameKey),
        QString::fromUtf8(SignatureImageKey),
        QString::fromUtf8(SignatureModeKey),
        QString::fromUtf8(TypedSignatureFontKey),
        QString::fromUtf8(TypedSignatureTextKey),
        QString::fromUtf8(ZoomLoginIdKey),
        QString::fromUtf8(ZoomNotAvailableKey),
        QString::fromUtf8(ZoomPasswordKey)
    };
    QCOMPARE(actualKeys, expectedKeys);

    const RepositoryResult<QVariant> storedImage = repository.loadSetting(
        QString::fromUtf8(SignatureImageKey),
        QVariant()
        );
    QVERIFY(storedImage);
    QCOMPARE(
        storedImage->toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(image).toBase64()
            )
        );

    const RepositoryResult<PersonalDetails> loaded = repository.load();
    QVERIFY(loaded);
    QCOMPARE(loaded->name, details.name);
    QCOMPARE(loaded->campus, details.campus);
    QCOMPARE(loaded->zoomLoginId, details.zoomLoginId);
    QCOMPARE(loaded->zoomPassword, details.zoomPassword);
    QCOMPARE(loaded->zoomNotAvailable, details.zoomNotAvailable);
    QCOMPARE(
        loaded->signatureImage,
        SignatureImage::prepareForEmbedding(image)
        );
    QCOMPARE(loaded->signatureMode, SignatureMode::Type);
    QCOMPARE(loaded->typedSignatureText, details.typedSignatureText);
    QCOMPARE(loaded->typedSignatureFont, details.typedSignatureFont);

    const RepositoryStatus campusSaved = repository.saveCampus(
        QStringLiteral("Campus updated independently")
        );
    QVERIFY(campusSaved);
    const RepositoryResult<QVariant> campus = repository.loadSetting(
        QString::fromUtf8(CampusKey),
        QVariant()
        );
    QVERIFY(campus);
    QCOMPARE(campus->toString(), QStringLiteral("Campus updated independently"));
}

void PersonalDetailsRepositoryTests::aggregateSaveFailureRollsBackAllKeys()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    PersonalDetailsRepository repository(services.settingsService());

    PersonalDetails baseline;
    baseline.name = QStringLiteral("baseline name");
    baseline.campus = QStringLiteral("baseline campus");
    baseline.zoomLoginId = QStringLiteral("baseline login");
    baseline.zoomPassword = QStringLiteral("baseline password");
    baseline.zoomNotAvailable = true;
    baseline.signatureImage = sourcePng();
    baseline.signatureMode = SignatureMode::Image;
    baseline.typedSignatureText = QStringLiteral("baseline signature");
    baseline.typedSignatureFont = 1;
    QVERIFY(repository.save(baseline));
    QVERIFY(repository.saveSetting(
        QStringLiteral("myInfo/unrelated"),
        QStringLiteral("preserved")
        ));

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "CREATE TRIGGER reject_personal_details_font "
            "BEFORE INSERT ON app_settings "
            "WHEN NEW.key = 'myInfo/typedSignatureFont' "
            "BEGIN SELECT RAISE(ABORT, 'reject personal details font'); END"
            )
        ));

    PersonalDetails replacement;
    replacement.name = QStringLiteral("replacement name");
    replacement.campus = QStringLiteral("replacement campus");
    replacement.zoomLoginId = QStringLiteral("replacement login");
    replacement.zoomPassword = QStringLiteral("replacement password");
    replacement.zoomNotAvailable = false;
    replacement.signatureImage = QByteArrayLiteral("replacement image");
    replacement.signatureMode = SignatureMode::Type;
    replacement.typedSignatureText = QStringLiteral("replacement signature");
    replacement.typedSignatureFont = 3;

    const Status serviceFailure = services.settingsService()->save(
        QString::fromUtf8(TypedSignatureFontKey),
        replacement.typedSignatureFont
        );
    QVERIFY(!serviceFailure);

    const RepositoryStatus failed = repository.save(replacement);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(failed.error().diagnostic, serviceFailure.error());

    const RepositoryResult<PersonalDetails> loaded = repository.load();
    QVERIFY(loaded);
    QCOMPARE(loaded->name, baseline.name);
    QCOMPARE(loaded->campus, baseline.campus);
    QCOMPARE(loaded->zoomLoginId, baseline.zoomLoginId);
    QCOMPARE(loaded->zoomPassword, baseline.zoomPassword);
    QCOMPARE(loaded->zoomNotAvailable, baseline.zoomNotAvailable);
    QCOMPARE(
        loaded->signatureImage,
        SignatureImage::prepareForEmbedding(baseline.signatureImage)
        );
    QCOMPARE(loaded->signatureMode, baseline.signatureMode);
    QCOMPARE(loaded->typedSignatureText, baseline.typedSignatureText);
    QCOMPARE(loaded->typedSignatureFont, baseline.typedSignatureFont);

    const RepositoryResult<QVariant> unrelated = repository.loadSetting(
        QStringLiteral("myInfo/unrelated"),
        QVariant()
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void PersonalDetailsRepositoryTests::
zoomMigrationUsesLegacyValueAndIgnoresWriteFailure()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    PersonalDetailsRepository repository(services.settingsService());

    QVERIFY(repository.saveSetting(
        QString::fromUtf8(LegacyZoomEmailKey),
        QStringLiteral("legacy@example.com")
        ));
    QVERIFY(repository.saveSetting(
        QString::fromUtf8(LegacyZoomPasswordKey),
        QStringLiteral("legacy-password")
        ));
    QVERIFY(repository.saveSetting(
        QString::fromUtf8(ZoomPasswordKey),
        QStringLiteral("current-password")
        ));
    QVERIFY(repository.saveSetting(
        QString::fromUtf8(LegacyZoomNotAvailableKey),
        false
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "CREATE TRIGGER reject_zoom_login_migration "
            "BEFORE INSERT ON app_settings "
            "WHEN NEW.key = 'myInfo/zoomLoginId' "
            "BEGIN SELECT RAISE(ABORT, 'reject migration'); END"
            )
        ));

    const RepositoryResult<PersonalDetails> loaded = repository.load();
    QVERIFY(loaded);
    QCOMPARE(loaded->zoomLoginId, QStringLiteral("legacy@example.com"));
    QCOMPARE(loaded->zoomPassword, QStringLiteral("current-password"));
    QVERIFY(!loaded->zoomNotAvailable);

    const RepositoryResult<QVariant> loginPrimary = repository.loadSetting(
        QString::fromUtf8(ZoomLoginIdKey),
        QVariant()
        );
    QVERIFY(loginPrimary);
    QVERIFY(!loginPrimary->isValid());

    const RepositoryResult<QVariant> passwordPrimary = repository.loadSetting(
        QString::fromUtf8(ZoomPasswordKey),
        QVariant()
        );
    QVERIFY(passwordPrimary);
    QCOMPARE(passwordPrimary->toString(), QStringLiteral("current-password"));
    const RepositoryResult<QVariant> availabilityPrimary = repository.loadSetting(
        QString::fromUtf8(ZoomNotAvailableKey),
        QVariant()
        );
    QVERIFY(availabilityPrimary);
    QVERIFY(!availabilityPrimary->toBool());
}

void PersonalDetailsRepositoryTests::zoomPrimaryReadFailureDoesNotUseLegacyValue()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QVERIFY(executeSql(
        services,
        QStringLiteral("ALTER TABLE app_settings RENAME TO app_settings_source")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO app_settings_source (key, value) VALUES "
            "('myInfo/zoomLoginId', 'broken primary'), "
            "('subPrep/personalZoomEmail', 'legacy@example.com')"
            )
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "CREATE VIEW app_settings AS "
            "SELECT key, "
            "CASE WHEN key = 'myInfo/zoomLoginId' "
            "THEN abs(-9223372036854775808) ELSE value END AS value "
            "FROM app_settings_source"
            )
        ));

    PersonalDetailsRepository repository(services.settingsService());
    const RepositoryResult<PersonalDetails> loaded = repository.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, RepositoryErrorCode::Technical);
    QVERIFY(loaded.error().diagnostic.startsWith(
        QStringLiteral(
            "Loading application setting failed for setting key "
            "'myInfo/zoomLoginId':"
            )
        ));
}

void PersonalDetailsRepositoryTests::
settingReadFailurePropagatesAsTechnicalWithOriginalDiagnostic()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    PersonalDetailsRepository repository(services.settingsService());

    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));
    const Result<QVariant> serviceResult = services.settingsService()->load(
        QString::fromUtf8(NameKey)
        );
    QVERIFY(!serviceResult);

    const RepositoryResult<QVariant> settingResult = repository.loadSetting(
        QString::fromUtf8(NameKey),
        QStringLiteral("not the fallback")
        );
    QVERIFY(!settingResult);
    QCOMPARE(settingResult.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(settingResult.error().diagnostic, serviceResult.error());

    const RepositoryResult<PersonalDetails> detailsResult = repository.load();
    QVERIFY(!detailsResult);
    QCOMPARE(detailsResult.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(detailsResult.error().diagnostic, serviceResult.error());
}

void PersonalDetailsRepositoryTests::unavailableServicesReturnTypedTechnicalErrors()
{
    ApplicationServices unavailableServices;
    PersonalDetailsRepository repository(unavailableServices.settingsService());
    QVERIFY(!repository.isAvailable());

    const RepositoryResult<PersonalDetails> loaded = repository.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(loaded.error().diagnostic, QString::fromUtf8(UnavailableDiagnostic));

    const RepositoryResult<QVariant> setting = repository.loadSetting(
        QStringLiteral("myInfo/name"),
        QStringLiteral("default")
        );
    QVERIFY(!setting);
    QCOMPARE(setting.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(setting.error().diagnostic, QString::fromUtf8(UnavailableDiagnostic));

    PersonalDetails details;
    const RepositoryStatus saved = repository.save(details);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(saved.error().diagnostic, QString::fromUtf8(UnavailableDiagnostic));

    const RepositoryStatus campusSaved = repository.saveCampus(QStringLiteral("campus"));
    QVERIFY(!campusSaved);
    QCOMPARE(campusSaved.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(
        campusSaved.error().diagnostic,
        QString::fromUtf8(UnavailableDiagnostic)
        );

    const RepositoryStatus settingSaved = repository.saveSetting(
        QStringLiteral("myInfo/name"),
        QStringLiteral("name")
        );
    QVERIFY(!settingSaved);
    QCOMPARE(settingSaved.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(
        settingSaved.error().diagnostic,
        QString::fromUtf8(UnavailableDiagnostic)
        );

    PersonalDetailsRepository nullRepository(nullptr);
    const RepositoryResult<QVariant> nullRead = nullRepository.loadSetting(
        QStringLiteral("myInfo/name"),
        QString()
        );
    QVERIFY(!nullRead);
    QCOMPARE(nullRead.error().code, RepositoryErrorCode::Technical);
    QCOMPARE(nullRead.error().diagnostic, QString::fromUtf8(UnavailableDiagnostic));
}

QTEST_GUILESS_MAIN(PersonalDetailsRepositoryTests)

#include "personal_details_repository_tests.moc"
