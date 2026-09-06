#pragma once

#include "classmngr/engine/schedule_report.h"
#include "classmngr/engine/sub_prep_class_information.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

struct SubPrepCampusInformation
{
    std::string officeNumber;
    std::string officeWifi;
    std::string officeWifiPassword;
    std::string photocopierCode;
};

struct SubPrepZoomInformation
{
    std::string loginId;
    std::string password;
};

struct SubPrepDocument
{
    SubPrepCampusInformation campus;
    SubPrepZoomInformation zoom;
    std::string classMaterials;
    std::string gradingInstructions;
    std::string specialInstructions;
    ScheduleReportModel schedule;
    std::vector<SubPrepTeacherGroup> classInformation;
    std::string subNotes;
};

struct SubPrepDocumentRequest
{
    SubPrepCampusInformation campus;
    SubPrepZoomInformation zoom;
    std::string classMaterials;
    std::string gradingInstructions;
    std::string specialInstructions;
    ScheduleReportModel schedule;
    std::vector<SubPrepTeacherGroup> classInformation;
    std::string subNotes;
};

class SubPrepDocumentService final
{
public:
    [[nodiscard]] static SubPrepDocument build(
        const SubPrepDocumentRequest& request
        );
};

} // namespace classmngr::engine
