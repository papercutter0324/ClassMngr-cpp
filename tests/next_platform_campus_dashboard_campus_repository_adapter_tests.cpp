#include "features/campus/data/campus_json_repository.h"
#include "next/platform/campus_dashboard_campus_repository_adapter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <string>

using namespace ClassMngr::Next;

namespace
{

CampusInfo representativeCampus()
{
    CampusInfo campus;
    campus.id = QStringLiteral("alpha_campus");
    campus.campusName = QStringLiteral("Alpha Campus");
    campus.campusCode = QStringLiteral("ALP");
    campus.buildingName = QStringLiteral("English Building");
    campus.buildingNameKr = QStringLiteral("Korean Building");
    campus.address = QStringLiteral("Legacy top-level address");
    campus.phoneNumber = QStringLiteral("02-1234-5678");
    campus.officeNumber = QStringLiteral("Room 101");
    campus.directionsAddressEn = QJsonObject{
        {QStringLiteral("building_name"), campus.buildingName},
        {QStringLiteral("province"), QStringLiteral("Seoul"),},
        {QStringLiteral("city"), QStringLiteral("Seoul")},
        {QStringLiteral("district"), QStringLiteral("Jongno")},
        {QStringLiteral("city_district"), QStringLiteral("Seoul Jongno")},
        {QStringLiteral("line1"), QStringLiteral("English Street 1")},
        {QStringLiteral("line2"), QStringLiteral("Suite 2")},
        {QStringLiteral("postal_code"), QStringLiteral("03000")},
        {QStringLiteral("addr_note"), QString()},
        {QStringLiteral("modern"), QJsonObject{
             {QStringLiteral("building_name"), QStringLiteral("Modern Hall")},
             {QStringLiteral("province"), QStringLiteral("Seoul")},
             {QStringLiteral("city"), QStringLiteral("Seoul")},
             {QStringLiteral("district"), QStringLiteral("Jongno")},
             {QStringLiteral("city_district"), QStringLiteral("Seoul Jongno")},
             {QStringLiteral("line1"), QStringLiteral("Modern Street 1")},
             {QStringLiteral("line2"), QStringLiteral("Suite 3")},
             {QStringLiteral("postal_code"), QStringLiteral("03001")},
             {QStringLiteral("addr_note"), QString()}
         }},
        {QStringLiteral("classic"), QJsonObject{
             {QStringLiteral("building_name"), QStringLiteral("Old Hall")},
             {QStringLiteral("province"), QStringLiteral("Seoul")},
             {QStringLiteral("city"), QStringLiteral("Seoul")},
             {QStringLiteral("district"), QStringLiteral("Jongno")},
             {QStringLiteral("city_district"), QStringLiteral("Seoul Jongno")},
             {QStringLiteral("line1"), QStringLiteral("Classic Street 1")},
             {QStringLiteral("line2"), QStringLiteral("Suite 4")},
             {QStringLiteral("postal_code"), QStringLiteral("03002")},
             {QStringLiteral("addr_note"), QString()}
         }}
    };
    campus.directionsAddressKr = QJsonObject{
        {QStringLiteral("building_name"), campus.buildingNameKr},
        {QStringLiteral("province"), QStringLiteral("서울특별시")},
        {QStringLiteral("city"), QStringLiteral("서울")},
        {QStringLiteral("district"), QStringLiteral("종로구")},
        {QStringLiteral("city_district"), QStringLiteral("서울 종로구")},
        {QStringLiteral("line1"), QStringLiteral("한국어 거리 1")},
        {QStringLiteral("line2"), QStringLiteral("2층")},
        {QStringLiteral("postal_code"), QStringLiteral("03003")},
        {QStringLiteral("addr_note"), QString()}
    };
    campus.directionsNote = QStringLiteral("Use the east entrance");
    campus.transitSteps = {
        QStringLiteral("Take Line 2"),
        QStringLiteral("Exit at Gate 3")
    };
    campus.arrivalInfo = QStringLiteral("Call on arrival");
    campus.imageMain = QStringLiteral("hidden-campus-cover.png");
    campus.mapImagePaths = {
        QStringLiteral("maps/alpha-1.png"),
        QStringLiteral("maps/alpha-2.png")
    };
    campus.naverMapUrl = QStringLiteral("https://naver.example/alpha");
    campus.kakaoMapUrl = QStringLiteral("https://kakao.example/alpha");
    campus.officeWifi = QStringLiteral("Campus WiFi");
    campus.officeWifiPassword = QStringLiteral("secret-wifi");
    campus.printerName = QStringLiteral("Office Printer");
    campus.printerSteps = QStringLiteral("Install the driver");
    campus.printerDriverUrl = QStringLiteral("https://driver.example");
    campus.printerDriverUrlUnavailable = false;
    campus.photocopierCode = QStringLiteral("COPY-123");
    campus.housingLocations = QJsonArray{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Student House")},
            {QStringLiteral("custom_note"), QStringLiteral("Keep this")},
            {QStringLiteral("address"), QJsonObject{
                 {QStringLiteral("line1"), QStringLiteral("Housing Road")},
                 {QStringLiteral("custom_field"), QStringLiteral("Value")}
             }}
        },
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Second House")}
        }
    };
    return campus;
}

void compareSnapshots(
    const Application::CampusDashboardCampusSnapshot& actual,
    const Application::CampusDashboardCampusSnapshot& expected
    )
{
    QCOMPARE(actual.id.value(), expected.id.value());
    QCOMPARE(actual.campusName, expected.campusName);
    QCOMPARE(actual.campusCode, expected.campusCode);
    QCOMPARE(actual.buildingName, expected.buildingName);
    QCOMPARE(actual.buildingNameKr, expected.buildingNameKr);
    QCOMPARE(actual.address, expected.address);
    QCOMPARE(actual.phoneNumber, expected.phoneNumber);
    QCOMPARE(actual.officeNumber, expected.officeNumber);
    QCOMPARE(actual.directionsAddressEnJson, expected.directionsAddressEnJson);
    QCOMPARE(actual.directionsAddressKrJson, expected.directionsAddressKrJson);
    QCOMPARE(actual.directionsNote, expected.directionsNote);
    QCOMPARE(actual.transitSteps, expected.transitSteps);
    QCOMPARE(actual.arrivalInfo, expected.arrivalInfo);
    QCOMPARE(actual.imageMain, expected.imageMain);
    QCOMPARE(actual.mapImagePaths, expected.mapImagePaths);
    QCOMPARE(actual.naverMapUrl, expected.naverMapUrl);
    QCOMPARE(actual.kakaoMapUrl, expected.kakaoMapUrl);
    QCOMPARE(actual.officeWifi, expected.officeWifi);
    QCOMPARE(actual.officeWifiPassword, expected.officeWifiPassword);
    QCOMPARE(actual.printerName, expected.printerName);
    QCOMPARE(actual.printerSteps, expected.printerSteps);
    QCOMPARE(actual.printerDriverUrl, expected.printerDriverUrl);
    QCOMPARE(
        actual.printerDriverUrlUnavailable,
        expected.printerDriverUrlUnavailable
        );
    QCOMPARE(actual.photocopierCode, expected.photocopierCode);
    QCOMPARE(actual.housingLocationsJson, expected.housingLocationsJson);
}

std::optional<CampusInfo> loadCampusOrFail(
    const CampusJsonRepository& repository,
    const QString& campusId
    )
{
    const RepositoryResult<std::optional<CampusInfo>> loaded =
        repository.loadCampus(campusId);
    if (!loaded)
    {
        qFatal("Unable to read campus repository test data.");
    }
    return loaded.value();
}

}

class CampusDashboardCampusRepositoryAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void savesAndReloadsEveryCampusField();
    void savesStandaloneMainImageWithoutMapPaths();
    void missingUpdateTargetReturnsRecoverableNotFoundWithoutCreatingFile();
    void rejectsReservedCreateIdentityBeforeWritingDefaultFile();
    void rejectsReservedUpdateNameWithoutChangingExistingFile();
    void rejectsLocalizedFallbackOriginBeforeCreateOrUpdateWrites();
    void rejectsInvalidJsonBeforeCreateOrUpdateWrites();
    void acceptsValidEmptyJsonRootsForCreateAndUpdate();
    void createCollisionPreservesExistingNormalizedDestination();
    void createThenUpdateRetainsTheCreatedId();
    void mapsCreateRepositoryFailureToTechnicalWithExactMessage();
    void mapsRepositoryFailureToTechnicalWithExactMessage();

private:
    QTemporaryDir m_directory;
};

void CampusDashboardCampusRepositoryAdapterTests::init()
{
    QVERIFY(m_directory.isValid());
}

void CampusDashboardCampusRepositoryAdapterTests::
savesAndReloadsEveryCampusField()
{
    const CampusInfo source = representativeCampus();
    const auto expected = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(source);
    QVERIFY(expected.has_value());
    auto expectedSnapshot = expected.value();
    expectedSnapshot.imageMain = expectedSnapshot.mapImagePaths.front();
    QVERIFY(CampusJsonRepository(m_directory.path()).saveCampus(source));

    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );
    const Domain::Result<void> saved = adapter.saveCampus(
        expected.value(),
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(saved);

    const auto loaded = loadCampusOrFail(
        CampusJsonRepository(m_directory.path()),
        QStringLiteral("alpha_campus")
        );
    QVERIFY(loaded.has_value());

    const auto actual = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(loaded.value());
    QVERIFY(actual.has_value());
    compareSnapshots(actual.value(), expectedSnapshot);
}

void CampusDashboardCampusRepositoryAdapterTests::
savesStandaloneMainImageWithoutMapPaths()
{
    CampusInfo source = representativeCampus();
    source.id = QStringLiteral("standalone_main");
    source.imageMain = QStringLiteral("standalone-campus-cover.png");
    source.mapImagePaths.clear();

    const auto expected = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(source);
    QVERIFY(expected.has_value());
    QVERIFY(CampusJsonRepository(m_directory.path()).saveCampus(source));

    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );
    const Domain::Result<void> saved = adapter.saveCampus(
        expected.value(),
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(saved);

    const auto loaded = loadCampusOrFail(
        CampusJsonRepository(m_directory.path()),
        QStringLiteral("standalone_main")
        );
    QVERIFY(loaded.has_value());

    const auto actual = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(loaded.value());
    QVERIFY(actual.has_value());
    compareSnapshots(actual.value(), expected.value());
    QCOMPARE(actual->imageMain, std::string("standalone-campus-cover.png"));
    QVERIFY(actual->mapImagePaths.empty());
}

void CampusDashboardCampusRepositoryAdapterTests::
missingUpdateTargetReturnsRecoverableNotFoundWithoutCreatingFile()
{
    CampusInfo source = representativeCampus();
    source.id = QStringLiteral("missing_update");
    const auto snapshot = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(source);
    QVERIFY(snapshot.has_value());

    const CampusJsonRepository repository(m_directory.path());
    const QString missingPath = repository.filePathForCampusId(source.id);
    QVERIFY(!QFile::exists(missingPath));

    const Domain::Result<void> updated =
        Platform::CampusDashboardCampusRepositoryAdapter(m_directory.path())
            .saveCampus(
                snapshot.value(),
                Application::CampusDashboardCampusSaveIntent::Update
                );

    QVERIFY(!updated);
    QCOMPARE(updated.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(updated.error().recoverable);
    QCOMPARE(
        updated.error().message,
        std::string("This campus no longer exists. Reload it before saving.")
        );
    QVERIFY(!QFile::exists(missingPath));
    QVERIFY(!loadCampusOrFail(repository, source.id).has_value());
}

void CampusDashboardCampusRepositoryAdapterTests::
rejectsReservedCreateIdentityBeforeWritingDefaultFile()
{
    CampusInfo source = representativeCampus();
    source.id = QStringLiteral("default");
    source.campusName = QStringLiteral("Default");
    const auto snapshot = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(source);
    QVERIFY(snapshot.has_value());
    auto invalidJsonSnapshot = snapshot.value();
    invalidJsonSnapshot.directionsAddressEnJson = "{malformed";

    const CampusJsonRepository repository(m_directory.path());
    const QString reservedPath = repository.filePathForCampusId(
        QStringLiteral("default")
        );
    QVERIFY(!QFile::exists(reservedPath));

    const Domain::Result<void> created =
        Platform::CampusDashboardCampusRepositoryAdapter(m_directory.path())
            .saveCampus(
                invalidJsonSnapshot,
                Application::CampusDashboardCampusSaveIntent::Create
                );

    QVERIFY(!created);
    QCOMPARE(created.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!created.error().recoverable);
    QVERIFY(
        created.error().message.find("Choose a different campus name")
        != std::string::npos
        );
    QVERIFY(!QFile::exists(reservedPath));
    QVERIFY(!loadCampusOrFail(
        repository,
        QStringLiteral("default")
        ).has_value());
}

void CampusDashboardCampusRepositoryAdapterTests::
rejectsReservedUpdateNameWithoutChangingExistingFile()
{
    CampusInfo existing = representativeCampus();
    existing.id = QStringLiteral("alpha");
    existing.campusName = QStringLiteral("Alpha");
    CampusJsonRepository repository(m_directory.path());
    const RepositoryStatus seeded = repository.saveCampus(existing);
    QVERIFY(seeded);

    const QString existingPath = repository.filePathForCampusId(
        QStringLiteral("alpha")
        );
    QFile originalFile(existingPath);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray before = originalFile.readAll();
    originalFile.close();

    existing.campusName = QStringLiteral("Default");
    const auto snapshot = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(existing);
    QVERIFY(snapshot.has_value());
    auto invalidJsonSnapshot = snapshot.value();
    invalidJsonSnapshot.housingLocationsJson = "{}";

    const Domain::Result<void> updated =
        Platform::CampusDashboardCampusRepositoryAdapter(m_directory.path())
            .saveCampus(
                invalidJsonSnapshot,
                Application::CampusDashboardCampusSaveIntent::Update
                );

    QVERIFY(!updated);
    QCOMPARE(updated.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!updated.error().recoverable);
    QVERIFY(
        updated.error().message.find("Choose a different campus name")
        != std::string::npos
        );

    QFile unchangedFile(existingPath);
    QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
    QCOMPARE(unchangedFile.readAll(), before);
    const auto stillAlpha = loadCampusOrFail(
        repository,
        QStringLiteral("alpha")
        );
    QVERIFY(stillAlpha.has_value());
    QCOMPARE(stillAlpha->campusName, QStringLiteral("Alpha"));
}

void CampusDashboardCampusRepositoryAdapterTests::
rejectsLocalizedFallbackOriginBeforeCreateOrUpdateWrites()
{
    const Application::CampusDashboardCampusNameOrigin fallbackOrigin =
        Application::CampusDashboardCampusNameOrigin::
            LocalizedDefaultFallback;

    CampusInfo newCampus = representativeCampus();
    newCampus.id = QStringLiteral("campus");
    newCampus.campusName = QStringLiteral("기본값");
    const auto createSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(newCampus);
    QVERIFY(createSnapshot.has_value());

    const CampusJsonRepository repository(m_directory.path());
    const QString createPath = repository.filePathForCampusId(
        QStringLiteral("campus")
        );
    QVERIFY(!QFile::exists(createPath));

    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );
    const Domain::Result<void> created = adapter.saveCampus(
        createSnapshot.value(),
        Application::CampusDashboardCampusSaveIntent::Create,
        fallbackOrigin
        );
    QVERIFY(!created);
    QCOMPARE(created.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!created.error().recoverable);
    QVERIFY(
        created.error().message.find("reserved Default identity")
        != std::string::npos
        );
    QVERIFY(!QFile::exists(createPath));

    CampusInfo existing = representativeCampus();
    existing.id = QStringLiteral("alpha");
    existing.campusName = QStringLiteral("Alpha");
    QVERIFY(repository.saveCampus(existing));
    const QString existingPath = repository.filePathForCampusId(
        QStringLiteral("alpha")
        );
    QFile originalFile(existingPath);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray before = originalFile.readAll();
    originalFile.close();

    existing.campusName = QStringLiteral("기본값");
    const auto updateSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(existing);
    QVERIFY(updateSnapshot.has_value());
    const Domain::Result<void> updated = adapter.saveCampus(
        updateSnapshot.value(),
        Application::CampusDashboardCampusSaveIntent::Update,
        fallbackOrigin
        );
    QVERIFY(!updated);
    QCOMPARE(updated.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!updated.error().recoverable);

    QFile unchangedFile(existingPath);
    QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
    QCOMPARE(unchangedFile.readAll(), before);
}

void CampusDashboardCampusRepositoryAdapterTests::
rejectsInvalidJsonBeforeCreateOrUpdateWrites()
{
    enum class JsonField
    {
        DirectionsEn,
        DirectionsKr,
        HousingLocations
    };
    struct InvalidJsonCase
    {
        JsonField field;
        const char* value;
        const char* expectedMessage;
    };
    const std::array invalidCases{
        InvalidJsonCase{
            JsonField::DirectionsEn,
            "{malformed",
            "directionsAddressEnJson must be a valid JSON object."
        },
        InvalidJsonCase{
            JsonField::DirectionsEn,
            "[]",
            "directionsAddressEnJson must be a valid JSON object."
        },
        InvalidJsonCase{
            JsonField::DirectionsKr,
            "{malformed",
            "directionsAddressKrJson must be a valid JSON object."
        },
        InvalidJsonCase{
            JsonField::DirectionsKr,
            "[]",
            "directionsAddressKrJson must be a valid JSON object."
        },
        InvalidJsonCase{
            JsonField::HousingLocations,
            "[malformed",
            "housingLocationsJson must be a valid JSON array."
        },
        InvalidJsonCase{
            JsonField::HousingLocations,
            "{}",
            "housingLocationsJson must be a valid JSON array."
        }
    };
    const auto setInvalidJson = [](
        Application::CampusDashboardCampusSnapshot& snapshot,
        const InvalidJsonCase& invalidCase
        )
    {
        switch (invalidCase.field)
        {
        case JsonField::DirectionsEn:
            snapshot.directionsAddressEnJson = invalidCase.value;
            break;
        case JsonField::DirectionsKr:
            snapshot.directionsAddressKrJson = invalidCase.value;
            break;
        case JsonField::HousingLocations:
            snapshot.housingLocationsJson = invalidCase.value;
            break;
        }
    };

    CampusJsonRepository repository(m_directory.path());
    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );

    CampusInfo createSource = representativeCampus();
    createSource.id = QStringLiteral("invalid_json_create");
    createSource.campusName = QStringLiteral("Invalid JSON Create");
    const auto createSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(createSource);
    QVERIFY(createSnapshot.has_value());
    const QString createPath = repository.filePathForCampusId(
        createSource.id
        );
    QVERIFY(!QFile::exists(createPath));

    for (const InvalidJsonCase& invalidCase : invalidCases)
    {
        auto candidate = createSnapshot.value();
        setInvalidJson(candidate, invalidCase);
        const Domain::Result<void> created = adapter.saveCampus(
            candidate,
            Application::CampusDashboardCampusSaveIntent::Create
            );
        QVERIFY2(!created, invalidCase.expectedMessage);
        QCOMPARE(created.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            created.error().message,
            std::string(invalidCase.expectedMessage)
            );
        QVERIFY(!created.error().recoverable);
        QVERIFY(!QFile::exists(createPath));
    }

    CampusInfo updateSource = representativeCampus();
    updateSource.id = QStringLiteral("invalid_json_update");
    updateSource.campusName = QStringLiteral("Invalid JSON Update");
    const RepositoryStatus seeded = repository.saveCampus(updateSource);
    QVERIFY(seeded);

    QFile originalFile(repository.filePathForCampusId(updateSource.id));
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = originalFile.readAll();
    originalFile.close();

    const auto updateSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(updateSource);
    QVERIFY(updateSnapshot.has_value());
    const QString updatePath = repository.filePathForCampusId(
        updateSource.id
        );

    for (const InvalidJsonCase& invalidCase : invalidCases)
    {
        auto candidate = updateSnapshot.value();
        setInvalidJson(candidate, invalidCase);
        const Domain::Result<void> updated = adapter.saveCampus(
            candidate,
            Application::CampusDashboardCampusSaveIntent::Update
            );
        QVERIFY2(!updated, invalidCase.expectedMessage);
        QCOMPARE(updated.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            updated.error().message,
            std::string(invalidCase.expectedMessage)
            );
        QVERIFY(!updated.error().recoverable);

        QFile unchangedFile(updatePath);
        QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedFile.readAll(), originalBytes);
    }
}

void CampusDashboardCampusRepositoryAdapterTests::
acceptsValidEmptyJsonRootsForCreateAndUpdate()
{
    CampusJsonRepository repository(m_directory.path());
    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );

    CampusInfo createSource = representativeCampus();
    createSource.id = QStringLiteral("empty_json_create");
    createSource.campusName = QStringLiteral("Empty JSON Create");
    const auto createSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(createSource);
    QVERIFY(createSnapshot.has_value());
    auto emptyCreateSnapshot = createSnapshot.value();
    emptyCreateSnapshot.directionsAddressEnJson = "{}";
    emptyCreateSnapshot.directionsAddressKrJson = "{}";
    emptyCreateSnapshot.housingLocationsJson = "[]";

    QTemporaryDir createBaselineDirectory;
    QVERIFY(createBaselineDirectory.isValid());
    CampusJsonRepository createBaselineRepository(
        createBaselineDirectory.path()
        );
    const CampusInfo expectedCreateInput =
        Platform::CampusDashboardCampusRepositoryAdapter::
            campusInfoFromSnapshot(emptyCreateSnapshot);
    const auto baselineCreated =
        createBaselineRepository.createCampus(expectedCreateInput);
    QVERIFY(baselineCreated.has_value());
    const auto expectedCreatedCampus = loadCampusOrFail(
        createBaselineRepository,
        createSource.id
        );
    QVERIFY(expectedCreatedCampus.has_value());

    const Domain::Result<void> created = adapter.saveCampus(
        emptyCreateSnapshot,
        Application::CampusDashboardCampusSaveIntent::Create
        );
    QVERIFY(created);
    const auto createdCampus = loadCampusOrFail(repository, createSource.id);
    QVERIFY(createdCampus.has_value());
    QVERIFY(
        createdCampus->directionsAddressEn
        == expectedCreatedCampus->directionsAddressEn
        );
    QVERIFY(
        createdCampus->directionsAddressKr
        == expectedCreatedCampus->directionsAddressKr
        );
    QVERIFY(createdCampus->housingLocations.isEmpty());
    QVERIFY(QFile::exists(repository.filePathForCampusId(createSource.id)));

    CampusInfo updateSource = representativeCampus();
    updateSource.id = QStringLiteral("empty_json_update");
    updateSource.campusName = QStringLiteral("Empty JSON Update");
    QVERIFY(repository.saveCampus(updateSource));

    QTemporaryDir updateBaselineDirectory;
    QVERIFY(updateBaselineDirectory.isValid());
    CampusJsonRepository updateBaselineRepository(
        updateBaselineDirectory.path()
        );
    QVERIFY(updateBaselineRepository.saveCampus(updateSource));

    const auto updateSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(updateSource);
    QVERIFY(updateSnapshot.has_value());
    auto emptyUpdateSnapshot = updateSnapshot.value();
    emptyUpdateSnapshot.campusName = "Updated Empty JSON Campus";
    emptyUpdateSnapshot.directionsAddressEnJson = "{}";
    emptyUpdateSnapshot.directionsAddressKrJson = "{}";
    emptyUpdateSnapshot.housingLocationsJson = "[]";

    const CampusInfo expectedUpdateInput =
        Platform::CampusDashboardCampusRepositoryAdapter::
            campusInfoFromSnapshot(emptyUpdateSnapshot);
    QVERIFY(updateBaselineRepository.saveCampus(expectedUpdateInput));
    const auto expectedUpdatedCampus = loadCampusOrFail(
        updateBaselineRepository,
        updateSource.id
        );
    QVERIFY(expectedUpdatedCampus.has_value());

    const Domain::Result<void> updated = adapter.saveCampus(
        emptyUpdateSnapshot,
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(updated);
    const auto updatedCampus = loadCampusOrFail(repository, updateSource.id);
    QVERIFY(updatedCampus.has_value());
    QVERIFY(
        updatedCampus->directionsAddressEn
        == expectedUpdatedCampus->directionsAddressEn
        );
    QVERIFY(
        updatedCampus->directionsAddressKr
        == expectedUpdatedCampus->directionsAddressKr
        );
    QVERIFY(updatedCampus->housingLocations.isEmpty());
    QCOMPARE(
        updatedCampus->campusName,
        QStringLiteral("Updated Empty JSON Campus")
        );
}

void CampusDashboardCampusRepositoryAdapterTests::
createCollisionPreservesExistingNormalizedDestination()
{
    CampusInfo existing = representativeCampus();
    existing.id = QStringLiteral("north_west");
    existing.campusName = QStringLiteral("North-West");
    existing.campusCode = QStringLiteral("SENTINEL-CODE");
    existing.buildingName = QStringLiteral("Sentinel Building");
    existing.address = QStringLiteral("Sentinel Address");

    CampusJsonRepository repository(m_directory.path());
    const RepositoryStatus seeded = repository.saveCampus(existing);
    QVERIFY(seeded);

    const QString existingPath = repository.filePathForCampusId(existing.id);
    QFile existingFile(existingPath);
    QVERIFY(existingFile.open(QIODevice::ReadOnly));
    const QByteArray before = existingFile.readAll();
    existingFile.close();

    CampusInfo duplicate = representativeCampus();
    duplicate.id = QStringLiteral("North West");
    duplicate.campusName = QStringLiteral("North West");
    duplicate.campusCode = QStringLiteral("DUPLICATE-CODE");
    duplicate.buildingName = QStringLiteral("Duplicate Building");
    const auto snapshot = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(duplicate);
    QVERIFY(snapshot.has_value());

    const Domain::Result<void> created =
        Platform::CampusDashboardCampusRepositoryAdapter(m_directory.path())
            .saveCampus(
                snapshot.value(),
                Application::CampusDashboardCampusSaveIntent::Create
                );
    QVERIFY(!created);
    QCOMPARE(created.error().code, Domain::ErrorCode::Conflict);
    QVERIFY(!created.error().recoverable);

    QFile unchangedFile(existingPath);
    QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
    QCOMPARE(unchangedFile.readAll(), before);
}

void CampusDashboardCampusRepositoryAdapterTests::
createThenUpdateRetainsTheCreatedId()
{
    CampusInfo source = representativeCampus();
    source.id = QStringLiteral("unused_slug");
    source.campusName = QStringLiteral("Unused Slug");
    source.buildingName = QStringLiteral("Original Building");
    const auto createSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(source);
    QVERIFY(createSnapshot.has_value());

    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );
    const Domain::Result<void> created = adapter.saveCampus(
        createSnapshot.value(),
        Application::CampusDashboardCampusSaveIntent::Create
        );
    QVERIFY(created);

    source.campusName = QStringLiteral("Renamed Display Name");
    source.buildingName = QStringLiteral("Edited Building");
    const auto updateSnapshot =
        Platform::CampusDashboardCampusRepositoryAdapter::
            snapshotFromCampusInfo(source);
    QVERIFY(updateSnapshot.has_value());
    const Domain::Result<void> updated = adapter.saveCampus(
        updateSnapshot.value(),
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(updated);

    const CampusJsonRepository repository(m_directory.path());
    const auto saved = loadCampusOrFail(
        repository,
        QStringLiteral("unused_slug")
        );
    QVERIFY(saved.has_value());
    QCOMPARE(saved->id, QStringLiteral("unused_slug"));
    QCOMPARE(saved->campusName, QStringLiteral("Renamed Display Name"));
    QCOMPARE(saved->buildingName, QStringLiteral("Edited Building"));
    QVERIFY(!QFile::exists(
        repository.filePathForCampusId(QStringLiteral("renamed_display_name"))
        ));
}

void CampusDashboardCampusRepositoryAdapterTests::
mapsCreateRepositoryFailureToTechnicalWithExactMessage()
{
    QTemporaryDir blockerDirectory;
    QVERIFY(blockerDirectory.isValid());

    QFile blocker(blockerDirectory.filePath(QStringLiteral("not-a-directory")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("file blocks directory creation");
    blocker.close();

    const CampusInfo source = representativeCampus();
    const auto snapshot = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(source);
    QVERIFY(snapshot.has_value());

    const RepositoryStatus repositoryFailure =
        CampusJsonRepository(blocker.fileName()).createCampus(source);
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, RepositoryErrorCode::Technical);

    const Domain::Result<void> mappedFailure =
        Platform::CampusDashboardCampusRepositoryAdapter(blocker.fileName())
            .saveCampus(
                snapshot.value(),
                Application::CampusDashboardCampusSaveIntent::Create
                );
    QVERIFY(!mappedFailure);
    QCOMPARE(mappedFailure.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        mappedFailure.error().message,
        repositoryFailure.error().diagnostic.toUtf8().toStdString()
        );
    QVERIFY(!mappedFailure.error().recoverable);
}

void CampusDashboardCampusRepositoryAdapterTests::
mapsRepositoryFailureToTechnicalWithExactMessage()
{
    CampusInfo source = representativeCampus();
    source.id = QStringLiteral("repository_failure");
    const auto snapshot = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(source);
    QVERIFY(snapshot.has_value());

    const QString targetFileName =
        CampusJsonRepository::idFromName(source.id) + QStringLiteral(".json");
    QVERIFY(QDir(m_directory.path()).mkdir(targetFileName));

    const RepositoryStatus repositoryFailure =
        CampusJsonRepository(m_directory.path()).saveCampus(source);
    QVERIFY(!repositoryFailure);

    const Domain::Result<void> mappedFailure =
        Platform::CampusDashboardCampusRepositoryAdapter(m_directory.path())
            .saveCampus(
                snapshot.value(),
                Application::CampusDashboardCampusSaveIntent::Update
                );
    QVERIFY(!mappedFailure);
    QCOMPARE(mappedFailure.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        mappedFailure.error().message,
        repositoryFailure.error().diagnostic.toUtf8().toStdString()
        );
    QVERIFY(!mappedFailure.error().recoverable);
}

QTEST_APPLESS_MAIN(CampusDashboardCampusRepositoryAdapterTests)

#include "next_platform_campus_dashboard_campus_repository_adapter_tests.moc"
