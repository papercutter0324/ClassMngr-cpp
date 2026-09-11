#pragma once

#include <string>

namespace classmngr::engine
{

struct CampusRecord
{
    int id = -1;

    std::string name;

    std::string buildingName;
    std::string address;
    std::string phoneNumber;

    std::string officeNumber;

    std::string transitSteps;
    std::string arrivalInfo;

    std::string imagePath;

    std::string officeWifi;
    std::string officeWifiPassword;

    std::string printerName;
    std::string printerSteps;

    std::string photocopierCode;
    std::string housingLocations;
};

} // namespace classmngr::engine
