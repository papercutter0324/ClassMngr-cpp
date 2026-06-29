#pragma once

#include <QString>

class QPdfDocument;
class QWidget;

namespace PdfPrintService
{
enum class Status
{
    Sent,
    Canceled,
    Failed
};

struct Request
{
    QWidget* parent = nullptr;
    QPdfDocument* document = nullptr;
    QString documentPath;
    int currentPageIndex = 0;
    QString dialogTitle;
};

struct Result
{
    Status status = Status::Failed;
    QString message;
};

[[nodiscard]] Result printPdfDocument(
    const Request& request
    );
}
