#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// A selected-campus detail projection for the Campus Dashboard. Strings are
// owning UTF-8 values. The three JSON strings are narrowly scoped to the
// extensible directions-address objects and housing array; all other fields
// remain explicit so this contract does not retain a complete campus record.
struct CampusDashboardSelectedCampusSnapshot final
{
    Domain::CampusId id;
    std::string campusName;
    std::string campusCode;
    std::string buildingName;
    std::string buildingNameKr;
    std::string address;
    std::string phoneNumber;
    std::string officeNumber;
    std::string directionsAddressEnJson;
    std::string directionsAddressKrJson;
    std::string directionsNote;
    std::vector<std::string> transitSteps;
    std::string arrivalInfo;
    std::string imageMain;
    std::vector<std::string> mapImagePaths;
    std::string naverMapUrl;
    std::string kakaoMapUrl;
    std::string officeWifi;
    std::string officeWifiPassword;
    std::string printerName;
    std::string printerSteps;
    std::string printerDriverUrl;
    bool printerDriverUrlUnavailable = true;
    std::string photocopierCode;
    std::string housingLocationsJson;
};

using CampusDashboardSelectedCampusReadResult =
    Domain::Result<std::optional<CampusDashboardSelectedCampusSnapshot>>;

} // namespace ClassMngr::Next::Application
