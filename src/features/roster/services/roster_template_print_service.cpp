#include "features/roster/services/roster_template_print_service.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/startup_profiler.h"
#include "domain/rules/schedule_value_parser.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/roster_print_class_info_read_query.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_snapshot.h"
#include "next/application/roster_template_print_source_read_query.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QColor>
#include <QAtomicInteger>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QFileInfo>
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
#include <QSet>
#include <QTemporaryDir>
#include <QTime>
#include <QVector>

#include <algorithm>
#include <array>
#include <charconv>
#include <string>

namespace RosterTemplatePrintService
{
namespace
{

QAtomicInteger<int>& rosterPdfRenderSequence()
{
    static QAtomicInteger<int> sequence{0};
    return sequence;
}

class RendererProjectionReleaseGuard
{
public:
    ~RendererProjectionReleaseGuard()
    {
        if (m_active)
        {
            StartupProfiler::recordSubPrepOutputRendererProjectionReleased();
        }
    }

    void activate() noexcept
    {
        m_active = true;
    }

private:
    bool m_active = false;
};

RosterTemplateRenderer::Fonts fonts() { return {FontManager::getUiFont(-1), FontManager::getKoreanFont(-1)}; }
QList<RosterTemplateRenderer::RosterClassData> projected(const QList<RosterClassData>& classes)
{
    QList<RosterTemplateRenderer::RosterClassData> result;
    for (const auto& data : classes) {
        RosterTemplateRenderer::RosterClassData item;
        item.classroom = {data.classroom.id, data.classroom.name};
        item.classInfo = data.classInfo;
        item.roster = {data.roster.columns, data.roster.rows};
        result.append(std::move(item));
    }
    return result;
}
Result failed(const QString& message) { return {Status::Failed, message}; }
Result sent() { return {Status::Sent, {}}; }
Result canceled() { return {Status::Canceled, {}}; }
QString failedPdfMessage(const QString& message) { return message.isEmpty() ? QObject::tr("Unable to create the roster print file.") : message; }
#include "roster_template_print_private_service.inc"
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

QList<TemplateId> availableTemplateIds() { return RosterTemplateRenderer::availableTemplateIds(); }
QString templateDisplayName(TemplateId id) { return RosterTemplateRenderer::templateDisplayName(id); }
QPageLayout::Orientation templateOrientation(TemplateId id) { return RosterTemplateRenderer::templateOrientation(id); }
QPageSize::PageSizeId templatePageSize(TemplateId id) { return RosterTemplateRenderer::templatePageSize(id); }
int perClassExtraInfoMaxExtraColumns(QPageLayout::Orientation orientation) { return RosterTemplateRenderer::perClassExtraInfoMaxExtraColumns(orientation); }
QStringList availablePerClassExtraInfoColumns(const QList<RosterClassData>& classes) { return RosterTemplateRenderer::availablePerClassExtraInfoColumns(projected(classes)); }
QList<RosterCellValue> buildByDayCellValues(const QList<RosterClassData>& classes, QString* error) { return RosterTemplateRenderer::buildByDayCellValues(projected(classes), error); }
QList<RosterCellValue> buildDailyCellValues(const QList<RosterClassData>& classes, QString* error) { return RosterTemplateRenderer::buildDailyCellValues(projected(classes), error); }
QList<RosterCellValue> buildPerClassExtraInfoCellValues(const QList<RosterClassData>& classes, const QStringList& columns, QPageLayout::Orientation orientation, QString* error) { return RosterTemplateRenderer::buildPerClassExtraInfoCellValues(projected(classes), columns, orientation, error); }
QImage renderTemplatePreview(TemplateId id, const QSize& size)
{
    return RosterTemplateRenderer::previewPage(id,
        {id == TemplateId::PerClassWithExtraInfo ? QStringLiteral("class|0") : QStringLiteral("Monday"), {}},
        size, RosterTemplatePrintService::templateOrientation(id), fonts());
}
QImage renderTemplatePreview(const Request& request, const QSize& size, bool live, QString* error)
{
    if (!live) return RosterTemplateRenderer::previewPage(request.templateId,
        {request.templateId == TemplateId::PerClassWithExtraInfo ? QStringLiteral("class|0") : QStringLiteral("Monday"), {}},
        size, request.templateId == TemplateId::PerClassWithExtraInfo ? request.perClassExtraInfoOrientation : RosterTemplatePrintService::templateOrientation(request.templateId), fonts());
    QList<RosterClassData> classes;
    const auto loaded = loadRosterClassesForRequest(request, &classes);
    if (loaded.status != Status::Sent) { if (error) *error = loaded.message; return {}; }
    QString failure;
    const auto values = RosterTemplateRenderer::buildTemplateCellValues(request.templateId, projected(classes), request.selectedExtraColumns, request.perClassExtraInfoOrientation, &failure);
    const auto days = RosterTemplateRenderer::daysWithValues(request.templateId, values);
    if (!failure.isEmpty() || days.isEmpty()) { if (error) *error = failure.isEmpty() ? QObject::tr("No selected classes match this roster layout.") : failure; return {}; }
    return RosterTemplateRenderer::previewPage(request.templateId, {days.first(), values}, size,
        request.templateId == TemplateId::PerClassWithExtraInfo ? request.perClassExtraInfoOrientation : RosterTemplatePrintService::templateOrientation(request.templateId), fonts());
}
Result saveRostersPdf(
    const QList<RosterClassData>& classes,
    const QString& documentPath,
    TemplateId templateId,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation perClassExtraInfoOrientation
    )
{
    const bool profiling = StartupProfiler::isActive();
    QJsonObject rosterOperation;
    if (profiling)
    {
        int sourceRowCount = 0;
        int sourceCellCount = 0;
        int sourceColumnCount = 0;
        for (const RosterClassData& rosterClass : classes)
        {
            sourceRowCount += rosterClass.roster.rows.size();
            sourceColumnCount += rosterClass.roster.columns.size();
            for (const QStringList& row : rosterClass.roster.rows)
            {
                sourceCellCount += row.size();
            }
        }
        const int renderSequence =
            rosterPdfRenderSequence().fetchAndAddRelaxed(1) + 1;
        rosterOperation = {
            {
                QStringLiteral("operationId"),
                QStringLiteral("roster-pdf-%1").arg(renderSequence)
            },
            {QStringLiteral("operationType"), QStringLiteral("sub-prep-roster-pdf")},
            {QStringLiteral("templateId"), static_cast<int>(templateId)},
            {QStringLiteral("classCount"), classes.size()},
            {QStringLiteral("sourceRowCount"), sourceRowCount},
            {QStringLiteral("sourceCellCount"), sourceCellCount},
            {QStringLiteral("sourceColumnCount"), sourceColumnCount}
        };
        StartupProfiler::recordSubPrepRenderSample(
            QStringLiteral("sub-prep-roster-pdf-render-entry"),
            rosterOperation
            );
    }

    if (documentPath.trimmed().isEmpty())
    {
        return failed(
            QObject::tr("No roster print file path was provided.")
            );
    }

    QString errorMessage;
    RendererProjectionReleaseGuard projectionReleaseGuard;
    const QList<RosterCellValue> values =
        RosterTemplateRenderer::buildTemplateCellValues(
            templateId,
            projected(classes),
            selectedExtraColumns,
            perClassExtraInfoOrientation,
            &errorMessage
            );

    if (!errorMessage.isEmpty())
    {
        return failed(errorMessage);
    }

    StartupProfiler::recordSubPrepOutputRendererProjected(
        classes.size(),
        values.size()
        );
    projectionReleaseGuard.activate();

    const QStringList days =
        RosterTemplateRenderer::daysWithValues(
            templateId,
            values
            );

    if (days.isEmpty())
    {
        return failed(
            QObject::tr("No selected classes match the roster print layout.")
            );
    }

    const auto rendered = RosterTemplateRenderer::saveCellValuesPdf(documentPath, templateId,
        templateId == TemplateId::PerClassWithExtraInfo ? perClassExtraInfoOrientation : RosterTemplatePrintService::templateOrientation(templateId), values, fonts());
    if (rendered.status != Status::Sent) return rendered;

    if (profiling)
    {
        QJsonObject operation = rosterOperation;
        const QSize writerSize = QPageLayout(QPageSize(QPageSize::A4),
            templateId == TemplateId::PerClassWithExtraInfo ? perClassExtraInfoOrientation : RosterTemplatePrintService::templateOrientation(templateId),
            QMarginsF()).fullRectPixels(300).size();
        operation.insert(QStringLiteral("projectedCellCount"), values.size());
        operation.insert(QStringLiteral("pageCount"), days.size());
        operation.insert(
            QStringLiteral("writerWidthPixels"),
            writerSize.width()
            );
        operation.insert(
            QStringLiteral("writerHeightPixels"),
            writerSize.height()
            );
        operation.insert(
            QStringLiteral("writerResolutionDpi"),
            300
            );
        operation.insert(
            QStringLiteral("pdfBytes"),
            static_cast<double>(QFileInfo(documentPath).size())
            );
        StartupProfiler::recordSubPrepRenderSample(
            QStringLiteral("sub-prep-roster-pdf-writer-completion"),
            operation
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
                    : RosterTemplatePrintService::templateOrientation(request.templateId),
                false,
                RosterTemplatePrintService::templatePageSize(request.templateId),
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
