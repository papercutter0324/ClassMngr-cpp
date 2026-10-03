#include "features/campus/data/campus_json_repository.h"
#include "features/campus/data/campus_json_codec.h"
#include "next/platform/campus_dashboard_selected_campus_read_adapter.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace ClassMngr::Next;

namespace
{

void writeJson(
    const CampusJsonRepository& repository,
    const QString& campusId,
    const QJsonObject& json
    )
{
    QFile file(repository.filePathForCampusId(campusId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qFatal("Unable to create campus test record.");
    }
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

CampusInfo fullCampusInfo()
{
    CampusInfo campus;
    campus.id = QStringLiteral("alpha_campus");
    campus.campusName = QStringLiteral("Alpha Campus");
    campus.campusCode = QStringLiteral("ALP");
    campus.buildingName = QStringLiteral("English Building");
    campus.buildingNameKr = QStringLiteral("Korean Building");
    campus.address = QStringLiteral("Hidden top-level address");
    campus.phoneNumber = QStringLiteral("02-1234-5678");
    campus.officeNumber = QStringLiteral("Room 101");
    campus.directionsAddressEn = QJsonObject{
        {QStringLiteral("city"), QStringLiteral("Seoul")},
        {QStringLiteral("line1"), QStringLiteral("English Street 1")},
        {QStringLiteral("modern"), QJsonObject{
             {QStringLiteral("province"), QStringLiteral("Seoul")},
             {QStringLiteral("line1"), QStringLiteral("Modern English 1")}
         }},
        {QStringLiteral("classic"), QJsonObject{
             {QStringLiteral("province"), QStringLiteral("Seoul")},
             {QStringLiteral("line1"), QStringLiteral("Classic English 1")}
         }}
    };
    campus.directionsAddressKr = QJsonObject{
        {QStringLiteral("city"), QStringLiteral("\uC11C\uC6B8")},
        {QStringLiteral("line1"), QStringLiteral("\uD55C\uAD6D\uC5B4 \uAC70\uB9AC 1")},
        {QStringLiteral("modern"), QJsonObject{
             {QStringLiteral("province"), QStringLiteral("\uC11C\uC6B8")},
             {QStringLiteral("line1"), QStringLiteral("\uD604\uB300 \uC8FC\uC18C")}
         }},
        {QStringLiteral("classic"), QJsonObject{
             {QStringLiteral("province"), QStringLiteral("\uC11C\uC6B8")},
             {QStringLiteral("line1"), QStringLiteral("\uAD6C \uC8FC\uC18C")}
         }}
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

}

class CampusDashboardSelectedCampusReadAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void mapsSelectedCampusProjection();
    void missingDefaultAndUnreadableRecordsStayAbsent();

private:
    QTemporaryDir m_directory;
};

void CampusDashboardSelectedCampusReadAdapterTests::init()
{
    QVERIFY(m_directory.isValid());
}

void CampusDashboardSelectedCampusReadAdapterTests::mapsSelectedCampusProjection()
{
    CampusJsonRepository repository(m_directory.path());
    QJsonObject record = CampusJsonCodec::toJson(fullCampusInfo());
    // image_main is hidden from the editable page but must survive detail reads
    // independently of the explicit map image collection.
    record.insert(
        QStringLiteral("image_main"),
        QStringLiteral("hidden-campus-cover.png")
        );
    writeJson(repository, QStringLiteral("alpha_campus"), record);

    Platform::CampusDashboardSelectedCampusReadAdapter adapter(
        m_directory.path()
        );
    const auto campusId = Domain::CampusId::fromString("alpha_campus");
    QVERIFY(campusId.has_value());

    const Application::CampusDashboardSelectedCampusReadResult result =
        adapter.loadCampus(campusId.value());
    QVERIFY(result);
    QVERIFY(result.value().has_value());

    const auto& campus = result.value().value();
    QCOMPARE(campus.id.value(), std::string("alpha_campus"));
    QCOMPARE(campus.campusName, std::string("Alpha Campus"));
    QCOMPARE(campus.campusCode, std::string("ALP"));
    QCOMPARE(campus.buildingName, std::string("English Building"));
    QCOMPARE(campus.buildingNameKr, std::string("Korean Building"));
    QCOMPARE(campus.address, std::string("Hidden top-level address"));
    QCOMPARE(campus.phoneNumber, std::string("02-1234-5678"));
    QCOMPARE(campus.officeNumber, std::string("Room 101"));
    QCOMPARE(campus.directionsNote, std::string("Use the east entrance"));
    QCOMPARE(campus.transitSteps.size(), std::size_t(2));
    QCOMPARE(campus.transitSteps.at(0), std::string("Take Line 2"));
    QCOMPARE(campus.transitSteps.at(1), std::string("Exit at Gate 3"));
    QCOMPARE(campus.arrivalInfo, std::string("Call on arrival"));
    QCOMPARE(campus.imageMain, std::string("hidden-campus-cover.png"));
    QCOMPARE(campus.mapImagePaths.size(), std::size_t(2));
    QCOMPARE(campus.mapImagePaths.at(0), std::string("maps/alpha-1.png"));
    QCOMPARE(campus.mapImagePaths.at(1), std::string("maps/alpha-2.png"));
    QCOMPARE(campus.naverMapUrl, std::string("https://naver.example/alpha"));
    QCOMPARE(campus.kakaoMapUrl, std::string("https://kakao.example/alpha"));
    QCOMPARE(campus.officeWifi, std::string("Campus WiFi"));
    QCOMPARE(campus.officeWifiPassword, std::string("secret-wifi"));
    QCOMPARE(campus.printerName, std::string("Office Printer"));
    QCOMPARE(campus.printerSteps, std::string("Install the driver"));
    QCOMPARE(campus.printerDriverUrl, std::string("https://driver.example"));
    QVERIFY(!campus.printerDriverUrlUnavailable);
    QCOMPARE(campus.photocopierCode, std::string("COPY-123"));

    const QJsonObject english = QJsonDocument::fromJson(
        QByteArray::fromStdString(campus.directionsAddressEnJson)
        ).object();
    QCOMPARE(english.value(QStringLiteral("city")).toString(), QStringLiteral("Seoul"));
    QCOMPARE(english.value(QStringLiteral("line1")).toString(), QStringLiteral("English Street 1"));
    QCOMPARE(
        english.value(QStringLiteral("modern")).toObject()
            .value(QStringLiteral("line1")).toString(),
        QStringLiteral("Modern English 1")
        );
    QCOMPARE(
        english.value(QStringLiteral("classic")).toObject()
            .value(QStringLiteral("line1")).toString(),
        QStringLiteral("Classic English 1")
        );

    const QJsonObject korean = QJsonDocument::fromJson(
        QByteArray::fromStdString(campus.directionsAddressKrJson)
        ).object();
    QCOMPARE(korean.value(QStringLiteral("city")).toString(), QStringLiteral("\uC11C\uC6B8"));
    QCOMPARE(korean.value(QStringLiteral("line1")).toString(), QStringLiteral("\uD55C\uAD6D\uC5B4 \uAC70\uB9AC 1"));
    QCOMPARE(
        korean.value(QStringLiteral("modern")).toObject()
            .value(QStringLiteral("line1")).toString(),
        QStringLiteral("\uD604\uB300 \uC8FC\uC18C")
        );
    QCOMPARE(
        korean.value(QStringLiteral("classic")).toObject()
            .value(QStringLiteral("line1")).toString(),
        QStringLiteral("\uAD6C \uC8FC\uC18C")
        );

    const QJsonArray housing = QJsonDocument::fromJson(
        QByteArray::fromStdString(campus.housingLocationsJson)
        ).array();
    QCOMPARE(housing.size(), 2);
    QCOMPARE(
        housing.at(0).toObject().value(QStringLiteral("custom_note")).toString(),
        QStringLiteral("Keep this")
        );
    QCOMPARE(
        housing.at(0).toObject().value(QStringLiteral("address")).toObject()
            .value(QStringLiteral("custom_field")).toString(),
        QStringLiteral("Value")
        );
}

void CampusDashboardSelectedCampusReadAdapterTests::
missingDefaultAndUnreadableRecordsStayAbsent()
{
    CampusJsonRepository repository(m_directory.path());
    writeJson(
        repository,
        QStringLiteral("malformed"),
        QJsonObject{{QStringLiteral("id"), QStringLiteral("malformed")}}
        );
    QFile malformedFile(repository.filePathForCampusId(QStringLiteral("malformed")));
    QVERIFY(malformedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    malformedFile.write("not-json");
    malformedFile.close();

    QVERIFY(QDir().mkpath(
        repository.filePathForCampusId(QStringLiteral("unreadable"))
        ));

    Platform::CampusDashboardSelectedCampusReadAdapter adapter(
        m_directory.path()
        );
    const auto read = [&adapter](const char* id)
    {
        const auto campusId = Domain::CampusId::fromString(id);
        Q_ASSERT(campusId.has_value());
        return adapter.loadCampus(campusId.value());
    };

    const auto missing = read("missing");
    const auto defaultCampus = read("default");
    const auto malformed = read("malformed");
    const auto unreadable = read("unreadable");

    QVERIFY(missing);
    QVERIFY(!missing.value().has_value());
    QVERIFY(defaultCampus);
    QVERIFY(!defaultCampus.value().has_value());
    QVERIFY(malformed);
    QVERIFY(!malformed.value().has_value());
    QVERIFY(unreadable);
    QVERIFY(!unreadable.value().has_value());
}

QTEST_APPLESS_MAIN(CampusDashboardSelectedCampusReadAdapterTests)

#include "next_platform_campus_dashboard_selected_campus_read_adapter_tests.moc"
