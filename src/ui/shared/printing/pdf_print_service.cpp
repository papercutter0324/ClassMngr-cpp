#include "ui/shared/printing/pdf_print_service.h"

#include <QAbstractPrintDialog>
#include <QDialog>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPageRanges>
#include <QPainter>
#include <QPdfDocument>
#include <QPrintDialog>
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

QList<int> allPageIndexes(
    int pageCount
    )
{
    QList<int> pages;
    pages.reserve(pageCount);

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        pages.append(pageIndex);
    }

    return pages;
}

QList<int> rangePageIndexes(
    const QPrinter& printer,
    int pageCount
    )
{
    const QPageRanges pageRanges =
        printer.pageRanges();

    QList<int> pages;

    if (!pageRanges.isEmpty())
    {
        pages.reserve(pageCount);

        for (int pageNumber = 1; pageNumber <= pageCount; ++pageNumber)
        {
            if (pageRanges.contains(pageNumber))
            {
                pages.append(pageNumber - 1);
            }
        }

        return pages;
    }

    int fromPage =
        printer.fromPage();
    int toPage =
        printer.toPage();

    if (fromPage <= 0)
    {
        fromPage =
            1;
    }

    if (toPage <= 0)
    {
        toPage =
            pageCount;
    }

    fromPage =
        std::clamp(
            fromPage,
            1,
            pageCount
            );
    toPage =
        std::clamp(
            toPage,
            1,
            pageCount
            );

    if (fromPage > toPage)
    {
        std::swap(
            fromPage,
            toPage
            );
    }

    pages.reserve(
        toPage - fromPage + 1
        );

    for (int pageNumber = fromPage; pageNumber <= toPage; ++pageNumber)
    {
        pages.append(pageNumber - 1);
    }

    return pages;
}

QList<int> selectedPrintPageIndexes(
    const QPrinter& printer,
    int pageCount,
    int currentPageIndex
    )
{
    if (pageCount <= 0)
    {
        return {};
    }

    switch (printer.printRange())
    {
    case QPrinter::CurrentPage:
    {
        QList<int> pages;
        pages.append(
            std::clamp(
                currentPageIndex,
                0,
                pageCount - 1
                )
            );
        return pages;
    }

    case QPrinter::PageRange:
        return rangePageIndexes(
            printer,
            pageCount
            );

    case QPrinter::AllPages:
    case QPrinter::Selection:
        break;
    }

    return allPageIndexes(pageCount);
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

bool renderPdfPageToPrinter(
    QPdfDocument* document,
    int pageIndex,
    QPrinter& printer,
    QPainter& painter
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
        fittedPrintRect(
            pagePointSize,
            printableRect
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

    const QImage image =
        document->render(
            pageIndex,
            renderSize
            );

    if (image.isNull())
    {
        return false;
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

    const int pageCount =
        request.document->pageCount();

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(
        QFileInfo(request.documentPath).fileName()
        );
    printer.setFromTo(
        1,
        pageCount
        );
    printer.setPrintRange(
        QPrinter::AllPages
        );

    QPrintDialog dialog(
        &printer,
        request.parent
        );
    dialog.setWindowTitle(
        request.dialogTitle.isEmpty()
            ? QObject::tr("Print File")
            : request.dialogTitle
        );
    dialog.setOptions(
        QAbstractPrintDialog::PrintPageRange
        | QAbstractPrintDialog::PrintCurrentPage
        | QAbstractPrintDialog::PrintCollateCopies
        | QAbstractPrintDialog::PrintToFile
        );
    dialog.setMinMax(
        1,
        pageCount
        );
    dialog.setFromTo(
        1,
        pageCount
        );
    dialog.setPrintRange(
        QAbstractPrintDialog::AllPages
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return canceled();
    }

    if (!printer.isValid())
    {
        return failed(
            QObject::tr("No valid printer is available.")
            );
    }

    const QList<int> pages =
        selectedPrintPageIndexes(
            printer,
            pageCount,
            request.currentPageIndex
            );

    if (pages.isEmpty())
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

    for (qsizetype index = 0; index < pages.size(); ++index)
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
            pages.at(index);

        if (
            !renderPdfPageToPrinter(
                request.document,
                pageIndex,
                printer,
                painter
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
