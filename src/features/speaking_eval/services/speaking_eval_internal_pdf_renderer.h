#pragma once

#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QString>

class SpeakingEvalInternalPdfRenderer final
{
public:
    [[nodiscard]] static bool render(
        const SpeakingEvalReportData& data,
        const QString& documentPath,
        QString* errorMessage
        );
};
