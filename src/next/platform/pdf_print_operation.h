#pragma once

#include <QFont>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfDocument>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class QWidget;

namespace ClassMngr::Next::Platform
{

enum class PdfPrintStatus
{
    Sent,
    Canceled,
    Failed
};

struct PdfPrintResult final
{
    PdfPrintStatus status = PdfPrintStatus::Failed;
    QString message;
};

struct PrintDialogGeometryPreferences final
{
    using Read = std::function<std::string(std::string_view stableDialogKey)>;
    using Write = std::function<void(
        std::string_view stableDialogKey,
        std::string_view geometryPayload
        )>;

    Read read;
    Write write;
};

// A synchronous, borrowed document operation. The implementation must not
// retain, close, or take ownership of document after this call returns.
struct ReadyPdfPrintRequest final
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
    QFont uiFont;
    PrintDialogGeometryPreferences dialogGeometryPreferences;
};

// A separate operation for locally staged PDFs. Paths are consumed in the
// supplied order as one printer job; this request never represents a viewer
// session or a business-level PDF request.
struct LocalPdfBatchPrintRequest final
{
    QWidget* parent = nullptr;
    QStringList documentPaths;
    QString dialogTitle;
    QPageLayout::Orientation pageOrientation = QPageLayout::Portrait;
    std::optional<QPageSize::PageSizeId> preferredPageSize;
    bool fitToPage = true;
};

class PdfPrintOperation
{
public:
    virtual ~PdfPrintOperation() = default;

    [[nodiscard]] virtual PdfPrintResult printReadyDocument(
        const ReadyPdfPrintRequest& request
        ) = 0;
    [[nodiscard]] virtual PdfPrintResult printLocalDocuments(
        const LocalPdfBatchPrintRequest& request
        ) = 0;
};

using PdfPrintOperationFactory = std::function<
    std::unique_ptr<PdfPrintOperation>()>;

[[nodiscard]] std::unique_ptr<PdfPrintOperation> makeQtPdfPrintOperation();

} // namespace ClassMngr::Next::Platform
