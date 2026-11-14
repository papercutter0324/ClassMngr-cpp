#pragma once

#include "features/speaking_eval/services/speaking_eval_report_data.h"

#include <QString>

class PdfGenerationPort;

class SpeakingEvalInternalPdfRenderer final
{
public:
    [[nodiscard]] static bool render(
        PdfGenerationPort& pdfGeneration,
        const SpeakingEvalReportData& data,
        const QString& documentPath,
        QString* errorMessage);
    [[nodiscard]] static bool render(
        const SpeakingEvalReportData& data,
        const QString& documentPath,
        QString* errorMessage
        );
};
