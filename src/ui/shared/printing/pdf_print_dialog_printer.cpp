#include "ui/shared/printing/pdf_print_dialog.h"

#include "ui/shared/printing/pdf_print_dialog_internal.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPageSize>
#include <QPrinterInfo>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVariant>

void PdfPrintDialog::populatePrinters()
{
    const QList<QPrinterInfo> printers =
        QPrinterInfo::availablePrinters();

    m_hasPrinters =
        !printers.isEmpty();

    if (!m_hasPrinters)
    {
        m_printerCombo->addItem(
            tr("No printers available")
            );
        m_printerCombo->setEnabled(false);
        return;
    }

    const QString defaultPrinterName =
        QPrinterInfo::defaultPrinterName();
    int defaultIndex =
        0;

    for (const QPrinterInfo& printerInfo : printers)
    {
        const int itemIndex =
            m_printerCombo->count();
        m_printerCombo->addItem(
            printerInfo.printerName(),
            printerInfo.printerName()
            );

        if (
            printerInfo.isDefault()
            || printerInfo.printerName() == defaultPrinterName
            )
        {
            defaultIndex =
                itemIndex;
        }
    }

    m_printerCombo->setCurrentIndex(defaultIndex);
}

void PdfPrintDialog::configurePrinterFromUi()
{
    if (m_hasPrinters)
    {
        m_printer.setPrinterName(
            m_printerCombo->currentData().toString()
            );
    }

    m_printer.setDocName(
        printJobTitle()
        );
    m_printer.setCreator(
        QStringLiteral("ClassMngr")
        );
    m_printer.setCopyCount(
        m_copiesSpin->value()
        );
    m_printer.setColorMode(
        m_colorCombo->currentData().toInt()
                == PdfPrintDialogPrivate::ColorModeBlackAndWhite
            ? QPrinter::GrayScale
            : QPrinter::Color
        );
    m_printer.setDuplex(
        m_twoSidedCheck->isChecked()
            ? QPrinter::DuplexLongSide
            : QPrinter::DuplexNone
        );

    const auto pageSizeId =
        static_cast<QPageSize::PageSizeId>(
            m_paperSizeCombo->currentData().toInt()
            );
    m_printer.setPageSize(
        QPageSize(pageSizeId)
        );
}

void PdfPrintDialog::updatePrinterCapabilities()
{
    if (!m_hasPrinters)
    {
        m_twoSidedCheck->setChecked(false);
        m_twoSidedCheck->setEnabled(false);
        return;
    }

    const bool supportsDuplex =
        selectedPrinterSupportsDuplex();

    if (!supportsDuplex)
    {
        const QSignalBlocker blocker(m_twoSidedCheck);
        m_twoSidedCheck->setChecked(false);
    }

    m_twoSidedCheck->setEnabled(supportsDuplex);
    configurePrinterFromUi();
}

bool PdfPrintDialog::selectedPrinterSupportsDuplex() const
{
    if (!m_hasPrinters)
    {
        return false;
    }

    const QPrinterInfo printerInfo =
        QPrinterInfo::printerInfo(
            m_printerCombo->currentData().toString()
            );

    const QList<QPrinter::DuplexMode> modes =
        printerInfo.supportedDuplexModes();

    return modes.contains(QPrinter::DuplexLongSide)
        || modes.contains(QPrinter::DuplexAuto);
}
