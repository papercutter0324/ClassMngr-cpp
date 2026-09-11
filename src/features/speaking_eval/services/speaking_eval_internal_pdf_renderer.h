#pragma once

#include "classmngr/engine/speaking_evaluation_report_content.h"

#include <QByteArray>
#include <QString>

class SpeakingEvalInternalPdfRenderer final
{
public:
    [[nodiscard]] static bool render(
        const classmngr::engine::SpeakingEvaluationReportContent& content,
        const QByteArray& signatureImage,
        const QString& documentPath,
        QString* errorMessage
        );
};
