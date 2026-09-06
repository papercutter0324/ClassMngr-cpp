#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class SpeakingEvaluationAiVoice
{
    DirectToStudent,
    ThirdPerson
};

struct SpeakingEvaluationAiPromptInput
{
    int grade = 0;
    std::string englishName;
    std::string koreanName;
    std::string didWell;
    std::string needsImprovement;
    SpeakingEvaluationAiVoice voice =
        SpeakingEvaluationAiVoice::DirectToStudent;
};

struct SpeakingEvaluationAiBatchStudentInput
{
    std::string id;
    int grade = 0;
    std::string englishName;
    std::string koreanName;
    std::string didWell;
    std::string needsImprovement;
};

struct SpeakingEvaluationAiBatchPromptInput
{
    std::vector<SpeakingEvaluationAiBatchStudentInput> students;
    std::vector<std::string> additionalNamesToRedact;
    SpeakingEvaluationAiVoice voice =
        SpeakingEvaluationAiVoice::DirectToStudent;
};

struct SpeakingEvaluationAiBatchComment
{
    std::string id;
    std::string comment;
    bool hadNamePlaceholder = false;
};

struct SpeakingEvaluationAiBatchParseResult
{
    std::vector<SpeakingEvaluationAiBatchComment> comments;
    std::vector<std::string> duplicateIds;
    std::vector<std::string> malformedIds;
    std::vector<std::string> unknownIds;
};

class SpeakingEvaluationAiPromptService final
{
public:
    [[nodiscard]] static std::vector<std::string> observationItems(
        std::string_view observations
        );

    [[nodiscard]] static bool canBuildPrompt(
        const SpeakingEvaluationAiPromptInput& input
        );

    [[nodiscard]] static std::string buildCommentPrompt(
        const SpeakingEvaluationAiPromptInput& input
        );

    [[nodiscard]] static std::string buildBatchCommentPrompt(
        const SpeakingEvaluationAiBatchPromptInput& input
        );

    [[nodiscard]] static SpeakingEvaluationAiBatchParseResult
    parseBatchResponse(
        std::string_view response,
        const std::vector<std::string>& expectedIds
        );
};

} // namespace classmngr::engine
