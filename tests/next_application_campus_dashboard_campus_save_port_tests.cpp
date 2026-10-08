#include "next/application/campus_dashboard_campus_save_port.h"

#include <cassert>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

class RecordingCampusSavePort final
    : public Application::CampusDashboardCampusSavePort
{
public:
    mutable int callCount = 0;
    mutable std::optional<Application::CampusDashboardCampusSnapshot> saved;

    [[nodiscard]] Domain::Result<void> saveCampus(
        const Application::CampusDashboardCampusSnapshot& campus
        ) const override
    {
        ++callCount;
        saved = campus;
        return Domain::Result<void>::success();
    }
};

void snapshotOwnsTheTypedSaveRequest()
{
    using Snapshot = Application::CampusDashboardCampusSnapshot;

    static_assert(std::is_same_v<decltype(Snapshot::campusName), std::string>);
    static_assert(
        std::is_same_v<
            decltype(Snapshot::transitSteps),
            std::vector<std::string>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(Snapshot::printerDriverUrlUnavailable),
            bool
            >
        );

    Snapshot snapshot{
        .id = *Domain::CampusId::fromString("alpha"),
        .campusName = "Alpha Campus",
        .campusCode = "ALP",
        .buildingName = "English Building",
        .buildingNameKr = "Korean Building",
        .address = "Campus Road",
        .phoneNumber = "02-1234-5678",
        .officeNumber = "Room 101",
        .directionsAddressEnJson = R"({"city":"Seoul"})",
        .directionsAddressKrJson = R"({"city":"Seoul"})",
        .directionsNote = "Use the east entrance",
        .transitSteps = {"Line 2", "Gate 3"},
        .arrivalInfo = "Call on arrival",
        .imageMain = "campus-cover.png",
        .mapImagePaths = {"map-1.png", "map-2.png"},
        .naverMapUrl = "https://naver.example/alpha",
        .kakaoMapUrl = "https://kakao.example/alpha",
        .officeWifi = "Campus WiFi",
        .officeWifiPassword = "secret",
        .printerName = "Office Printer",
        .printerSteps = "Install the driver",
        .printerDriverUrl = "https://driver.example",
        .printerDriverUrlUnavailable = false,
        .photocopierCode = "COPY-123",
        .housingLocationsJson = R"([{"name":"Student House"}])"
    };

    RecordingCampusSavePort port;
    const Domain::Result<void> result = port.saveCampus(snapshot);

    assert(result);
    assert(port.callCount == 1);
    assert(port.saved.has_value());
    assert(port.saved->id == snapshot.id);
    assert(port.saved->campusName == snapshot.campusName);
    assert(port.saved->campusCode == snapshot.campusCode);
    assert(port.saved->buildingName == snapshot.buildingName);
    assert(port.saved->buildingNameKr == snapshot.buildingNameKr);
    assert(port.saved->address == snapshot.address);
    assert(port.saved->phoneNumber == snapshot.phoneNumber);
    assert(port.saved->officeNumber == snapshot.officeNumber);
    assert(port.saved->directionsAddressEnJson == snapshot.directionsAddressEnJson);
    assert(port.saved->directionsAddressKrJson == snapshot.directionsAddressKrJson);
    assert(port.saved->directionsNote == snapshot.directionsNote);
    assert(port.saved->transitSteps == snapshot.transitSteps);
    assert(port.saved->arrivalInfo == snapshot.arrivalInfo);
    assert(port.saved->imageMain == snapshot.imageMain);
    assert(port.saved->mapImagePaths == snapshot.mapImagePaths);
    assert(port.saved->naverMapUrl == snapshot.naverMapUrl);
    assert(port.saved->kakaoMapUrl == snapshot.kakaoMapUrl);
    assert(port.saved->officeWifi == snapshot.officeWifi);
    assert(port.saved->officeWifiPassword == snapshot.officeWifiPassword);
    assert(port.saved->printerName == snapshot.printerName);
    assert(port.saved->printerSteps == snapshot.printerSteps);
    assert(port.saved->printerDriverUrl == snapshot.printerDriverUrl);
    assert(
        port.saved->printerDriverUrlUnavailable
        == snapshot.printerDriverUrlUnavailable
        );
    assert(port.saved->photocopierCode == snapshot.photocopierCode);
    assert(port.saved->housingLocationsJson == snapshot.housingLocationsJson);
}

}

int main()
{
    snapshotOwnsTheTypedSaveRequest();
}
