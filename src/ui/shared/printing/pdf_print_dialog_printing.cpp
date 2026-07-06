#include "ui/shared/printing/pdf_print_dialog.h"

#include "ui/shared/printing/pdf_print_dialog_internal.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPageSize>
#include <QPdfDocument>
#ifdef Q_OS_WIN
#include <QPrintDialog>
#endif
#include <QPrintPreviewWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVariant>

void PdfPrintDialog::updateValidation()
{
#ifdef Q_OS_WIN
    if (m_nativePrintButton)
    {
        m_nativePrintButton->setEnabled(m_hasPrinters);
    }
#endif

    bool ok = false;
    QString errorMessage;
    [[maybe_unused]] const QList<int> selectedPages =
        selectedPageIndexes(
        &ok,
        &errorMessage
        );

    if (!m_hasPrinters)
    {
        m_statusLabel->setText(
            tr("No printers are available.")
            );
        m_printButton->setEnabled(false);
        return;
    }

    if (!ok)
    {
        m_statusLabel->setText(errorMessage);
        m_printButton->setEnabled(false);
        return;
    }

    m_statusLabel->clear();
    m_printButton->setEnabled(true);
}

void PdfPrintDialog::updatePreview()
{
    configurePrinterFromUi();

    if (m_previewWidget)
    {
        m_previewWidget->updatePreview();
    }
}

void PdfPrintDialog::printDocument()
{
    bool ok = false;
    QString errorMessage;
    const PdfPrintDialogSupport::RenderOptions options =
        renderOptions(
            &ok,
            &errorMessage
            );

    if (!ok)
    {
        m_statusLabel->setText(errorMessage);
        m_printButton->setEnabled(false);
        return;
    }

    configurePrinterFromUi();

    if (!m_printer.isValid())
    {
        m_statusLabel->setText(
            tr("No valid printer is available.")
            );
        return;
    }

    m_printResult =
        m_renderFunction(
            m_printer,
            options
            );

    if (m_printResult.status == PdfPrintService::Status::Failed)
    {
        m_statusLabel->setText(m_printResult.message);
        return;
    }

    accept();
}

#ifdef Q_OS_WIN
void PdfPrintDialog::printWithNativeSystem()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::NativeFormat);

    if (m_hasPrinters)
    {
        printer.setPrinterName(
            m_printerCombo->currentData().toString()
            );
    }

    if (!printer.isValid())
    {
        m_statusLabel->setText(
            tr("No valid printer is available.")
            );
        return;
    }

    printer.setDocName(
        printJobTitle()
        );
    printer.setCreator(
        QStringLiteral("ClassMngr")
        );
    printer.setCopyCount(
        m_copiesSpin->value()
        );
    printer.setColorMode(
        m_colorCombo->currentData().toInt()
                == PdfPrintDialogPrivate::ColorModeBlackAndWhite
            ? QPrinter::GrayScale
            : QPrinter::Color
        );
    printer.setDuplex(
        m_twoSidedCheck->isChecked()
            ? QPrinter::DuplexLongSide
            : QPrinter::DuplexNone
        );

    const auto pageSizeId =
        static_cast<QPageSize::PageSizeId>(
            m_paperSizeCombo->currentData().toInt()
            );
    printer.setPageSize(
        QPageSize(pageSizeId)
        );
    printer.setPageOrientation(
        m_pageOrientation
        );

    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    QPrintDialog dialog(
        &printer,
        this
        );
    dialog.setWindowTitle(
        printJobTitle()
        );
    dialog.setMinMax(
        1,
        pageCount
        );
    dialog.setFromTo(
        1,
        pageCount
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    if (
        m_lockPreferredPageSize
        && m_preferredPageSize
        )
    {
        printer.setPageSize(
            QPageSize(*m_preferredPageSize)
            );
        printer.setPageOrientation(
            m_pageOrientation
            );
    }

    PdfPrintDialogSupport::RenderOptions options;
    options.pageIndexes =
        pageIndexesFromNativePrinterRange(
            printer
            );
    options.grayscale =
        printer.colorMode() == QPrinter::GrayScale;
    options.fitToPage =
        m_fitToPageCheck->isChecked();

    m_printResult =
        m_renderFunction(
            printer,
            options
            );

    if (m_printResult.status == PdfPrintService::Status::Failed)
    {
        m_statusLabel->setText(m_printResult.message);
        return;
    }

    accept();
}
#endif

void PdfPrintDialog::handlePreviewPaintRequested(
    QPrinter* printer
    )
{
    if (!printer)
    {
        return;
    }

    if (!m_hasPrinters)
    {
        return;
    }

    bool ok = false;
    QString errorMessage;
    PdfPrintDialogSupport::RenderOptions options =
        renderOptions(
            &ok,
            &errorMessage
            );

    if (!ok)
    {
        options.pageIndexes =
            allPageIndexes();
    }

    const PdfPrintService::Result result =
        m_renderFunction(
            *printer,
            options
            );

    if (result.status == PdfPrintService::Status::Failed)
    {
        m_statusLabel->setText(result.message);
    }
}
