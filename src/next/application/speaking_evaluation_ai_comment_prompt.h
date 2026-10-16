#pragma once

#include "next/application/ai_comment_voice_preferences.h"
#include "next/application/qt_compatible_text.h"
#include "next/application/speaking_evaluation_ai_batch_eligibility.h"
#include "next/application/speaking_evaluation_text_match_port.h"
#include "domain/models/speaking_evaluation_comment_limits.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationAiCommentPromptRequest final
{
    int grade = 0;
    std::u16string englishName;
    std::u16string koreanName;
    std::u16string didWell;
    std::u16string needsImprovement;
    AiCommentVoice voice = AiCommentVoice::DirectToStudent;
};

struct SpeakingEvaluationAiBatchPromptStudent final
{
    std::u16string id;
    int grade = 0;
    std::u16string englishName;
    std::u16string koreanName;
    std::u16string didWell;
    std::u16string needsImprovement;
};

struct SpeakingEvaluationAiBatchCommentPromptRequest final
{
    std::vector<SpeakingEvaluationAiBatchPromptStudent> students;
    std::vector<std::u16string> additionalNamesToRedact;
    AiCommentVoice voice = AiCommentVoice::DirectToStudent;
};

struct SpeakingEvaluationAiCommentPromptResult final
{
    bool valid = false;
    std::u16string prompt;
};

[[nodiscard]] inline std::vector<std::u16string> speakingEvaluationAiObservationItems(
    const std::u16string_view observations
    )
{
    std::u16string normalized;
    normalized.reserve(observations.size());
    for (std::size_t index = 0; index < observations.size(); ++index)
    {
        const char16_t character = observations[index];
        if (character == u'\r')
        {
            if (index + 1 < observations.size()
                && observations[index + 1] == u'\n')
            {
                ++index;
            }
            normalized.push_back(u'\n');
        }
        else
        {
            normalized.push_back(character);
        }
    }

    std::vector<std::u16string> items;
    std::size_t lineStart = 0;
    while (lineStart <= normalized.size())
    {
        const std::size_t lineEnd = normalized.find(u'\n', lineStart);
        const std::size_t length = lineEnd == std::u16string::npos
            ? normalized.size() - lineStart
            : lineEnd - lineStart;
        std::u16string item = trimQtWhitespace(
            std::u16string_view(normalized).substr(lineStart, length)
            );
        while (!item.empty()
            && (item.front() == 0x2022
                || item.front() == u'-'
                || item.front() == u'*'))
        {
            item.erase(item.begin());
            item = trimQtWhitespace(item);
        }
        if (!item.empty())
        {
            items.push_back(std::move(item));
        }
        if (lineEnd == std::u16string::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return items;
}

[[nodiscard]] inline int speakingEvaluationElementaryGrade(
    const std::u16string_view classGrade
    )
{
    std::u16string normalized = trimQtWhitespace(classGrade);
    for (char16_t& character : normalized)
    {
        if (character >= u'a' && character <= u'z')
        {
            character = static_cast<char16_t>(character - (u'a' - u'A'));
        }
    }
    if (!normalized.empty() && normalized.front() == u'E')
    {
        normalized.erase(normalized.begin());
    }
    if (!normalized.empty() && normalized.front() == u'+')
    {
        normalized.erase(normalized.begin());
    }
    if (normalized.empty())
    {
        return 0;
    }

    int grade = 0;
    for (const char16_t character : normalized)
    {
        if (character < u'0' || character > u'9')
        {
            return 0;
        }
        const int digit = static_cast<int>(character - u'0');
        if (grade > (std::numeric_limits<int>::max() - digit) / 10)
        {
            return 0;
        }
        grade = grade * 10 + digit;
    }
    return grade >= 4 && grade <= 6 ? grade : 0;
}

namespace SpeakingEvaluationAiCommentPromptDetail
{

struct RedactionAlias final
{
    std::u16string value;
};

struct RedactionCandidate final
{
    std::size_t start = 0;
    std::size_t length = 0;
    std::size_t aliasOrder = 0;
    std::u16string replacement;
};

[[nodiscard]] inline std::u16string ordinalGrade(const int grade)
{
    switch (grade)
    {
    case 4:
        return u"4th";
    case 5:
        return u"5th";
    case 6:
        return u"6th";
    default:
        return {};
    }
}

[[nodiscard]] inline std::u16string voiceInstruction(const AiCommentVoice voice)
{
    return voice == AiCommentVoice::ThirdPerson
        ? u"Write for a parent or guardian, use STD_NAME, "
          u"and use they/their rather than guessing gender."
        : u"Address the student directly as \"you\" and "
          u"use STD_NAME naturally.";
}

inline void appendDecimal(std::u16string& destination, const int value)
{
    const std::string digits = std::to_string(value);
    for (const char digit : digits)
    {
        destination.push_back(static_cast<char16_t>(digit));
    }
}

[[nodiscard]] inline std::u16string commonCommentRequirements(
    const AiCommentVoice voice
    )
{
    std::u16string result =
        u"- Write one paragraph of exactly 3 short sentences between ";
    appendDecimal(result, SpeakingEval::CommentMinLength);
    result += u" and ";
    appendDecimal(result, SpeakingEval::CommentPreferredMaxLength);
    result += u" characters, including spaces.\n"
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
        u"Do not alter or replace it.\n"
        u"- ";
    result += voiceInstruction(voice);
    result += u"\n- Do not use headings, bullet points, quotation marks, or "
        u"a character-count annotation.";
    return result;
}

[[nodiscard]] inline std::u16string promptItems(
    const std::u16string_view observations,
    const std::u16string_view englishName,
    const std::u16string_view koreanName,
    const std::vector<std::u16string>* const otherNames,
    const SpeakingEvaluationTextMatchPort& textMatcher
    )
{
    const std::u16string trimmedEnglishName = trimQtWhitespace(englishName);
    const std::u16string trimmedKoreanName = trimQtWhitespace(koreanName);

    std::vector<RedactionAlias> aliases;
    aliases.reserve(2 + (otherNames == nullptr ? 0 : otherNames->size()));
    auto appendAlias = [&aliases](const std::u16string_view value)
    {
        std::u16string trimmed = trimQtWhitespace(value);
        if (!trimmed.empty())
        {
            aliases.push_back({std::move(trimmed)});
        }
    };
    appendAlias(trimmedEnglishName);
    appendAlias(trimmedKoreanName);
    if (otherNames != nullptr)
    {
        for (const std::u16string& name : *otherNames)
        {
            appendAlias(name);
        }
    }
    std::stable_sort(
        aliases.begin(),
        aliases.end(),
        [](const RedactionAlias& left, const RedactionAlias& right)
        {
            return left.value.size() > right.value.size();
        }
        );

    std::vector<RedactionCandidate> candidates;
    for (std::size_t aliasIndex = 0; aliasIndex < aliases.size(); ++aliasIndex)
    {
        const RedactionAlias& alias = aliases[aliasIndex];
        const auto matches = textMatcher.findLiteralCaseInsensitiveMatches(
            observations,
            alias.value
            );
        for (const SpeakingEvaluationTextMatchSpan match : matches)
        {
            if (match.length == 0
                || match.start > observations.size()
                || match.length > observations.size() - match.start)
            {
                continue;
            }
            const std::u16string_view captured = observations.substr(
                match.start,
                match.length
                );
            const bool isStudentName =
                (!trimmedEnglishName.empty()
                    && textMatcher.equalsCaseInsensitive(
                        captured,
                        trimmedEnglishName
                        ))
                || (!trimmedKoreanName.empty()
                    && textMatcher.equalsCaseInsensitive(
                        captured,
                        trimmedKoreanName
                        ));
            candidates.push_back({
                match.start,
                match.length,
                aliasIndex,
                isStudentName ? u"STD_NAME" : u"CLASSMATE"
            });
        }
    }
    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const RedactionCandidate& left, const RedactionCandidate& right)
        {
            if (left.start != right.start)
            {
                return left.start < right.start;
            }
            return left.aliasOrder < right.aliasOrder;
        }
        );

    std::u16string redacted;
    redacted.reserve(observations.size());
    std::size_t previousEnd = 0;
    for (const RedactionCandidate& candidate : candidates)
    {
        if (candidate.start < previousEnd)
        {
            continue;
        }
        redacted.append(observations.substr(
            previousEnd,
            candidate.start - previousEnd
            ));
        redacted += candidate.replacement;
        previousEnd = candidate.start + candidate.length;
    }
    redacted.append(observations.substr(previousEnd));

    std::u16string result;
    const std::vector<std::u16string> items =
        speakingEvaluationAiObservationItems(redacted);
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (index != 0)
        {
            result.push_back(u'\n');
        }
        result += u"- ";
        result += items[index];
    }
    return result;
}

[[nodiscard]] inline bool isEligible(
    const int grade,
    const std::u16string_view englishName,
    const std::u16string_view koreanName,
    const std::u16string_view didWell,
    const std::u16string_view needsImprovement
    )
{
    return speakingEvaluationAiBatchEligibilityReason({
        .hasTrimmedStudentName =
            !trimQtWhitespace(englishName).empty()
            || !trimQtWhitespace(koreanName).empty(),
        .grade = grade,
        .hasDidWellItem = !speakingEvaluationAiObservationItems(didWell).empty(),
        .hasNeedsImprovementItem =
            !speakingEvaluationAiObservationItems(needsImprovement).empty()
    }) == SpeakingEvaluationAiBatchEligibilityReason::Eligible;
}

[[nodiscard]] inline bool singlePromptInputsAreValid(
    const int grade,
    const std::u16string_view didWell,
    const std::u16string_view needsImprovement
    )
{
    // The single prompt has historically allowed blank names. The dialog's
    // shared eligibility policy still requires a name before enabling it.
    return speakingEvaluationAiBatchEligibilityReason({
        .hasTrimmedStudentName = true,
        .grade = grade,
        .hasDidWellItem = !speakingEvaluationAiObservationItems(didWell).empty(),
        .hasNeedsImprovementItem =
            !speakingEvaluationAiObservationItems(needsImprovement).empty()
    }) == SpeakingEvaluationAiBatchEligibilityReason::Eligible;
}

} // namespace SpeakingEvaluationAiCommentPromptDetail

[[nodiscard]] inline SpeakingEvaluationAiCommentPromptResult
buildSpeakingEvaluationAiCommentPrompt(
    const SpeakingEvaluationAiCommentPromptRequest& request,
    const SpeakingEvaluationTextMatchPort& textMatcher
    )
{
    using namespace SpeakingEvaluationAiCommentPromptDetail;
    if (!singlePromptInputsAreValid(
            request.grade,
            request.didWell,
            request.needsImprovement
            ))
    {
        return {};
    }

    std::u16string prompt = u"Write one polished speaking-evaluation "
        u"comment for a ";
    prompt += ordinalGrade(request.grade);
    prompt += u"-grade elementary ESL student.\n\nRequirements:\n";
    prompt += commonCommentRequirements(request.voice);
    prompt += u"\n- Return only the finished comment.\n\nDid well:\n";
    prompt += promptItems(
        request.didWell,
        request.englishName,
        request.koreanName,
        nullptr,
        textMatcher
        );
    prompt += u"\n\nNeeds improvement:\n";
    prompt += promptItems(
        request.needsImprovement,
        request.englishName,
        request.koreanName,
        nullptr,
        textMatcher
        );
    return {true, std::move(prompt)};
}

[[nodiscard]] inline SpeakingEvaluationAiCommentPromptResult
buildSpeakingEvaluationAiBatchCommentPrompt(
    const SpeakingEvaluationAiBatchCommentPromptRequest& request,
    const SpeakingEvaluationTextMatchPort& textMatcher
    )
{
    using namespace SpeakingEvaluationAiCommentPromptDetail;
    if (request.students.empty())
    {
        return {};
    }

    std::unordered_set<std::u16string> ids;
    std::vector<std::u16string> namesToRedact =
        request.additionalNamesToRedact;
    for (const SpeakingEvaluationAiBatchPromptStudent& student : request.students)
    {
        if (trimQtWhitespace(student.id).empty()
            || !ids.insert(student.id).second
            || !isEligible(
                student.grade,
                student.englishName,
                student.koreanName,
                student.didWell,
                student.needsImprovement
                ))
        {
            return {};
        }
        namesToRedact.push_back(student.englishName);
        namesToRedact.push_back(student.koreanName);
    }

    std::u16string records;
    std::u16string outputExamples;
    for (std::size_t index = 0; index < request.students.size(); ++index)
    {
        const SpeakingEvaluationAiBatchPromptStudent& student =
            request.students[index];
        if (index != 0)
        {
            records += u"\n\n";
            outputExamples.push_back(u'\n');
        }
        records += u"Student ID: ";
        records += student.id;
        records += u"\nGrade: ";
        records += ordinalGrade(student.grade);
        records += u"-grade elementary ESL\nDid well:\n";
        records += promptItems(
            student.didWell,
            student.englishName,
            student.koreanName,
            &namesToRedact,
            textMatcher
            );
        records += u"\nNeeds improvement:\n";
        records += promptItems(
            student.needsImprovement,
            student.englishName,
            student.koreanName,
            &namesToRedact,
            textMatcher
            );

        outputExamples += u"<<<";
        outputExamples += student.id;
        outputExamples += u">>>\nFinished comment containing STD_NAME\n<<<END_";
        outputExamples += student.id;
        outputExamples += u">>>";
    }

    std::u16string prompt = u"Write one separate polished speaking-"
        u"evaluation comment for each anonymized student below.\n\n"
        u"Apply every requirement independently to every comment:\n";
    prompt += commonCommentRequirements(request.voice);
    prompt += u"\n\nResponse format:\n"
        u"- Return exactly one paired block for every Student ID, in the "
        u"same order as the input.\n"
        u"- Copy each Student ID exactly in both markers.\n"
        u"- Put only the finished comment between its markers.\n"
        u"- Do not add introductions, explanations, markdown fences, or "
        u"any other text.\n\n";
    prompt += outputExamples;
    prompt += u"\n\nStudent records:\n\n";
    prompt += records;
    return {true, std::move(prompt)};
}

} // namespace ClassMngr::Next::Application

