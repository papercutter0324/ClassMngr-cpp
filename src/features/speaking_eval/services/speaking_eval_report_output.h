#pragma once

#include <QString>
#include <QStringList>

namespace SpeakingEvalBatchReportService
{
struct Request;
}

namespace SpeakingEvalReportOutput
{
[[nodiscard]] bool targetFilePaths(
    const SpeakingEvalBatchReportService::Request& request,
    bool saveIndividualPdfFiles,
    QStringList* targetPaths,
    QString* errorMessage
    );

[[nodiscard]] bool commitFiles(
    const QStringList& stagedPaths,
    const QStringList& targetPaths,
    bool overwriteExisting,
    QString* errorMessage
    );
}
