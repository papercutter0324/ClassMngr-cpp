#include "sub_prep_print_service.h"

#include "sub_prep_document_model.h"
#include "sub_prep_pdf_renderer.h"

#include "ui/shared/printing/pdf_print_service.h"

#include <QCoreApplication>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfDocument>
#include <QTemporaryDir>

namespace SubPrepPrintService
{
namespace
{
constexpr QPageSize::PageSizeId SubPrepPdfPageSize = QPageSize::A4;

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

QString translate(
    const char* source
    )
{
    return QCoreApplication::translate(
        "SubPrepPage",
        source
        );
}
}

Result saveSubPrepPdf(
    const Request& request,
    const QString& documentPath
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return failed(
            QObject::tr("No sub prep print file path was provided.")
            );
    }

    return SubPrepPdfRenderer::renderPdf(
        SubPrepDocumentModel::build(request),
        documentPath
        );
}

Result printSubPrep(
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
            QStringLiteral("Sub Prep.pdf")
            );
    const Result saveResult =
        saveSubPrepPdf(request, documentPath);

    if (saveResult.status != Status::Sent)
    {
        return saveResult;
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
            QObject::tr("Unable to load the sub prep print file.")
            );
    }

    const PdfPrintService::Result printResult =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                documentPath,
                0,
                translate("Print Sub Prep"),
                QPageLayout::Portrait,
                false,
                SubPrepPdfPageSize,
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
}
