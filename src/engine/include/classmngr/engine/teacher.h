#pragma once

#include <string>
#include <vector>

namespace classmngr::engine
{

struct Teacher
{
    int id = -1;

    std::string teacherKr;
    std::string teacherEn;
    std::string preferredRomanization;
    std::string preferredName;

    std::string roomNumber;
    std::string birthday;
    std::string phoneNumber;

    std::string wifiName;
    std::string wifiPassword;
    std::string internetType = "WiFi";

    std::string zoomId;
    std::string zoomPassword;
    std::string projectionType = "HDMI";

    std::string notes;

    [[nodiscard]] std::vector<std::string> preferredNameChoices() const;
    [[nodiscard]] std::string preferredDisplayName() const;
};

} // namespace classmngr::engine
