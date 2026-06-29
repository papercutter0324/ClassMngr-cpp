#pragma once

#include "ui/shared/pages/basepage.h"

#include <QString>

class QLabel;
class QLineEdit;
class QIntValidator;
class QPdfDocument;
class QPdfView;
class QPushButton;

struct PdfViewerDocumentActions
{
    bool exportEnabled = false;
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

    [[nodiscard]] bool loadPdf(
        const QString& filePath,
        PdfViewerDocumentActions actions = {}
        );

    [[nodiscard]] QString currentFilePath() const;

    void setDocumentPageSpacing(
        DocumentPageSpacing spacing
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

private:
    void buildUi();
    void applyZoom();
    void updateZoomDisplay();
    void applyCalculatedFitZoom();
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
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_printButton = nullptr;

    QString m_currentFilePath;
    qreal m_currentZoom = 1.0;
    PdfViewerDocumentActions m_documentActions;
    DocumentPageSpacing m_documentPageSpacing = DocumentPageSpacing::Small;
};
