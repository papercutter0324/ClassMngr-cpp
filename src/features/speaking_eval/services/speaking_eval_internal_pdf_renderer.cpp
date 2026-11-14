#include "speaking_eval_internal_pdf_renderer.h"

#include "core/pdf_generation_port.h"

#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "features/speaking_eval/ui/speaking_eval_report_painter.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QFileInfo>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>

#include <algorithm>

namespace
{
QPageLayout reportPageLayout()
{
    return QPageLayout(
        QPageSize(
            QSizeF(7.5, 10.833333),
            QPageSize::Inch,
            QStringLiteral("SpeakingEvaluation")
            ),
        QPageLayout::Portrait,
        QMarginsF(),
        QPageLayout::Inch
        );
}

QRectF reportPageRect(PdfGenerationSession& writer)
{
    const QRect pageRect = writer.pageRectPixels();
    return !pageRect.isEmpty()
        ? QRectF(pageRect)
        : QRectF(0.0, 0.0, writer.paintDevice().width(), writer.paintDevice().height());
}
}

bool SpeakingEvalInternalPdfRenderer::render(
    PdfGenerationPort& pdfGeneration,
    const SpeakingEvalReportData& data,
    const QString& documentPath,
    QString* errorMessage
    )
{
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(data.reportTemplate);
    if (!assets.valid)
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The internal speaking-evaluation renderer is unavailable: %1"
                ).arg(assets.error);
        }
        return false;
    }

    auto session = pdfGeneration.create(documentPath, {
        .creator = QStringLiteral("ClassMngr"),
        .title = QObject::tr("Speaking Evaluation"),
        .resolutionDpi = PdfPrintService::GeneratedPdfResolutionDpi,
        .pageLayout = reportPageLayout()
    });
    if (!session)
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Unable to configure the report PDF page.");
        }
        return false;
    }

    auto& writer = *session;
    QPainter painter;
    if (!painter.begin(&writer.paintDevice()))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Unable to create the report PDF.");
        }
        return false;
    }
    SpeakingEvalReportPainter report;
    report.paint(&painter, reportPageRect(writer), data);
    if (!painter.end())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The report PDF could not be completed.");
        }
        return false;
    }
    if (!QFileInfo::exists(documentPath) || QFileInfo(documentPath).size() <= 0)
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The report PDF was not created.");
        }
        return false;
    }
    return true;
}
