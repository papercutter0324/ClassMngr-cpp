#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Owning UTF-8 Campus Dashboard value shared by selected-campus reads and
// writes. The JSON strings are limited to extensible address objects and the
// housing array; the remaining fields stay explicit.
struct CampusDashboardCampusSnapshot final
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

} // namespace ClassMngr::Next::Application
