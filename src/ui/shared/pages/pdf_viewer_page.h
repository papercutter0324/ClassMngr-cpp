#pragma once

#include "ui/shared/pages/basepage.h"

#include <QString>

class QLabel;
class QLineEdit;
class QIntValidator;
class QPdfDocument;
class QPdfView;
class QPushButton;

class PdfViewerPage : public BasePage
{
    Q_OBJECT

public:
    explicit PdfViewerPage(
        QWidget* parent = nullptr
        );

    [[nodiscard]] bool loadPdf(
        const QString& filePath
        );

    [[nodiscard]] QString currentFilePath() const;

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitWidth();
    void fitPage();

private slots:
    void applyZoomInput();
    void applyPageInput();
    void handleDocumentStatusChanged();

private:
    void buildUi();
    void applyZoom();
    void updateZoomDisplay();
    void updatePageDisplay();
    void showStatusMessage(
        const QString& message
        );
    void clearStatusMessage();
    [[nodiscard]] QString documentErrorText() const;
    [[nodiscard]] QString documentErrorText(
        int error
        ) const;

private:
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
};
