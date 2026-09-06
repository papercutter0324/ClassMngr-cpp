#include "speaking_eval_ai_prompt.h"

#include "classmngr/engine/speaking_evaluation_ai_prompt.h"
#include "classmngr/engine/speaking_evaluation_report_model.h"

#include <vector>

namespace
{
using EngineAiVoice = classmngr::engine::SpeakingEvaluationAiVoice;
using EnginePromptService =
    classmngr::engine::SpeakingEvaluationAiPromptService;

EngineAiVoice toPortableVoice(AiCommentVoice voice)
{
    return voice == AiCommentVoice::ThirdPerson
        ? EngineAiVoice::ThirdPerson
        : EngineAiVoice::DirectToStudent;
}

classmngr::engine::SpeakingEvaluationAiPromptInput toPortableInput(
    const SpeakingEvalAiPromptInput& source
    )
{
    classmngr::engine::SpeakingEvaluationAiPromptInput result;
    result.grade = source.grade;
    result.englishName = source.englishName.toStdString();
    result.koreanName = source.koreanName.toStdString();
    result.didWell = source.didWell.toStdString();
    result.needsImprovement = source.needsImprovement.toStdString();
    result.voice = toPortableVoice(source.voice);
    return result;
}

classmngr::engine::SpeakingEvaluationAiBatchPromptInput toPortableInput(
    const SpeakingEvalAiBatchPromptInput& source
    )
{
    classmngr::engine::SpeakingEvaluationAiBatchPromptInput result;
    result.voice = toPortableVoice(source.voice);
    result.additionalNamesToRedact.reserve(
        static_cast<std::size_t>(source.additionalNamesToRedact.size())
        );
    for (const QString& name : source.additionalNamesToRedact)
    {
        result.additionalNamesToRedact.push_back(name.toStdString());
    }

    result.students.reserve(
        static_cast<std::size_t>(source.students.size())
        );
    for (const auto& student : source.students)
    {
        result.students.push_back(
            {
                student.id.toStdString(),
                student.grade,
                student.englishName.toStdString(),
                student.koreanName.toStdString(),
                student.didWell.toStdString(),
                student.needsImprovement.toStdString()
            }
            );
    }
    return result;
}

std::vector<std::string> toPortableIds(const QStringList& source)
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const QString& value : source)
    {
        result.push_back(value.toStdString());
    }
    return result;
}
} // namespace

int speakingEvalElementaryGrade(
    const QString& classGrade
    )
{
    return classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
        classGrade.toStdString()
        );
}

QStringList speakingEvalAiObservationItems(
    const QString& observations
    )
{
    QStringList result;
    const auto items = EnginePromptService::observationItems(
        observations.toStdString()
        );
    result.reserve(static_cast<qsizetype>(items.size()));
    for (const std::string& item : items)
    {
        result.append(QString::fromStdString(item));
    }
    return result;
}

bool canBuildSpeakingEvalAiPrompt(
    const SpeakingEvalAiPromptInput& input
    )
{
    return EnginePromptService::canBuildPrompt(toPortableInput(input));
}

QString buildSpeakingEvalAiCommentPrompt(
    const SpeakingEvalAiPromptInput& input
    )
{
    return QString::fromStdString(
        EnginePromptService::buildCommentPrompt(toPortableInput(input))
        );
}

QString buildSpeakingEvalAiBatchCommentPrompt(
    const SpeakingEvalAiBatchPromptInput& input
    )
{
    return QString::fromStdString(
        EnginePromptService::buildBatchCommentPrompt(toPortableInput(input))
        );
}

SpeakingEvalAiBatchParseResult
parseSpeakingEvalAiBatchResponse(
    const QString& response,
    const QStringList& expectedIds
    )
{
    const auto parsed = EnginePromptService::parseBatchResponse(
        response.toStdString(),
        toPortableIds(expectedIds)
        );

    SpeakingEvalAiBatchParseResult result;
    result.comments.reserve(static_cast<qsizetype>(parsed.comments.size()));
    for (const auto& comment : parsed.comments)
    {
        result.comments.append(
            {
                QString::fromStdString(comment.id),
                QString::fromStdString(comment.comment),
                comment.hadNamePlaceholder
            }
            );
    }
    result.duplicateIds.reserve(
        static_cast<qsizetype>(parsed.duplicateIds.size())
        );
    for (const std::string& id : parsed.duplicateIds)
    {
        result.duplicateIds.append(QString::fromStdString(id));
    }
    result.malformedIds.reserve(
        static_cast<qsizetype>(parsed.malformedIds.size())
        );
    for (const std::string& id : parsed.malformedIds)
    {
        result.malformedIds.append(QString::fromStdString(id));
    }
    result.unknownIds.reserve(
        static_cast<qsizetype>(parsed.unknownIds.size())
        );
    for (const std::string& id : parsed.unknownIds)
    {
        result.unknownIds.append(QString::fromStdString(id));
    }
    return result;
}
