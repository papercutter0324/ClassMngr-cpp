#pragma once

#include "ui/shared/pages/basepage.h"

#include <QString>

class QLabel;
class QLineEdit;
class QIntValidator;
class QEvent;
class QPdfDocument;
class QPdfView;
class QPushButton;

struct PdfViewerDocumentDescriptor
{
    QString pdfFilePath;
    bool exportEnabled = false;
    QString exportFilePath;
    QString exportFileName;
    bool printEnabled = false;
};

class PdfViewerPage : public BasePage
{
    Q_OBJECT

public:
    explicit PdfViewerPage(
        QWidget* parent = nullptr
        );

    ~PdfViewerPage() override;

    void retranslateUi() override;
    [[nodiscard]] PageOutputCapabilities
        outputCapabilities() const override;
    void printCurrentPage() override;
    void saveCurrentPageAs() override;

    [[nodiscard]] bool loadPdf(
        const PdfViewerDocumentDescriptor& descriptor
        );
    void releaseDocument();

    [[nodiscard]] QString currentFilePath() const;
    [[nodiscard]] bool hasLoadedDocument() const;

    void setDocumentPageSpacing(
        DocumentPageSpacing spacing
        );

    void setDocumentViewerBackground(
        DocumentViewerBackground background
        );

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitWidth();
    void fitPage();

private slots:
    void applyZoomInput();
    void applyPageInput();
    void exportFile();
    void printFile();
    void handleDocumentStatusChanged();

protected:
    void changeEvent(
        QEvent* event
        ) override;

private:
    void buildUi();
    void applyUiFonts();
    void applyZoom();
    void updateZoomDisplay();
    void applyCalculatedFitZoom();
    void applyDocumentViewerBackground();
    [[nodiscard]] qreal effectiveZoomFactor() const;
    void updatePageDisplay();
    void updateDocumentActionButtons();
    [[nodiscard]] QString exportSourcePath() const;
    [[nodiscard]] bool copyFileTo(
        const QString& sourcePath,
        const QString& targetPath,
        QString* errorMessage
        ) const;
    void showStatusMessage(
        const QString& message
        );
    void clearStatusMessage();
    [[nodiscard]] QString documentErrorText() const;
    [[nodiscard]] QString documentErrorText(
        int error
        ) const;

private:
    bool m_tearingDown = false;
    bool m_documentReleased = true;
    QPdfDocument* m_document = nullptr;
    QPdfView* m_view = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_pageTotalLabel = nullptr;
    QLineEdit* m_pageInput = nullptr;
    QIntValidator* m_pageValidator = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLineEdit* m_zoomInput = nullptr;
    QPushButton* m_zoomOutButton = nullptr;
    QPushButton* m_zoomInButton = nullptr;
    QPushButton* m_fitWidthButton = nullptr;
    QPushButton* m_fitPageButton = nullptr;

    QString m_currentFilePath;
    qreal m_currentZoom = 1.0;
    PdfViewerDocumentDescriptor m_documentDescriptor;
    DocumentPageSpacing m_documentPageSpacing = DocumentPageSpacing::Small;
    DocumentViewerBackground m_documentViewerBackground =
        DocumentViewerBackground::Default;
};
