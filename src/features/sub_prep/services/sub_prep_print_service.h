#pragma once

#include "domain/models/document_output_result.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"

#include <QList>
#include <QFont>
#include <QString>

class QWidget;
class PdfGenerationPort;

namespace SubPrepPrintService
{
using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

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

[[nodiscard]] Result saveSubPrepPdf(
    const Request& request,
    const QString& documentPath
    );

[[nodiscard]] Result saveSubPrepPdf(
    PdfGenerationPort& pdfGeneration,
    const Request& request,
    const QString& documentPath,
    const QFont& outputFont);

[[nodiscard]] Result printSubPrep(
    PdfGenerationPort& pdfGeneration,
    const Request& request,
    const QFont& outputFont
    );

[[nodiscard]] Result printSubPrep(
    const Request& request
    );
}
