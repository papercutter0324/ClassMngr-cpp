#include "features/roster/services/roster_template_print_service.h"

#include "app/services/feature_services.h"
#include "classmngr/engine/roster_report.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QColor>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QMarginsF>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPen>
#include <QRect>
#include <QRectF>
#include <QTemporaryDir>
#include <QVector>

#include <algorithm>
#include <utility>
#include <vector>

namespace RosterTemplatePrintService
{
namespace
{

// Template-specific implementation is split into focused include fragments.
// They share this translation unit so rendering helpers remain private.

#include "roster_template_print_shared_data.inc"
#include "roster_template_print_shared_layout.inc"
#include "roster_template_print_shared_rendering.inc"
#include "roster_template_print_daily_template.inc"
#include "roster_template_print_by_day_template.inc"
#include "roster_template_print_per_class_template.inc"
#include "roster_template_print_private_service.inc"

classmngr::engine::RosterReportClass toPortableRosterClass(
    const RosterClassData& data
    )
{
    classmngr::engine::RosterReportClass portable;
    portable.classId = data.classroom.id;
    portable.classroomName = data.classroom.name.toStdString();
    portable.classGrade = data.info.classGrade.toStdString();
    portable.classLevel = data.info.classLevel.toStdString();
    portable.teacherEn = data.info.teacherEn.toStdString();
    portable.teacherKr = data.info.teacherKr.toStdString();
    portable.roomNumber = data.info.roomNumber.toStdString();
    portable.wifiName = data.info.wifiName.toStdString();
    portable.wifiPassword = data.info.wifiPassword.toStdString();
    portable.zoomId = data.info.zoomId.toStdString();
    portable.zoomPassword = data.info.zoomPassword.toStdString();
    portable.rosterColumns.reserve(data.roster.columns.size());
    for (const QString& column : data.roster.columns)
    {
        portable.rosterColumns.push_back(column.toStdString());
    }

    portable.classTimes.reserve(data.info.classTimes.size());
    for (const ClassTime& time : data.info.classTimes)
    {
        portable.classTimes.push_back({
            time.day.toStdString(),
            time.startTime.toStdString(),
            time.endTime.toStdString()
        });
    }

    portable.rosterRows.reserve(data.roster.rows.size());
    for (const QStringList& row : data.roster.rows)
    {
        std::vector<std::string> portableRow;
        portableRow.reserve(row.size());
        for (const QString& value : row)
        {
            portableRow.push_back(value.toStdString());
        }
        portable.rosterRows.push_back(std::move(portableRow));
    }

    return portable;
}

std::vector<classmngr::engine::RosterReportClass> toPortableRosterClasses(
    const QList<RosterClassData>& classes
    )
{
    std::vector<classmngr::engine::RosterReportClass> portable;
    portable.reserve(classes.size());
    for (const RosterClassData& data : classes)
    {
        portable.push_back(toPortableRosterClass(data));
    }
    return portable;
}

QList<RosterCellValue> fromPortableRosterValues(
    classmngr::engine::Result<
        std::vector<classmngr::engine::RosterReportCellValue>> result,
    QString* errorMessage
    )
{
    if (!result)
    {
        if (errorMessage)
        {
            *errorMessage =
                QString::fromStdString(result.error().message);
        }
        return {};
    }

    QList<RosterCellValue> values;
    values.reserve(static_cast<qsizetype>(result->size()));
    for (const auto& value : *result)
    {
        values.append({
            QString::fromStdString(value.page),
            value.row,
            value.column,
            QString::fromStdString(value.value)
        });
    }
    return values;
}

} // namespace

QList<TemplateId> availableTemplateIds()
{
    return {
        TemplateId::ByDay,
        TemplateId::Daily,
        TemplateId::PerClassWithExtraInfo
    };
}

QString templateDisplayName(
    TemplateId templateId
    )
{
    return templateTitle(templateId);
}

QPageLayout::Orientation templateOrientation(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return QPageLayout::Portrait;

    case TemplateId::Daily:
        return QPageLayout::Portrait;

    case TemplateId::ByDay:
    default:
        return QPageLayout::Landscape;
    }
}

QPageSize::PageSizeId templatePageSize(
    TemplateId templateId
    )
{
    Q_UNUSED(templateId);
    return RosterPdfPageSize;
}

QImage renderTemplatePreview(
    TemplateId templateId,
    const QSize& requestedSize
    )
{
    return renderPreviewImage(
        templateId,
        requestedSize,
        templateId == TemplateId::PerClassWithExtraInfo
            ? QStringLiteral("class|0")
            : DaySheets.first(),
        {},
        templateOrientation(templateId)
        );
}

QImage renderTemplatePreview(
    const Request& request,
    const QSize& requestedSize,
    bool liveData,
    QString* errorMessage
    )
{
    if (!liveData)
    {
        return renderPreviewImage(
            request.templateId,
            requestedSize,
            request.templateId == TemplateId::PerClassWithExtraInfo
                ? QStringLiteral("class|0")
                : DaySheets.first(),
            {},
            request.templateId == TemplateId::PerClassWithExtraInfo
                ? request.perClassExtraInfoOrientation
                : templateOrientation(request.templateId)
            );
    }

    QList<RosterClassData> rosterClasses;
    const Result loadResult =
        loadRosterClassesForRequest(
            request,
            &rosterClasses
            );

    if (loadResult.status != Status::Sent)
    {
        if (errorMessage)
        {
            *errorMessage = loadResult.message;
        }
        return {};
    }

    QString valueError;
    const QList<RosterCellValue> values =
        buildTemplateCellValues(
            request.templateId,
            rosterClasses,
            request.selectedExtraColumns,
            request.perClassExtraInfoOrientation,
            &valueError
            );

    if (!valueError.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = valueError;
        }
        return {};
    }

    const QStringList days =
        daysWithValues(
            request.templateId,
            values
            );

    if (days.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("No selected classes match this roster layout.");
        }
        return {};
    }

    const QString day =
        days.first();

    return renderPreviewImage(
        request.templateId,
        requestedSize,
        day,
        valuesForDay(
            values,
            day
            ),
        request.templateId == TemplateId::PerClassWithExtraInfo
            ? request.perClassExtraInfoOrientation
            : templateOrientation(request.templateId)
        );
}

QList<int> resolveClassIds(
    Scope scope,
    int currentClassId,
    const QList<int>& selectedClassIds,
    const QList<Classroom>& classes
    )
{
    QList<int> ids;

    switch (scope)
    {
    case Scope::CurrentClass:
        if (currentClassId > 0)
        {
            ids.append(currentClassId);
        }
        break;

    case Scope::SelectedClasses:
        for (int classId : selectedClassIds)
        {
            if (classId > 0 && !ids.contains(classId))
            {
                ids.append(classId);
            }
        }
        break;

    case Scope::AllClasses:
    default:
        for (const Classroom& classroom : classes)
        {
            if (classroom.id > 0 && !ids.contains(classroom.id))
            {
                ids.append(classroom.id);
            }
        }
        break;
    }

    return ids;
}

QList<RosterCellValue> buildByDayCellValues(
    const QList<RosterClassData>& classes,
    QString* errorMessage
    )
{
    return fromPortableRosterValues(
        classmngr::engine::RosterReportService::buildByDayCellValues(
            toPortableRosterClasses(classes)
            ),
        errorMessage
        );
}

QList<RosterCellValue> buildDailyCellValues(
    const QList<RosterClassData>& classes,
    QString* errorMessage
    )
{
    return fromPortableRosterValues(
        classmngr::engine::RosterReportService::buildDailyCellValues(
            toPortableRosterClasses(classes)
            ),
        errorMessage
        );
}

int perClassExtraInfoMaxExtraColumns(
    QPageLayout::Orientation orientation
    )
{
    return classmngr::engine::RosterReportService::perClassExtraInfoMaxColumns(
        orientation == QPageLayout::Landscape
            ? classmngr::engine::RosterReportOrientation::Landscape
            : classmngr::engine::RosterReportOrientation::Portrait
        );
}

QStringList availablePerClassExtraInfoColumns(
    const QList<RosterClassData>& classes
    )
{
    const std::vector<std::string> portableColumns =
        classmngr::engine::RosterReportService::availablePerClassExtraInfoColumns(
            toPortableRosterClasses(classes)
            );
    QStringList columns;
    columns.reserve(static_cast<qsizetype>(portableColumns.size()));
    for (const std::string& column : portableColumns)
    {
        columns.append(QString::fromStdString(column));
    }
    return columns;
}

QList<RosterCellValue> buildPerClassExtraInfoCellValues(
    const QList<RosterClassData>& classes,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation orientation,
    QString* errorMessage
    )
{
    std::vector<std::string> portableColumns;
    portableColumns.reserve(selectedExtraColumns.size());
    for (const QString& column : selectedExtraColumns)
    {
        portableColumns.push_back(column.toStdString());
    }

    return fromPortableRosterValues(
        classmngr::engine::RosterReportService::buildPerClassExtraInfoCellValues(
            toPortableRosterClasses(classes),
            portableColumns,
            orientation == QPageLayout::Landscape
                ? classmngr::engine::RosterReportOrientation::Landscape
                : classmngr::engine::RosterReportOrientation::Portrait
            ),
        errorMessage
        );
}

Result saveRostersPdf(
    const QList<RosterClassData>& classes,
    const QString& documentPath,
    TemplateId templateId,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation perClassExtraInfoOrientation
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return failed(
            QObject::tr("No roster print file path was provided.")
            );
    }

    QString errorMessage;
    const QList<RosterCellValue> values =
        buildTemplateCellValues(
            templateId,
            classes,
            selectedExtraColumns,
            perClassExtraInfoOrientation,
            &errorMessage
            );

    if (!errorMessage.isEmpty())
    {
        return failed(errorMessage);
    }

    const QStringList days =
        daysWithValues(
            templateId,
            values
            );

    if (days.isEmpty())
    {
        return failed(
            QObject::tr("No selected classes match the roster print layout.")
            );
    }

    QPdfWriter writer(documentPath);
    if (!configureRosterPdfWriter(
            writer,
            templateId,
            templateId == TemplateId::PerClassWithExtraInfo
                ? perClassExtraInfoOrientation
                : templateOrientation(templateId)
            ))
    {
        return failed(
            QObject::tr("Unable to configure the roster print file.")
            );
    }

    QPainter painter;
    if (!painter.begin(&writer))
    {
        return failed(
            QObject::tr("Unable to create the roster print file.")
            );
    }

    const QRectF pageRect =
        rosterPdfPageRect(writer);
    const QRectF contentRect =
        templateContentRect(
            templateId,
            pageRect,
            writer.resolution()
            );

    if (
        pageRect.width() <= 0.0
        || pageRect.height() <= 0.0
        || contentRect.width() <= 0.0
        || contentRect.height() <= 0.0
        )
    {
        painter.end();
        return failed(
            QObject::tr("Unable to determine the roster print area.")
            );
    }

    for (int index = 0; index < days.size(); ++index)
    {
        if (
            index > 0
            && !writer.newPage()
            )
        {
            painter.end();
            return failed(
                QObject::tr("Unable to add a roster print page.")
                );
        }

        const QString& day =
            days.at(index);
        paintTemplateDay(
            templateId,
            painter,
            day,
            valuesForDay(
                values,
                day
                ),
            pageRect,
            contentRect,
            writer.resolution()
            );
    }

    if (!painter.end())
    {
        return failed(
            QObject::tr("The roster print file could not be completed.")
            );
    }

    return {
        Status::Sent,
        QObject::tr("Roster PDF created.")
    };
}

Result saveRostersPdf(
    const Request& request,
    const QString& documentPath
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return saveRostersPdf(
            QList<RosterClassData>(),
            documentPath,
            request.templateId,
            request.selectedExtraColumns,
            request.perClassExtraInfoOrientation
            );
    }

    QList<RosterClassData> rosterClasses;
    const Result loadResult =
        loadRosterClassesForRequest(
            request,
            &rosterClasses
            );

    if (loadResult.status != Status::Sent)
    {
        return loadResult;
    }

    return saveRostersPdf(
        rosterClasses,
        documentPath,
        request.templateId,
        request.selectedExtraColumns,
        request.perClassExtraInfoOrientation
        );
}

Result printRosters(
    const Request& request
    )
{
    QList<RosterClassData> rosterClasses;
    const Result loadResult =
        loadRosterClassesForRequest(
            request,
            &rosterClasses
            );

    if (loadResult.status != Status::Sent)
    {
        return loadResult;
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return failed(QObject::tr("Unable to create a temporary print folder."));
    }

    const QString pdfPath =
        temporaryDirectory.filePath(QStringLiteral("Rosters.pdf"));

    {
        const Result saveResult =
            saveRostersPdf(
                rosterClasses,
                pdfPath,
                request.templateId,
                request.selectedExtraColumns,
                request.perClassExtraInfoOrientation
                );

        if (saveResult.status != Status::Sent)
        {
            return failed(
                failedPdfMessage(saveResult.message)
                );
        }
    }

    QPdfDocument document;
    const QPdfDocument::Error loadError =
        document.load(pdfPath);

    if (
        loadError != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0
        )
    {
        return failed(QObject::tr("Unable to load the generated roster PDF."));
    }

    const PdfPrintService::Result printResult =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                pdfPath,
                0,
                QObject::tr("Print Rosters"),
                request.templateId == TemplateId::PerClassWithExtraInfo
                    ? request.perClassExtraInfoOrientation
                    : templateOrientation(request.templateId),
                false,
                templatePageSize(request.templateId),
                true
            }
            );

    switch (printResult.status)
    {
    case PdfPrintService::Status::Sent:
        return sent();

    case PdfPrintService::Status::Canceled:
        return canceled();

    case PdfPrintService::Status::Failed:
    default:
        return failed(printResult.message);
    }
}

} // namespace RosterTemplatePrintService
