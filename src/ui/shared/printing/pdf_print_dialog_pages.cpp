#include "ui/shared/printing/pdf_print_dialog.h"

#include "ui/shared/printing/pdf_print_dialog_internal.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPdfDocument>
#include <QStringList>
#include <QVariant>

#include <algorithm>

QList<int> PdfPrintDialog::allPageIndexes() const
{
    QList<int> pages;

    const int pageCount =
        m_document ? m_document->pageCount() : 0;
    pages.reserve(pageCount);

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        pages.append(pageIndex);
    }

    return pages;
}

#ifdef Q_OS_WIN
QList<int> PdfPrintDialog::pageIndexesFromNativePrinterRange(
    const QPrinter& printer
    ) const
{
    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    if (pageCount <= 0)
    {
        return {};
    }

    if (printer.printRange() == QPrinter::CurrentPage)
    {
        return {
            std::clamp(
                m_currentPageIndex,
                0,
                pageCount - 1
                )
        };
    }

    if (printer.printRange() != QPrinter::PageRange)
    {
        return allPageIndexes();
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
        return allPageIndexes();
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
#endif

QList<int> PdfPrintDialog::selectedPageIndexes(
    bool* ok,
    QString* errorMessage
    ) const
{
    if (ok)
    {
        *ok =
            true;
    }

    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    if (pageCount <= 0)
    {
        if (ok)
        {
            *ok =
                false;
        }
        if (errorMessage)
        {
            *errorMessage =
                tr("No PDF pages are available.");
        }
        return {};
    }

    if (
        m_pagesCombo->currentData().toInt()
            != PdfPrintDialogPrivate::PageRangeCustom
        )
    {
        return allPageIndexes();
    }

    const QString text =
        m_customPagesEdit->text().trimmed();

    if (
        text.isEmpty()
        || text == PdfPrintDialogPrivate::customPagesSample()
        )
    {
        return allPageIndexes();
    }

    for (QChar character : text)
    {
        if (!PdfPrintDialogPrivate::isAllowedPageRangeCharacter(character))
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Use commas between pages and hyphens only in ranges.");
            }
            return {};
        }
    }

    QList<int> pages;
    const QStringList segments =
        text.split(
            QLatin1Char(',')
            );

    for (const QString& segmentText : segments)
    {
        const QString segment =
            segmentText.trimmed();

        if (segment.isEmpty())
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Enter page ranges like 1-3, 6, 9-11.");
            }
            return {};
        }

        const qsizetype hyphenCount =
            segment.count(
                QLatin1Char('-')
                );

        if (hyphenCount > 1)
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Use one hyphen per page range.");
            }
            return {};
        }

        if (hyphenCount == 0)
        {
            bool pageOk = false;
            const int pageNumber =
                segment.toInt(&pageOk);

            if (
                !pageOk
                || pageNumber < 1
                || pageNumber > pageCount
                )
            {
                if (ok)
                {
                    *ok =
                        false;
                }
                if (errorMessage)
                {
                    *errorMessage =
                        tr("Use page numbers from 1 to %1.")
                            .arg(pageCount);
                }
                return {};
            }

            pages.append(pageNumber - 1);
            continue;
        }

        const QStringList bounds =
            segment.split(
                QLatin1Char('-')
                );

        bool startOk = false;
        bool endOk = false;
        const int startPage =
            bounds.value(0).trimmed().toInt(&startOk);
        const int endPage =
            bounds.value(1).trimmed().toInt(&endOk);

        if (
            !startOk
            || !endOk
            || startPage < 1
            || endPage < 1
            || startPage > pageCount
            || endPage > pageCount
            || startPage > endPage
            )
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Use valid page ranges from 1 to %1.")
                        .arg(pageCount);
            }
            return {};
        }

        for (int pageNumber = startPage; pageNumber <= endPage; ++pageNumber)
        {
            pages.append(pageNumber - 1);
        }
    }

    if (pages.isEmpty())
    {
        return allPageIndexes();
    }

    return pages;
}

PdfPrintDialogSupport::RenderOptions PdfPrintDialog::renderOptions(
    bool* ok,
    QString* errorMessage
    ) const
{
    PdfPrintDialogSupport::RenderOptions options;
    options.pageIndexes =
        selectedPageIndexes(
            ok,
            errorMessage
            );
    options.grayscale =
        m_colorCombo->currentData().toInt()
            == PdfPrintDialogPrivate::ColorModeBlackAndWhite;
    options.fitToPage =
        m_fitToPageCheck->isChecked();

    return options;
}
