#pragma once

#include "domain/models/document_output_result.h"

#include <QPageLayout>
#include <QPageSize>
#include <QString>
#include <QStringList>

#include <optional>

class QPdfDocument;
class QWidget;

namespace PdfPrintService
{
inline constexpr int GeneratedPdfResolutionDpi = 300;

using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

struct Request
{
    QWidget* parent = nullptr;
    QPdfDocument* document = nullptr;
    QString documentPath;
    int currentPageIndex = 0;
    QString dialogTitle;
    QPageLayout::Orientation pageOrientation = QPageLayout::Portrait;
    bool fitToPageByDefault = false;
    std::optional<QPageSize::PageSizeId> preferredPageSize;
    bool lockPreferredPageSize = false;
};

struct BatchRequest
{
    QWidget* parent = nullptr;
    QStringList documentPaths;
    QString dialogTitle;
    QPageLayout::Orientation pageOrientation = QPageLayout::Portrait;
    std::optional<QPageSize::PageSizeId> preferredPageSize;
    bool fitToPage = true;
};

[[nodiscard]] Result printPdfDocument(
    const Request& request
    );

// Opens one printer settings dialog, then submits all supplied PDFs as a
// single print job.  This is intended for batch reports, not the interactive
// single-document preview workflow above.
[[nodiscard]] Result printPdfDocuments(
    const BatchRequest& request
    );
}
