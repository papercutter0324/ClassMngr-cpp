#pragma once

#include "domain/models/document_output_result.h"
#include "features/speaking_eval/services/speaking_eval_report_batch_types.h"
#include "features/speaking_eval/services/speaking_eval_powerpoint_automation_port.h"
#include "domain/models/class_info.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QWidget;
class ArchiveWritePort;
class PdfGenerationPort;

namespace SpeakingEvalBatchReportService
{

enum class Renderer
{
    Internal,
    PowerPoint
};

using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

struct Request
{
    QWidget* parent = nullptr;
    QList<StudentReport> reports;
    Renderer renderer = Renderer::Internal;
    bool savePdf = false;
    bool printReports = false;
    bool keepIndividualPdfFiles = false;
    bool overwriteExisting = false;
    QString outputDirectory;
    // An exact destination for a single-report save-as request. When set,
    // outputDirectory is ignored.
    QString outputFilePath;
    // Resolved by the caller while its catalog/legacy resource lease lives.
    QString documentsRoot;
    std::function<bool(int completed, int total, const QString& studentName)>
        progressCallback;
    // Replaces the printer operation when supplied. The staged PDFs remain
    // alive for the synchronous call; an empty callback keeps the Legacy
    // PdfPrintService path.
    std::function<Result(QWidget*, const QStringList&, const QString&)>
        printCallback;
    std::function<bool(const SpeakingEvalReportData&, const QString&, QString*)>
        internalRenderer;
    std::shared_ptr<SpeakingEvalPowerPointAutomationPort> powerPointAutomation;
    std::function<void(const QString&)> stagingDirectoryCreated;
};

[[nodiscard]] QString rendererDisplayName(
    Renderer renderer
    );

[[nodiscard]] QString safeFileName(
    const QString& englishName,
    const QString& koreanName
    );

[[nodiscard]] QString defaultOutputDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory = {}
    );

// Explicit-input variant for Next callers. The supplied Documents directory
// is used directly and no platform location query is performed.
[[nodiscard]] QString defaultOutputDirectoryFromDocumentsDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    );

[[nodiscard]] QString batchArchivePath(
    const QString& outputDirectory
    );

// Legacy composition conveniences. Explicit-resource/Next callers inject the port.
[[nodiscard]] bool isPowerPointRendererAvailable();

[[nodiscard]] QString powerPointRendererAvailabilityMessage();

[[nodiscard]] Result exportReports(
    const Request& request
    );

// Source-compatible Legacy composition using the ZIP archive adapter.
[[nodiscard]] Result exportResolvedReports(const Request& request);

// Qt-only engine entry. Resource paths and archive output are explicitly
// supplied. A null archive writer fails closed when a batch archive is needed.
[[nodiscard]] Result exportResolvedReports(
    const Request& request, ArchiveWritePort* archiveWriter);

[[nodiscard]] Result exportResolvedReports(
    const Request& request, ArchiveWritePort* archiveWriter, PdfGenerationPort* pdfGeneration);

} // namespace SpeakingEvalBatchReportService
