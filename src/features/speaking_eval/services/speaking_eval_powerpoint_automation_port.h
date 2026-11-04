#pragma once

#include "domain/models/document_output_result.h"
#include "features/speaking_eval/services/speaking_eval_report_batch_types.h"
#include <QStringList>

struct SpeakingEvalPowerPointAvailability final
{
    bool available = false;
    QString message;
};

// All inputs are borrowed only for renderBatch. Output paths belong to the
// caller's staging directory; the implementation releases its private host
// workspace and process before returning.
struct SpeakingEvalPowerPointBatchRequest final
{
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports;
    QString documentsRoot;
    QStringList outputPaths;
    QString stagingDirectory;
    SpeakingEvalBatchReportService::ProgressCallback progressCallback;
};

class SpeakingEvalPowerPointAutomationPort
{
public:
    virtual ~SpeakingEvalPowerPointAutomationPort() = default;
    [[nodiscard]] virtual SpeakingEvalPowerPointAvailability availability() const = 0;
    [[nodiscard]] virtual DocumentOutputResult renderBatch(
        const SpeakingEvalPowerPointBatchRequest& request) = 0;
};
