#include "ui/shared/printing/pdf_print_service.h"

#include "ui/shared/printing/pdf_print_dialog.h"

#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QPdfDocument>
#include <QAbstractPrintDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
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

QString documentDisplayName(
    const QString& documentPath
    )
{
    const QString fileName =
        QFileInfo(documentPath).fileName();

    return fileName.trimmed().isEmpty()
        ? QObject::tr("PDF Document")
        : fileName;
}

QString printJobTitle(
    const QString& documentPath
    )
{
    return QObject::tr("Print from ClassMngr - %1")
        .arg(
            documentDisplayName(documentPath)
            );
}

QList<int> allPageIndexes(
    QPdfDocument* document
    )
{
    QList<int> pages;

    const int pageCount =
        document ? document->pageCount() : 0;
    pages.reserve(pageCount);

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        pages.append(pageIndex);
    }

    return pages;
}

QList<int> pageIndexesFromPrinterRange(
    const QPrinter& printer,
    QPdfDocument* document,
    int currentPageIndex
    )
{
    const int pageCount =
        document ? document->pageCount() : 0;

    if (pageCount <= 0)
    {
        return {};
    }

    if (printer.printRange() == QPrinter::CurrentPage)
    {
        return {
            std::clamp(
                currentPageIndex,
                0,
                pageCount - 1
                )
        };
    }

    if (printer.printRange() != QPrinter::PageRange)
    {
        return allPageIndexes(document);
    }

    const int fromPage =
        printer.fromPage();
    const int toPage =
        printer.toPage();

    if (
        fromPage < 1
        || toPage < 1
        || fromPage > toPage
        )
    {
        return allPageIndexes(document);
    }

    QList<int> pages;
    const int firstPage =
        std::clamp(
            fromPage,
            1,
            pageCount
            );
    const int lastPage =
        std::clamp(
            toPage,
            firstPage,
            pageCount
            );

    pages.reserve(lastPage - firstPage + 1);
    for (int pageNumber = firstPage; pageNumber <= lastPage; ++pageNumber)
    {
        pages.append(pageNumber - 1);
    }

    return pages;
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

    QRectF printBounds =
        options.fitToPage
            ? printer.pageRect(QPrinter::DevicePixel)
            : printer.paperRect(QPrinter::DevicePixel);

    if (
        printBounds.width() <= 0.0
        || printBounds.height() <= 0.0
        )
    {
        printBounds =
            printer.pageRect(QPrinter::DevicePixel);
    }

    if (
        printBounds.width() <= 0.0
        || printBounds.height() <= 0.0
        )
    {
        printBounds =
            QRectF(
                0.0,
                0.0,
                printer.width(),
                printer.height()
                );
    }

    if (
        printBounds.width() <= 0.0
        || printBounds.height() <= 0.0
        )
    {
        return false;
    }

    const QRectF targetRect =
        options.fitToPage
            ? fittedPrintRect(
                pagePointSize,
                printBounds
                )
            : naturalPrintRect(
                pagePointSize,
                printBounds,
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

    printer.setFullPage(
        !options.fitToPage
        );

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

Result printPdfDocumentWithCustomPreview(
    const Request& request
    )
{
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
        },
        request.currentPageIndex,
        request.pageOrientation,
        request.fitToPageByDefault,
        request.preferredPageSize,
        request.lockPreferredPageSize
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return canceled();
    }

    return dialog.printResult();
}

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
Result printPdfDocumentWithSystemDialog(
    const Request& request
    )
{
#ifdef Q_OS_LINUX
    if (QPrinterInfo::availablePrinterNames().isEmpty())
    {
        return failed(
            QObject::tr("No printers are available.")
            );
    }
#endif

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::NativeFormat);
    printer.setDocName(
        printJobTitle(request.documentPath)
        );
    printer.setCreator(
        QStringLiteral("ClassMngr")
        );
    printer.setPageOrientation(
        request.pageOrientation
        );
    if (request.preferredPageSize)
    {
        printer.setPageSize(
            QPageSize(*request.preferredPageSize)
            );
    }

    QPrintDialog dialog(
        &printer,
        request.parent
        );

#ifdef Q_OS_LINUX
    dialog.setOptions(
        QAbstractPrintDialog::PrintPageRange
        | QAbstractPrintDialog::PrintCurrentPage
        | QAbstractPrintDialog::PrintCollateCopies
        | QAbstractPrintDialog::PrintShowPageSize
        );
#endif
    dialog.setWindowTitle(
        request.dialogTitle.trimmed().isEmpty()
            ? printJobTitle(request.documentPath)
            : request.dialogTitle
        );
    dialog.setMinMax(
        1,
        request.document->pageCount()
        );
    dialog.setFromTo(
        1,
        request.document->pageCount()
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return canceled();
    }

    if (
        request.lockPreferredPageSize
        && request.preferredPageSize
        )
    {
        printer.setPageSize(
            QPageSize(*request.preferredPageSize)
            );
        printer.setPageOrientation(
            request.pageOrientation
            );
    }

    PdfPrintDialogSupport::RenderOptions options;
    options.pageIndexes =
        pageIndexesFromPrinterRange(
            printer,
            request.document,
            request.currentPageIndex
            );
    options.grayscale =
        printer.colorMode() == QPrinter::GrayScale;
    options.fitToPage =
        request.fitToPageByDefault;

    return paintPdfDocumentToPrinter(
        request.document,
        printer,
        options
        );
}
#endif
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

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return printPdfDocumentWithSystemDialog(
        request
        );
#else
    return printPdfDocumentWithCustomPreview(
        request
        );
#endif
}
}
