#include "classmngr/engine/speaking_evaluation_ai_prompt.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::SpeakingEvaluationAiBatchPromptInput;
using classmngr::engine::SpeakingEvaluationAiPromptInput;
using classmngr::engine::SpeakingEvaluationAiPromptService;
using classmngr::engine::SpeakingEvaluationAiVoice;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationAiPromptTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        SpeakingEvaluationAiPromptService::observationItems(
            "  * Strong vocabulary\r\n"
            "\xE2\x80\xA2 Clear pronunciation\r"
            "\n"
            "\n"
            "- Eye contact"
            )
            == std::vector<std::string>{
                "Strong vocabulary",
                "Clear pronunciation",
                "Eye contact"
            },
        "observation line normalization changed"
        );

    SpeakingEvaluationAiPromptInput promptInput;
    promptInput.grade = 5;
    promptInput.englishName = "Alice";
    promptInput.koreanName =
        "\xEA\xB9\x80" "\xEB\xAF\xBC" "\xEC\xA7\x80";
    promptInput.didWell =
        "Alice showed strong vocabulary\n"
        "\xEA\xB9\x80" "\xEB\xAF\xBC" "\xEC\xA7\x80"
        " maintained eye contact";
    promptInput.needsImprovement = "Clear pronunciation";

    passed &= expect(
        SpeakingEvaluationAiPromptService::canBuildPrompt(promptInput),
        "eligible prompt input was rejected"
        );
    const std::string prompt =
        SpeakingEvaluationAiPromptService::buildCommentPrompt(promptInput);
    passed &= expect(
        prompt.find("5th-grade elementary ESL student")
                != std::string::npos
            && prompt.find("- STD_NAME showed strong vocabulary")
                != std::string::npos
            && prompt.find("- STD_NAME maintained eye contact")
                != std::string::npos
            && prompt.find("Alice") == std::string::npos
            && prompt.find(
                "\xEA\xB9\x80" "\xEB\xAF\xBC" "\xEC\xA7\x80"
                ) == std::string::npos,
        "single-student prompt did not redact names"
        );

    promptInput.grade = 3;
    passed &= expect(
        !SpeakingEvaluationAiPromptService::canBuildPrompt(promptInput)
            && SpeakingEvaluationAiPromptService::buildCommentPrompt(
                promptInput
                ).empty(),
        "unsupported grade produced a prompt"
        );

    SpeakingEvaluationAiBatchPromptInput batchInput;
    batchInput.voice = SpeakingEvaluationAiVoice::ThirdPerson;
    batchInput.students = {
        {
            "STUDENT_01",
            4,
            "Alice",
            "학생이름1",
            "Alice used strong vocabulary",
            "Alice should add supporting details"
        },
        {
            "STUDENT_02",
            6,
            "Bob",
            "학생이름2",
            "Bob maintained eye contact",
            "Bob should extend answers"
        }
    };
    batchInput.students[0].didWell += "\nBob collaborated helpfully";

    const std::string batchPrompt =
        SpeakingEvaluationAiPromptService::buildBatchCommentPrompt(
            batchInput
            );
    passed &= expect(
        batchPrompt.find("Write for a parent or guardian")
                != std::string::npos
            && batchPrompt.find("<<<STUDENT_01>>>") != std::string::npos
            && batchPrompt.find("<<<END_STUDENT_02>>>")
                != std::string::npos
            && batchPrompt.find("Alice") == std::string::npos
            && batchPrompt.find("Bob") == std::string::npos
            && batchPrompt.find("학생이름1") == std::string::npos
            && batchPrompt.find("학생이름2") == std::string::npos
            && batchPrompt.find("CLASSMATE collaborated helpfully")
                != std::string::npos,
        "batch prompt did not anonymize records"
        );

    batchInput.students[1].id = "STUDENT_01";
    passed &= expect(
        SpeakingEvaluationAiPromptService::buildBatchCommentPrompt(
            batchInput
            ).empty(),
        "duplicate batch IDs were accepted"
        );

    const auto parseResult =
        SpeakingEvaluationAiPromptService::parseBatchResponse(
            "The requested comments follow.\n"
            "```text\n"
            "<<<STUDENT_02>>>\n"
            "Great work, STD_NAME.\n"
            "<<<END_STUDENT_02>>>\n"
            "<<<STUDENT_01>>>\n"
            "First duplicate.\n"
            "<<<END_STUDENT_01>>>\n"
            "<<<STUDENT_99>>>\n"
            "Unknown student.\n"
            "<<<END_STUDENT_99>>>\n"
            "<<<STUDENT_01>>>\n"
            "Second duplicate.\n"
            "<<<END_STUDENT_01>>>\n"
            "<<<STUDENT_03>>>\n"
            "This block is truncated.\n"
            "```",
            {
                "STUDENT_01",
                "STUDENT_02",
                "STUDENT_03",
                "STUDENT_04"
            }
            );
    passed &= expect(
        parseResult.comments.size() == 1
            && parseResult.comments.front().id == "STUDENT_02"
            && parseResult.comments.front().comment
                == "Great work, STD_NAME."
            && parseResult.comments.front().hadNamePlaceholder
            && parseResult.duplicateIds
                == std::vector<std::string>{ "STUDENT_01" }
            && parseResult.malformedIds
                == std::vector<std::string>{ "STUDENT_03" }
            && parseResult.unknownIds
                == std::vector<std::string>{ "STUDENT_99" },
        "batch response parsing changed"
        );

    return passed ? 0 : 1;
}
