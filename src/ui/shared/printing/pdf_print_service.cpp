#include "ui/shared/printing/pdf_print_service.h"

#include "ui/shared/printing/pdf_print_dialog.h"

#include <QImage>
#include <QObject>
#include <QPainter>
#include <QPdfDocument>
#include <QPrinter>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QtGlobal>

#include <algorithm>

namespace PdfPrintService
{
namespace
{
constexpr int MaximumPrintRenderDpi = 300;

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

QRectF fittedPrintRect(
    const QSizeF& sourceSize,
    const QRectF& bounds
    )
{
    QSizeF targetSize =
        sourceSize;
    targetSize.scale(
        bounds.size(),
        Qt::KeepAspectRatio
        );

    return QRectF(
        bounds.x()
            + (bounds.width() - targetSize.width()) / 2.0,
        bounds.y()
            + (bounds.height() - targetSize.height()) / 2.0,
        targetSize.width(),
        targetSize.height()
        );
}

QRectF naturalPrintRect(
    const QSizeF& sourcePointSize,
    const QRectF& bounds,
    const QPrinter& printer
    )
{
    const qreal horizontalPixelsPerPoint =
        std::max(
            printer.logicalDpiX() / 72.0,
            0.01
            );
    const qreal verticalPixelsPerPoint =
        std::max(
            printer.logicalDpiY() / 72.0,
            0.01
            );

    const QSizeF targetSize(
        sourcePointSize.width() * horizontalPixelsPerPoint,
        sourcePointSize.height() * verticalPixelsPerPoint
        );

    return QRectF(
        bounds.x()
            + (bounds.width() - targetSize.width()) / 2.0,
        bounds.y()
            + (bounds.height() - targetSize.height()) / 2.0,
        targetSize.width(),
        targetSize.height()
        );
}

bool renderPdfPageToPrinter(
    QPdfDocument* document,
    int pageIndex,
    QPrinter& printer,
    QPainter& painter,
    const PdfPrintDialogSupport::RenderOptions& options
    )
{
    if (!document)
    {
        return false;
    }

    const QSizeF pagePointSize =
        document->pagePointSize(pageIndex);

    if (
        !pagePointSize.isValid()
        || pagePointSize.isEmpty()
        )
    {
        return false;
    }

    QRectF printableRect =
        printer.pageRect(QPrinter::DevicePixel);

    if (
        printableRect.width() <= 0.0
        || printableRect.height() <= 0.0
        )
    {
        printableRect =
            QRectF(
                0.0,
                0.0,
                printer.width(),
                printer.height()
                );
    }

    if (
        printableRect.width() <= 0.0
        || printableRect.height() <= 0.0
        )
    {
        return false;
    }

    const QRectF targetRect =
        options.fitToPage
            ? fittedPrintRect(
                pagePointSize,
                printableRect
                )
            : naturalPrintRect(
                pagePointSize,
                printableRect,
                printer
                );

    const int printerDpi =
        std::max(
            1,
            printer.resolution()
            );
    const int renderDpi =
        std::min(
            printerDpi,
            MaximumPrintRenderDpi
            );
    const qreal renderScale =
        static_cast<qreal>(renderDpi) / printerDpi;

    const QSize renderSize(
        std::max(
            1,
            qRound(targetRect.width() * renderScale)
            ),
        std::max(
            1,
            qRound(targetRect.height() * renderScale)
            )
        );

    QImage image =
        document->render(
            pageIndex,
            renderSize
            );

    if (image.isNull())
    {
        return false;
    }

    if (options.grayscale)
    {
        image =
            image.convertToFormat(
                QImage::Format_Grayscale8
                );
    }

    painter.save();
    painter.setRenderHint(
        QPainter::SmoothPixmapTransform,
        true
        );
    painter.fillRect(
        targetRect,
        Qt::white
        );
    painter.drawImage(
        targetRect,
        image
        );
    painter.restore();

    return true;
}

Result paintPdfDocumentToPrinter(
    QPdfDocument* document,
    QPrinter& printer,
    const PdfPrintDialogSupport::RenderOptions& options
    )
{
    if (
        !document
        || document->status() != QPdfDocument::Status::Ready
        || document->pageCount() <= 0
        )
    {
        return failed(
            QObject::tr("No PDF file is available to print.")
            );
    }

    if (options.pageIndexes.isEmpty())
    {
        return failed(
            QObject::tr("No pages were selected to print.")
            );
    }

    QPainter painter;

    if (!painter.begin(&printer))
    {
        return failed(
            QObject::tr("Unable to start the print job.")
            );
    }

    for (qsizetype index = 0; index < options.pageIndexes.size(); ++index)
    {
        if (
            index > 0
            && !printer.newPage()
            )
        {
            painter.end();
            return failed(
                QObject::tr("Unable to create a new printed page.")
                );
        }

        const int pageIndex =
            options.pageIndexes.at(index);

        if (
            !renderPdfPageToPrinter(
                document,
                pageIndex,
                printer,
                painter,
                options
                )
            )
        {
            painter.end();
            return failed(
                QObject::tr("Unable to render page %1 for printing.")
                    .arg(pageIndex + 1)
                );
        }
    }

    if (!painter.end())
    {
        return failed(
            QObject::tr("The print job could not be completed.")
            );
    }

    if (printer.printerState() == QPrinter::Error)
    {
        return failed(
            QObject::tr("The printer reported an error while printing.")
            );
    }

    return sent();
}
}

Result printPdfDocument(
    const Request& request
    )
{
    if (
        !request.document
        || request.document->status() != QPdfDocument::Status::Ready
        || request.document->pageCount() <= 0
        )
    {
        return failed(
            QObject::tr("No PDF file is available to print.")
            );
    }

    PdfPrintDialog dialog(
        request.parent,
        request.document,
        request.documentPath,
        [document = request.document](
            QPrinter& printer,
            const PdfPrintDialogSupport::RenderOptions& options
            )
        {
            return paintPdfDocumentToPrinter(
                document,
                printer,
                options
                );
        }
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return canceled();
    }

    return dialog.printResult();
}
}
