#include "speaking_eval_report_data_assembler.h"

#include "classmngr/engine/speaking_evaluation_report_service.h"

QString SpeakingEvalReportDataAssembler::overallGrade(
    const std::array<QString, 6>& scores
    )
{
    classmngr::engine::SpeakingEvaluationScores portableScores;
    for (std::size_t index = 0; index < scores.size(); ++index)
    {
        portableScores[index] = scores[index].toStdString();
    }

    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportService::overallGrade(
            portableScores
            )
        );
}
