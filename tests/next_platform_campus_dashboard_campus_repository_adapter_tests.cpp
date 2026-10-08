#include "features/campus/data/campus_json_repository.h"
#include "next/platform/campus_dashboard_campus_repository_adapter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

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

}

class CampusDashboardCampusRepositoryAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void savesAndReloadsEveryCampusField();
    void savesStandaloneMainImageWithoutMapPaths();
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

    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );
    const Domain::Result<void> saved = adapter.saveCampus(expected.value());
    QVERIFY(saved);

    const auto loaded = CampusJsonRepository(m_directory.path())
        .loadCampus(QStringLiteral("alpha_campus"));
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

    Platform::CampusDashboardCampusRepositoryAdapter adapter(
        m_directory.path()
        );
    const Domain::Result<void> saved = adapter.saveCampus(expected.value());
    QVERIFY(saved);

    const auto loaded = CampusJsonRepository(m_directory.path())
        .loadCampus(QStringLiteral("standalone_main"));
    QVERIFY(loaded.has_value());

    const auto actual = Platform::CampusDashboardCampusRepositoryAdapter::
        snapshotFromCampusInfo(loaded.value());
    QVERIFY(actual.has_value());
    compareSnapshots(actual.value(), expected.value());
    QCOMPARE(actual->imageMain, std::string("standalone-campus-cover.png"));
    QVERIFY(actual->mapImagePaths.empty());
}

void CampusDashboardCampusRepositoryAdapterTests::
mapsRepositoryFailureToTechnicalWithExactMessage()
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

    const Status repositoryFailure = CampusJsonRepository(blocker.fileName())
        .saveCampus(source);
    QVERIFY(!repositoryFailure);

    const Domain::Result<void> mappedFailure =
        Platform::CampusDashboardCampusRepositoryAdapter(blocker.fileName())
            .saveCampus(snapshot.value());
    QVERIFY(!mappedFailure);
    QCOMPARE(mappedFailure.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        mappedFailure.error().message,
        repositoryFailure.error().toUtf8().toStdString()
        );
    QVERIFY(!mappedFailure.error().recoverable);
}

QTEST_APPLESS_MAIN(CampusDashboardCampusRepositoryAdapterTests)

#include "next_platform_campus_dashboard_campus_repository_adapter_tests.moc"
