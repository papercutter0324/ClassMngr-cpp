#pragma once

#include "domain/models/document_output_result.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"
#include "domain/models/class_info.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

namespace SpeakingEvalBatchReportService
{

enum class Renderer
{
    Internal,
    PowerPoint
};

using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

struct StudentReport
{
    QString displayName;
    SpeakingEvalReportData report;
    int sourceRow = -1;
};

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
    std::function<bool(int completed, int total, const QString& studentName)>
        progressCallback;
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

[[nodiscard]] QString batchArchivePath(
    const QString& outputDirectory
    );

[[nodiscard]] bool isPowerPointRendererAvailable();

[[nodiscard]] QString powerPointRendererAvailabilityMessage();

[[nodiscard]] Result exportReports(
    const Request& request
    );

} // namespace SpeakingEvalBatchReportService
