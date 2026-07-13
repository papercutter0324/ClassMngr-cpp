#pragma once

#include "features/schedule/ui/schedule_view_model.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"

#include <QList>
#include <QString>

class QWidget;

namespace SubPrepPrintService
{
enum class Status
{
    Sent,
    Canceled,
    Failed
};

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

struct Request
{
    QWidget* parent = nullptr;
    CampusInformation campus;
    ZoomInformation zoom;
    QString classMaterials;
    QString gradingInstructions;
    QString specialInstructions;
    ScheduleViewModel schedule;
    QList<SubPrepClassInformation::TeacherGroup> classInformation;
    QString subNotes;
};

struct Result
{
    Status status = Status::Failed;
    QString message;
};

[[nodiscard]] Result saveSubPrepPdf(
    const Request& request,
    const QString& documentPath
    );

[[nodiscard]] Result printSubPrep(
    const Request& request
    );
}
