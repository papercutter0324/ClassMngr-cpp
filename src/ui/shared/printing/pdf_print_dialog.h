#pragma once

#include "ui/shared/printing/pdf_print_service.h"

#include <QDialog>
#include <QList>
#include <QPageSize>
#include <QPrinter>
#include <QString>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPdfDocument;
class QPrintPreviewWidget;
class QPushButton;
class QSpinBox;

namespace PdfPrintDialogSupport
{
struct RenderOptions
{
    QList<int> pageIndexes;
    bool grayscale = false;
    bool fitToPage = false;
};

using RenderFunction =
    std::function<PdfPrintService::Result(QPrinter&, const RenderOptions&)>;
}

class PdfPrintDialog : public QDialog
{
public:
    PdfPrintDialog(
        QWidget* parent,
        QPdfDocument* document,
        const QString& documentPath,
        PdfPrintDialogSupport::RenderFunction renderFunction
        );

    [[nodiscard]] PdfPrintService::Result printResult() const;

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

private:
    void buildUi();
    void connectSignals();
    void populatePrinters();
    void configurePrinterFromUi();
    void updatePrinterCapabilities();
    void updatePageRangeControls();
    void updateValidation();
    void updatePreview();
    void printDocument();
    void handlePreviewPaintRequested(
        QPrinter* printer
        );

    [[nodiscard]] QString documentDisplayName() const;
    [[nodiscard]] QString printJobTitle() const;
    [[nodiscard]] QList<int> allPageIndexes() const;
    [[nodiscard]] QList<int> selectedPageIndexes(
        bool* ok,
        QString* errorMessage
        ) const;
    [[nodiscard]] PdfPrintDialogSupport::RenderOptions renderOptions(
        bool* ok,
        QString* errorMessage
        ) const;
    [[nodiscard]] bool selectedPrinterSupportsDuplex() const;

    QPdfDocument* m_document = nullptr;
    QString m_documentPath;
    PdfPrintDialogSupport::RenderFunction m_renderFunction;
    PdfPrintService::Result m_printResult;
    QPrinter m_printer;

    QComboBox* m_printerCombo = nullptr;
    QComboBox* m_pagesCombo = nullptr;
    QLineEdit* m_customPagesEdit = nullptr;
    QSpinBox* m_copiesSpin = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_paperSizeCombo = nullptr;
    QCheckBox* m_twoSidedCheck = nullptr;
    QCheckBox* m_fitToPageCheck = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPrintPreviewWidget* m_previewWidget = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    bool m_hasPrinters = false;
};
