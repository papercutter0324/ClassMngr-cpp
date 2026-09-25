#include "speaking_eval_report_data_assembler.h"

#include "next/domain/speaking_evaluation_grade.h"

#include <QByteArray>

#include <array>
#include <string_view>

namespace NextDomain = ClassMngr::Next::Domain;

QString SpeakingEvalReportDataAssembler::overallGrade(
    const std::array<QString, 6>& scores
    )
{
    NextDomain::SpeakingEvaluationComponentScores componentScores{};
    for (std::size_t index = 0; index < scores.size(); ++index)
    {
        const QByteArray label = scores[index].toLatin1();
        componentScores[index] =
            NextDomain::speakingEvaluationGradeFromLabel(
                std::string_view(
                    label.constData(),
                    static_cast<std::size_t>(label.size())
                    )
            );
    }

    const auto overallGrade =
        NextDomain::calculateOverallSpeakingEvaluationGrade(componentScores);
    if (!overallGrade)
    {
        return QStringLiteral("N/A");
    }

    const std::string_view label =
        NextDomain::speakingEvaluationGradeLabel(*overallGrade);
    return QString::fromLatin1(
        label.data(),
        static_cast<qsizetype>(label.size())
        );
}
