#include "speaking_eval_internal_pdf_renderer.h"

#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QFileInfo>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

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

QRectF reportPageRect(const QPdfWriter& writer)
{
    const QRect pageRect = writer.pageLayout().fullRectPixels(
        std::max(1, writer.resolution())
        );
    return !pageRect.isEmpty()
        ? QRectF(pageRect)
        : QRectF(0.0, 0.0, writer.width(), writer.height());
}
}

bool SpeakingEvalInternalPdfRenderer::render(
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

    QPdfWriter writer(documentPath);
    writer.setCreator(QStringLiteral("ClassMngr"));
    writer.setTitle(QObject::tr("Speaking Evaluation"));
    writer.setResolution(PdfPrintService::GeneratedPdfResolutionDpi);
    if (!writer.setPageLayout(reportPageLayout()))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Unable to configure the report PDF page.");
        }
        return false;
    }

    QPainter painter;
    if (!painter.begin(&writer))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Unable to create the report PDF.");
        }
        return false;
    }
    SpeakingEvalReportWidget report;
    report.setReportData(data);
    report.paintReport(&painter, reportPageRect(writer));
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
