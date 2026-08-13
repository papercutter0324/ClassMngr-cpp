#pragma once

#include "ui/shared/dialogs/dialog_shell.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QList>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <optional>

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

class PdfPrintDialog : public DialogShell
{
    Q_OBJECT

public:
    PdfPrintDialog(
        QWidget* parent,
        QPdfDocument* document,
        const QString& documentPath,
        PdfPrintDialogSupport::RenderFunction renderFunction,
        int currentPageIndex = 0,
        QPageLayout::Orientation pageOrientation = QPageLayout::Portrait,
        bool fitToPageByDefault = false,
        std::optional<QPageSize::PageSizeId> preferredPageSize = std::nullopt,
        bool lockPreferredPageSize = false
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
#ifdef Q_OS_WIN
    void printWithNativeSystem();
#endif
    void handlePreviewPaintRequested(
        QPrinter* printer
        );

    [[nodiscard]] QString documentDisplayName() const;
    [[nodiscard]] QString printJobTitle() const;
    [[nodiscard]] QList<int> allPageIndexes() const;
#ifdef Q_OS_WIN
    [[nodiscard]] QList<int> pageIndexesFromNativePrinterRange(
        const QPrinter& printer
        ) const;
#endif
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
    int m_currentPageIndex = 0;
    QPageLayout::Orientation m_pageOrientation = QPageLayout::Portrait;
    bool m_fitToPageByDefault = false;
    std::optional<QPageSize::PageSizeId> m_preferredPageSize;
    bool m_lockPreferredPageSize = false;
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
#ifdef Q_OS_WIN
    QPushButton* m_nativePrintButton = nullptr;
#endif
    QPushButton* m_cancelButton = nullptr;

    bool m_hasPrinters = false;
};
