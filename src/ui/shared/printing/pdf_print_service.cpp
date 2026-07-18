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
#include <memory>
#include <vector>

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

QPageLayout::Orientation pageOrientation(
    QPdfDocument* document,
    int pageIndex,
    QPageLayout::Orientation fallback
    )
{
    if (!document || pageIndex < 0 || pageIndex >= document->pageCount())
    {
        return fallback;
    }

    const QSizeF size = document->pagePointSize(pageIndex);

    if (
        !size.isValid()
        || size.isEmpty()
        || qFuzzyCompare(size.width(), size.height())
        )
    {
        return fallback;
    }

    return size.width() > size.height()
        ? QPageLayout::Landscape
        : QPageLayout::Portrait;
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
    printer.setDuplex(QPrinter::DuplexLongSide);
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

Result printPdfDocuments(
    const BatchRequest& request
    )
{
    if (request.documentPaths.isEmpty())
    {
        return failed(
            QObject::tr("No PDF files are available to print.")
            );
    }

#ifdef Q_OS_LINUX
    if (QPrinterInfo::availablePrinterNames().isEmpty())
    {
        return failed(
            QObject::tr("No printers are available.")
            );
    }
#endif

    std::vector<std::unique_ptr<QPdfDocument>> documents;
    documents.reserve(request.documentPaths.size());
    int pageCount = 0;

    for (const QString& documentPath : request.documentPaths)
    {
        auto document = std::make_unique<QPdfDocument>();
        if (document->load(documentPath) != QPdfDocument::Error::None
            || document->status() != QPdfDocument::Status::Ready
            || document->pageCount() <= 0)
        {
            return failed(
                QObject::tr("Unable to open \"%1\" for printing.")
                    .arg(documentDisplayName(documentPath))
                );
        }

        pageCount += document->pageCount();
        documents.push_back(std::move(document));
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::NativeFormat);
    printer.setDuplex(QPrinter::DuplexLongSide);
    printer.setDocName(
        request.dialogTitle.trimmed().isEmpty()
            ? QObject::tr("Print from ClassMngr - Speaking Evaluation Reports")
            : request.dialogTitle
        );
    printer.setCreator(QStringLiteral("ClassMngr"));
    printer.setPageOrientation(request.pageOrientation);
    if (request.preferredPageSize)
    {
        printer.setPageSize(QPageSize(*request.preferredPageSize));
    }

    QPrintDialog dialog(&printer, request.parent);
    dialog.setWindowTitle(
        request.dialogTitle.trimmed().isEmpty()
            ? QObject::tr("Print Reports")
            : request.dialogTitle
        );
    dialog.setMinMax(1, pageCount);
    dialog.setFromTo(1, pageCount);

#ifdef Q_OS_LINUX
    dialog.setOptions(
        QAbstractPrintDialog::PrintPageRange
        | QAbstractPrintDialog::PrintCollateCopies
        | QAbstractPrintDialog::PrintShowPageSize
        );
#endif

    if (dialog.exec() != QDialog::Accepted)
    {
        return canceled();
    }

    int firstPage = 1;
    int lastPage = pageCount;
    if (printer.printRange() == QPrinter::PageRange
        && printer.fromPage() > 0
        && printer.toPage() >= printer.fromPage())
    {
        firstPage = std::clamp(printer.fromPage(), 1, pageCount);
        lastPage = std::clamp(printer.toPage(), firstPage, pageCount);
    }

    int firstIncludedGlobalPage = 0;
    QPdfDocument* firstIncludedDocument = nullptr;
    int firstIncludedDocumentPage = -1;
    for (const std::unique_ptr<QPdfDocument>& document : documents)
    {
        for (int pageIndex = 0; pageIndex < document->pageCount(); ++pageIndex)
        {
            ++firstIncludedGlobalPage;
            if (
                firstIncludedGlobalPage >= firstPage
                && firstIncludedGlobalPage <= lastPage
                )
            {
                firstIncludedDocument = document.get();
                firstIncludedDocumentPage = pageIndex;
                break;
            }
        }

        if (firstIncludedDocument)
        {
            break;
        }
    }

    if (firstIncludedDocument)
    {
        printer.setPageOrientation(
            pageOrientation(
                firstIncludedDocument,
                firstIncludedDocumentPage,
                request.pageOrientation
                )
            );
    }

    printer.setFullPage(!request.fitToPage);
    PdfPrintDialogSupport::RenderOptions options;
    options.grayscale = printer.colorMode() == QPrinter::GrayScale;
    options.fitToPage = request.fitToPage;

    QPainter painter;
    if (!painter.begin(&printer))
    {
        return failed(
            QObject::tr("Unable to start the print job.")
            );
    }

    int globalPage = 0;
    int printedPageCount = 0;
    for (const std::unique_ptr<QPdfDocument>& document : documents)
    {
        for (int pageIndex = 0;
             pageIndex < document->pageCount();
             ++pageIndex)
        {
            ++globalPage;
            if (globalPage < firstPage || globalPage > lastPage)
            {
                continue;
            }

            if (printedPageCount > 0)
            {
                printer.setPageOrientation(
                    pageOrientation(
                        document.get(),
                        pageIndex,
                        request.pageOrientation
                        )
                    );

                if (!printer.newPage())
                {
                    painter.end();
                    return failed(
                        QObject::tr("Unable to create a new printed page.")
                        );
                }
            }

            if (!renderPdfPageToPrinter(
                    document.get(),
                    pageIndex,
                    printer,
                    painter,
                    options
                    ))
            {
                painter.end();
                return failed(
                    QObject::tr("Unable to render a PDF page for printing.")
                    );
            }

            ++printedPageCount;
        }
    }

    if (printedPageCount == 0)
    {
        painter.end();
        return failed(
            QObject::tr("No pages were selected to print.")
            );
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
