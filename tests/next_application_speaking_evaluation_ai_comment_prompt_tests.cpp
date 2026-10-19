#include "next/application/speaking_evaluation_ai_comment_prompt.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

char16_t lowerAscii(const char16_t value)
{
    return value >= u'A' && value <= u'Z'
        ? static_cast<char16_t>(value + (u'a' - u'A'))
        : value;
}

class AsciiTextMatcher final : public SpeakingEvaluationTextMatchPort
{
public:
    std::vector<SpeakingEvaluationTextMatchSpan>
    findLiteralCaseInsensitiveMatches(
        const std::u16string_view text,
        const std::u16string_view literal
        ) const override
    {
        std::vector<SpeakingEvaluationTextMatchSpan> matches;
        if (literal.empty() || literal.size() > text.size())
        {
            return matches;
        }
        std::size_t cursor = 0;
        while (cursor + literal.size() <= text.size())
        {
            bool equal = true;
            for (std::size_t index = 0; index < literal.size(); ++index)
            {
                if (lowerAscii(text[cursor + index])
                    != lowerAscii(literal[index]))
                {
                    equal = false;
                    break;
                }
            }
            if (equal)
            {
                matches.push_back({cursor, literal.size()});
                cursor += literal.size();
            }
            else
            {
                ++cursor;
            }
        }
        return matches;
    }

    bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (lowerAscii(left[index]) != lowerAscii(right[index]))
            {
                return false;
            }
        }
        return true;
    }
};

bool check(const bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

std::u16string expectedCommonRequirements(const AiCommentVoice voice)
{
    const std::u16string voiceInstruction =
        voice == AiCommentVoice::ThirdPerson
            ? u"Write for a parent or guardian, use STD_NAME, and use "
              u"they/their rather than guessing gender."
            : u"Address the student directly as \"you\" and use STD_NAME "
              u"naturally.";
    return u"- Write one paragraph of exactly 3 short sentences between 100 "
        u"and 420 characters, including spaces.\n"
        u"- When the submitted notes provide enough relevant detail, aim "
        u"for 300 to 350 characters. Otherwise, stay within the required "
        u"range without inventing or repeating information.\n"
        u"- Use simple, common English that an elementary ESL student "
        u"and a parent with limited English can understand.\n"
        u"- Avoid long sentences, complex clauses, idioms, and uncommon "
        u"words.\n"
        u"- Sentence 1 must give positive feedback, sentence 2 must give "
        u"constructive advice, and sentence 3 must give positive "
        u"feedback.\n"
        u"- Sentences 1 and 3 must emphasize different skill categories. "
        u"If sentence 1 emphasizes presentation skills, sentence 3 must "
        u"emphasize grammar-related skills. If sentence 1 emphasizes "
        u"grammar-related skills, sentence 3 must emphasize presentation "
        u"skills.\n"
        u"- Aim to praise at least two submitted skills in each positive "
        u"sentence. If one category does not contain two suitable items, "
        u"supplement it with another submitted strength from the Did well "
        u"list. Never invent a skill to reach two items.\n"
        u"- Select only the most useful observations. You do not need "
        u"to mention every item, and do not cram several items into one "
        u"sentence.\n"
        u"- End with a short final sentence that praises specific items "
        u"from the Did well list.\n"
        u"- Use a warm, supportive, professional, and age-appropriate "
        u"tone.\n"
        u"- Use only the submitted observations; do not invent "
        u"abilities, scores, or events.\n"
        u"- Include the exact placeholder STD_NAME at least once. "
        u"Do not alter or replace it.\n- "
        + voiceInstruction
        + u"\n- Do not use headings, bullet points, quotation marks, or "
          u"a character-count annotation.";
}

bool observationsNormalizeLineEndingsAndRepeatedPrefixes()
{
    const auto items = speakingEvaluationAiObservationItems(
        u" \u2022 - * Alice did well\r\n\r\n-- Clear ideas\r*\tPractice\n---"
        );
    return check(
        items == std::vector<std::u16string>{
            u"Alice did well",
            u"Clear ideas",
            u"Practice"
        },
        "Observation normalization changed line endings, prefixes, or blank-line handling."
        );
}

bool elementaryGradeConversionPreservesLegacyAcceptedForms()
{
    const std::u16string overflow(100, u'9');
    const std::u16string leadingZeroes = std::u16string(100, u'0') + u"5";
    return check(
        speakingEvaluationElementaryGrade(u"E4") == 4
            && speakingEvaluationElementaryGrade(u"e6") == 6
            && speakingEvaluationElementaryGrade(u" +5 ") == 5
            && speakingEvaluationElementaryGrade(leadingZeroes) == 5
            && speakingEvaluationElementaryGrade(overflow) == 0
            && speakingEvaluationElementaryGrade(u"M1") == 0
            && speakingEvaluationElementaryGrade(u"E7") == 0,
        "Elementary grade conversion changed accepted values or overflow handling."
        );
}

bool singlePromptTextAndVoiceAreExact()
{
    const AsciiTextMatcher matcher;
    SpeakingEvaluationAiCommentPromptRequest request;
    request.grade = 5;
    request.didWell = u"Strong vocabulary\r\n\n- Clear ideas";
    request.needsImprovement = u"Use examples\n* Practice pronunciation";

    const auto direct = buildSpeakingEvaluationAiCommentPrompt(request, matcher);
    if (!check(direct.valid, "A valid single prompt was rejected."))
    {
        return false;
    }
    const std::u16string directExpected =
        u"Write one polished speaking-evaluation comment for a 5th-grade "
        u"elementary ESL student.\n\nRequirements:\n"
        + expectedCommonRequirements(AiCommentVoice::DirectToStudent)
        + u"\n- Return only the finished comment.\n\nDid well:\n"
          u"- Strong vocabulary\n- Clear ideas\n\nNeeds improvement:\n"
          u"- Use examples\n- Practice pronunciation";
    if (!check(direct.prompt == directExpected,
            "The direct-voice single prompt text changed."))
    {
        return false;
    }

    request.voice = AiCommentVoice::ThirdPerson;
    const auto thirdPerson = buildSpeakingEvaluationAiCommentPrompt(request, matcher);
    const std::u16string thirdPersonExpected =
        u"Write one polished speaking-evaluation comment for a 5th-grade "
        u"elementary ESL student.\n\nRequirements:\n"
        + expectedCommonRequirements(AiCommentVoice::ThirdPerson)
        + u"\n- Return only the finished comment.\n\nDid well:\n"
          u"- Strong vocabulary\n- Clear ideas\n\nNeeds improvement:\n"
          u"- Use examples\n- Practice pronunciation";
    return check(
        thirdPerson.valid && thirdPerson.prompt == thirdPersonExpected,
        "The third-person single prompt text changed."
        );
}

bool singlePromptKeepsIntegerGradesAndOrdinalWording()
{
    const AsciiTextMatcher matcher;
    SpeakingEvaluationAiCommentPromptRequest request;
    request.didWell = u"Strong vocabulary";
    request.needsImprovement = u"Use details";

    for (const auto& [grade, ordinal] : {
             std::pair{4, std::u16string_view(u"4th-grade")},
             std::pair{5, std::u16string_view(u"5th-grade")},
             std::pair{6, std::u16string_view(u"6th-grade")}
         })
    {
        request.grade = grade;
        const auto result = buildSpeakingEvaluationAiCommentPrompt(
            request,
            matcher
            );
        const std::u16string expectedPrefix =
            u"Write one polished speaking-evaluation comment for a "
            + std::u16string(ordinal)
            + u" elementary ESL student.\n\nRequirements:\n";
        if (!result.valid || !result.prompt.starts_with(expectedPrefix))
        {
            std::fprintf(
                stderr,
                "Integer grade %d did not produce the expected ordinal prefix.\n",
                grade
                );
            if (!result.valid)
            {
                std::fprintf(stderr, "Prompt was rejected.\n");
            }
            else
            {
                std::fprintf(stderr, "Actual prefix: ");
                const std::size_t prefixLength = std::min<std::size_t>(
                    result.prompt.size(),
                    expectedPrefix.size()
                    );
                for (std::size_t index = 0; index < prefixLength; ++index)
                {
                    std::fputc(static_cast<char>(result.prompt[index]), stderr);
                }
                std::fprintf(stderr, "\nExpected prefix: ");
                for (const char16_t character : expectedPrefix)
                {
                    std::fputc(static_cast<char>(character), stderr);
                }
                std::fputc('\n', stderr);
            }
            return false;
        }
    }
    return true;
}

bool batchRedactionUsesAliasesAndLongestFirstWithoutBoundaries()
{
    const AsciiTextMatcher matcher;
    SpeakingEvaluationAiBatchCommentPromptRequest request;
    request.students = {
        {
            u"STUDENT_01", 4, u"Ann", {},
            u"Ann praised Anna, then Unchecked and XAnn!",
            u"Add an example"
        },
        {
            u"STUDENT_02", 5, u"Anna", {},
            u"Anna spoke clearly", u"Use details"
        }
    };
    request.additionalNamesToRedact = {u"Unchecked"};
    const auto result = buildSpeakingEvaluationAiBatchCommentPrompt(
        request,
        matcher
        );
    if (!check(result.valid, "A valid batch prompt was rejected."))
    {
        return false;
    }
    const std::u16string expectedRecords =
        u"\n\nStudent records:\n\n"
        u"Student ID: STUDENT_01\n"
        u"Grade: 4th-grade elementary ESL\n"
        u"Did well:\n"
        u"- STD_NAME praised CLASSMATE, then CLASSMATE and XSTD_NAME!\n"
        u"Needs improvement:\n"
        u"- Add an example\n\n"
        u"Student ID: STUDENT_02\n"
        u"Grade: 5th-grade elementary ESL\n"
        u"Did well:\n"
        u"- STD_NAME spoke clearly\n"
        u"Needs improvement:\n"
        u"- Use details";
    const std::u16string expectedTemplate =
        u"Write one separate polished speaking-evaluation comment for "
        u"each anonymized student below.\n\n"
        u"Apply every requirement independently to every comment:\n";
    const std::u16string expectedFormat =
        u"\n\nResponse format:\n"
        u"- Return exactly one paired block for every Student ID, in the "
        u"same order as the input.\n"
        u"- Copy each Student ID exactly in both markers.\n"
        u"- Put only the finished comment between its markers.\n"
        u"- Do not add introductions, explanations, markdown fences, or "
        u"any other text.\n\n"
        u"<<<STUDENT_01>>>\nFinished comment containing STD_NAME\n"
        u"<<<END_STUDENT_01>>>\n"
        u"<<<STUDENT_02>>>\nFinished comment containing STD_NAME\n"
        u"<<<END_STUDENT_02>>>";
    const std::u16string expectedDirect =
        expectedTemplate
        + expectedCommonRequirements(AiCommentVoice::DirectToStudent)
        + expectedFormat
        + expectedRecords;
    if (!check(
            result.prompt == expectedDirect,
            "The complete direct-voice batch prompt text changed."
            ))
    {
        return false;
    }

    request.voice = AiCommentVoice::ThirdPerson;
    const auto thirdPerson = buildSpeakingEvaluationAiBatchCommentPrompt(
        request,
        matcher
        );
    const std::u16string expectedThirdPerson =
        expectedTemplate
        + expectedCommonRequirements(AiCommentVoice::ThirdPerson)
        + expectedFormat
        + expectedRecords;
    return check(
        thirdPerson.valid && thirdPerson.prompt == expectedThirdPerson,
        "The complete third-person batch prompt text changed."
        );
}

bool batchPromptHasNoStudentCountCap()
{
    const AsciiTextMatcher matcher;
    SpeakingEvaluationAiBatchCommentPromptRequest request;
    for (int index = 1; index <= 30; ++index)
    {
        const std::u16string number = index < 10
            ? std::u16string(u"0") + static_cast<char16_t>(u'0' + index)
            : std::u16string(1, static_cast<char16_t>(u'0' + index / 10))
                + static_cast<char16_t>(u'0' + index % 10);
        request.students.push_back({
            u"STUDENT_" + number,
            4 + (index % 3),
            u"Name" + number,
            {},
            u"Strong speaking",
            u"Add detail"
        });
    }
    const auto result = buildSpeakingEvaluationAiBatchCommentPrompt(
        request,
        matcher
        );
    return check(
        result.valid
            && result.prompt.find(u"Student ID: STUDENT_25") != std::u16string::npos
            && result.prompt.find(u"<<<STUDENT_30>>>") != std::u16string::npos
            && result.prompt.find(u"Student ID: STUDENT_30") != std::u16string::npos,
        "Batch prompt unexpectedly capped the student count."
        );
}

bool invalidInputsFollowTheExistingEligibilityPolicy()
{
    const AsciiTextMatcher matcher;
    SpeakingEvaluationAiCommentPromptRequest single;
    single.grade = 5;
    single.didWell = u"Strong vocabulary";
    single.needsImprovement = u"Use details";
    if (!check(
            buildSpeakingEvaluationAiCommentPrompt(single, matcher).valid,
            "Single prompt no longer permits a blank name."
            ))
    {
        return false;
    }
    single.grade = 3;
    if (!check(
            !buildSpeakingEvaluationAiCommentPrompt(single, matcher).valid,
            "An unsupported single-prompt grade was accepted."
            ))
    {
        return false;
    }

    SpeakingEvaluationAiBatchCommentPromptRequest batch;
    batch.students = {{u"STUDENT_01", 5, u"A", {}, u"Good", u"Improve"}};
    if (!check(
            buildSpeakingEvaluationAiBatchCommentPrompt(batch, matcher).valid,
            "A valid batch record was rejected."
            ))
    {
        return false;
    }
    auto invalid = batch;
    invalid.students.front().id = u" \t ";
    if (!check(!buildSpeakingEvaluationAiBatchCommentPrompt(invalid, matcher).valid,
            "A blank batch ID was accepted."))
    {
        return false;
    }
    invalid = batch;
    invalid.students.push_back(invalid.students.front());
    if (!check(!buildSpeakingEvaluationAiBatchCommentPrompt(invalid, matcher).valid,
            "A duplicate batch ID was accepted."))
    {
        return false;
    }
    invalid = batch;
    invalid.students.front().grade = 7;
    if (!check(!buildSpeakingEvaluationAiBatchCommentPrompt(invalid, matcher).valid,
            "An unsupported batch grade was accepted."))
    {
        return false;
    }
    invalid = batch;
    invalid.students.front().needsImprovement.clear();
    if (!check(!buildSpeakingEvaluationAiBatchCommentPrompt(invalid, matcher).valid,
            "A batch record missing a note item was accepted."))
    {
        return false;
    }
    invalid = batch;
    invalid.students.front().englishName.clear();
    if (!check(!buildSpeakingEvaluationAiBatchCommentPrompt(invalid, matcher).valid,
            "A batch record missing a name was accepted."))
    {
        return false;
    }
    return true;
}

} // namespace

int main()
{
    if (!observationsNormalizeLineEndingsAndRepeatedPrefixes()
        || !elementaryGradeConversionPreservesLegacyAcceptedForms()
        || !singlePromptTextAndVoiceAreExact()
        || !singlePromptKeepsIntegerGradesAndOrdinalWording()
        || !batchRedactionUsesAliasesAndLongestFirstWithoutBoundaries()
        || !batchPromptHasNoStudentCountCap()
        || !invalidInputsFollowTheExistingEligibilityPolicy())
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
