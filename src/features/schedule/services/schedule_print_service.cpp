#include "schedule_print_service.h"

#include "features/schedule/services/schedule_pdf_renderer.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QObject>
#include <QPageSize>
#include <QPdfDocument>
#include <QTemporaryDir>

namespace SchedulePrintService
{
namespace
{
constexpr QPageSize::PageSizeId SchedulePdfPageSize = QPageSize::A4;

Result failed(
    const QString& message
    )
{
    return {
        Status::Failed,
        message
    };
}

Result canceled()
{
    return {
        Status::Canceled,
        QString()
    };
}

Result sent()
{
    return {
        Status::Sent,
        QObject::tr("Print job sent.")
    };
}

SchedulePdfRenderer::Request rendererRequest(
    const Request& request
    )
{
    return {
        request.document,
        request.style,
        request.currentTheme,
        request.englishFont,
        request.koreanFont,
        request.userName,
        request.showEnglishNames,
        request.pageOrientation
    };
}
}

Result saveSchedulePdf(
    const Request& request,
    const QString& documentPath
    )
{
    return SchedulePdfRenderer::renderSchedulePdf(
        rendererRequest(request),
        documentPath
        );
}

Result printSchedule(
    const Request& request
    )
{
    QTemporaryDir temporaryDirectory;

    if (!temporaryDirectory.isValid())
    {
        return failed(
            QObject::tr("Unable to create a temporary print file.")
            );
    }

    const QString documentPath =
        temporaryDirectory.filePath(
            QStringLiteral("Schedule.pdf")
            );

    {
        const Result saveResult =
            saveSchedulePdf(
                request,
                documentPath
                );

        if (saveResult.status != Status::Sent)
        {
            return saveResult;
        }
    }

    QPdfDocument document;
    const QPdfDocument::Error loadError =
        document.load(documentPath);

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

    const PdfPrintService::Result result =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                documentPath,
                0,
                QObject::tr("Print Schedule"),
                request.pageOrientation,
                false,
                SchedulePdfPageSize,
                true,
                request.englishFont
            }
            );

    switch (result.status)
    {
    case PdfPrintService::Status::Sent:
        return sent();

    case PdfPrintService::Status::Canceled:
        return canceled();

    case PdfPrintService::Status::Failed:
    default:
        return failed(result.message);
    }
}
}
