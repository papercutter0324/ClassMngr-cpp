#include "next/platform/qt_schedule_output_adapter.h"
#include "core/qt_pdf_generation_adapter.h"

#include "features/schedule/services/schedule_pdf_renderer.h"

#include <exception>
#include <utility>

#include <QObject>
#include <QPageSize>
#include <QPdfDocument>
#include <QTemporaryDir>

namespace ClassMngr::Next::Platform
{
namespace
{
using OutputStatus = Ui::ScheduleOutputResult::Status;

Ui::ScheduleOutputResult outputResult(
    const PdfPrintResult& result
    )
{
    switch (result.status)
    {
    case PdfPrintStatus::Sent:
        return {OutputStatus::Sent, result.message};
    case PdfPrintStatus::Canceled:
        return {OutputStatus::Canceled, {}};
    case PdfPrintStatus::Failed:
    default:
        return {OutputStatus::Failed, result.message};
    }
}

Ui::ScheduleOutputResult outputResult(
    const SchedulePdfRenderer::Result& result
    )
{
    switch (result.status)
    {
    case SchedulePdfRenderer::Status::Sent:
        return {OutputStatus::Sent, result.message};
    case SchedulePdfRenderer::Status::Canceled:
        return {OutputStatus::Canceled, {}};
    case SchedulePdfRenderer::Status::Failed:
    case SchedulePdfRenderer::Status::InternalRendererFailed:
    default:
        return {OutputStatus::Failed, result.message};
    }
}

SchedulePdfRenderer::Request rendererRequest(
    Ui::ScheduleOutputRequest& request
    )
{
    return {
        std::move(request.document),
        request.style,
        request.currentTheme,
        std::move(request.englishFont),
        std::move(request.koreanFont),
        std::move(request.profileName),
        request.showEnglishNames,
        request.orientation
    };
}

Ui::ScheduleOutputResult failed(
    const QString& message
    )
{
    return {OutputStatus::Failed, message};
}
}

Ui::ScheduleOutputResult performScheduleOutput(
    Ui::ScheduleOutputRequest request,
    QWidget* parent,
    PdfPrintOperationFactory printOperationFactory
    )
{
    try
    {
        const auto pdf = makeQtPdfGenerationPort();
        const SchedulePdfRenderer::Request pdfRequest =
            rendererRequest(request);

        if (request.kind == Ui::ScheduleOutputKind::SaveAsPdf)
        {
            const SchedulePdfRenderer::Result saveResult =
                SchedulePdfRenderer::renderSchedulePdf(
                    *pdf,
                    pdfRequest,
                    request.documentPath
                    );
            return outputResult(saveResult);
        }

        QTemporaryDir temporaryDirectory;
        if (!temporaryDirectory.isValid())
        {
            return failed(
                QObject::tr("Unable to create a temporary print file.")
                );
        }

        const QString documentPath = temporaryDirectory.filePath(
            QStringLiteral("Schedule.pdf")
            );
        const SchedulePdfRenderer::Result renderResult =
            SchedulePdfRenderer::renderSchedulePdf(
                *pdf,
                pdfRequest,
                documentPath
                );
        if (renderResult.status != SchedulePdfRenderer::Status::Sent)
        {
            return failed(renderResult.message);
        }

        PdfPrintResult printResult;
        {
            QPdfDocument document;
            const QPdfDocument::Error loadError = document.load(documentPath);
            if (
                loadError != QPdfDocument::Error::None
                || document.status() != QPdfDocument::Status::Ready
                || document.pageCount() <= 0
                )
            {
                return failed(
                    QObject::tr("Unable to load the schedule print file.")
                    );
            }

            if (!printOperationFactory)
            {
                return failed(
                    QObject::tr("Unable to create the schedule print operation.")
                    );
            }
            std::unique_ptr<PdfPrintOperation> printOperation =
                printOperationFactory();
            if (!printOperation)
            {
                return failed(
                    QObject::tr("Unable to create the schedule print operation.")
                    );
            }

            printResult = printOperation->printReadyDocument({
                .parent = parent,
                .document = &document,
                .documentPath = documentPath,
                .currentPageIndex = 0,
                .dialogTitle = QObject::tr("Print Schedule"),
                .pageOrientation = request.orientation,
                .fitToPageByDefault = false,
                .preferredPageSize = QPageSize::A4,
                .lockPreferredPageSize = true,
                .uiFont = pdfRequest.englishFont
            });
            document.close();
        }

        return outputResult(printResult);
    }
    catch (const std::exception&)
    {
        return failed({});
    }
    catch (...)
    {
        return failed({});
    }
}

} // namespace ClassMngr::Next::Platform
