#pragma once

#include "features/schedule/ui/schedule_view_model.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"

#include <QList>
#include <QString>

namespace SubPrepPrintService
{
struct Request;
}

namespace SubPrepDocumentModel
{
struct CampusInformation
{
    QString officeNumber;
    QString officeWifi;
    QString officeWifiPassword;
    QString photocopierCode;
};

struct ZoomInformation
{
    QString loginId;
    QString password;
};

struct Document
{
    CampusInformation campus;
    ZoomInformation zoom;
    QString classMaterials;
    QString gradingInstructions;
    QString specialInstructions;
    ScheduleViewModel schedule;
    QList<SubPrepClassInformation::TeacherGroup> classInformation;
    QString subNotes;
};

[[nodiscard]] Document build(
    const SubPrepPrintService::Request& request
    );
}
