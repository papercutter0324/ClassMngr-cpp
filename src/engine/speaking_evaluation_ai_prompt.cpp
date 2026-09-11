#include "classmngr/engine/speaking_evaluation_ai_prompt.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace classmngr::engine
{
namespace
{
std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (
        first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first]))
        )
    {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1]))
        )
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

char asciiLower(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

bool equalsIgnoreAsciiCase(
    std::string_view left,
    std::string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (asciiLower(left[index]) != asciiLower(right[index]))
        {
            return false;
        }
    }

    return true;
}

std::size_t findIgnoreAsciiCase(
    std::string_view value,
    std::string_view needle,
    std::size_t start
    )
{
    if (needle.empty() || start > value.size())
    {
        return std::string_view::npos;
    }

    for (
        std::size_t position = start;
        position + needle.size() <= value.size();
        ++position
        )
    {
        bool matches = true;
        for (std::size_t index = 0; index < needle.size(); ++index)
        {
            if (
                asciiLower(value[position + index])
                != asciiLower(needle[index])
                )
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return position;
        }
    }

    return std::string_view::npos;
}

void replaceAllIgnoreAsciiCase(
    std::string& value,
    std::string_view needle,
    std::string_view replacement
    )
{
    std::size_t searchStart = 0;
    while (true)
    {
        const std::size_t position =
            findIgnoreAsciiCase(value, needle, searchStart);
        if (position == std::string_view::npos)
        {
            return;
        }

        value.replace(position, needle.size(), replacement);
        searchStart = position + replacement.size();
    }
}

std::string withoutListPrefix(std::string_view value)
{
    std::string line = trimAsciiWhitespace(value);
    while (true)
    {
        if (line.starts_with("\xE2\x80\xA2"))
        {
            line.erase(0, 3);
        }
        else if (!line.empty() && (line.front() == '-' || line.front() == '*'))
        {
            line.erase(0, 1);
        }
        else
        {
            return line;
        }

        line = trimAsciiWhitespace(line);
    }
}

std::string joinStrings(
    const std::vector<std::string>& values,
    std::string_view separator
    )
{
    std::string result;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            result += separator;
        }
        result += values[index];
    }
    return result;
}

std::string promptItems(
    std::string observations,
    std::string_view englishName,
    std::string_view koreanName,
    const std::vector<std::string>& otherNames = {}
    )
{
    const std::array<std::string, 2> studentNames{
        trimAsciiWhitespace(englishName),
        trimAsciiWhitespace(koreanName)
    };
    for (const std::string& studentName : studentNames)
    {
        if (!studentName.empty())
        {
            replaceAllIgnoreAsciiCase(
                observations,
                studentName,
                "STD_NAME"
                );
        }
    }

    std::vector<std::string> redactedNames = otherNames;
    std::ranges::sort(
        redactedNames,
        [](const std::string& left, const std::string& right)
        {
            return left.size() > right.size();
        }
        );
    for (const std::string& name : redactedNames)
    {
        const std::string trimmedName = trimAsciiWhitespace(name);
        if (
            trimmedName.empty()
            || equalsIgnoreAsciiCase(trimmedName, studentNames[0])
            || equalsIgnoreAsciiCase(trimmedName, studentNames[1])
            )
        {
            continue;
        }
        replaceAllIgnoreAsciiCase(
            observations,
            trimmedName,
            "CLASSMATE"
            );
    }

    std::vector<std::string> lines;
    for (const std::string& item : SpeakingEvaluationAiPromptService::observationItems(observations))
    {
        lines.push_back("- " + item);
    }
    return joinStrings(lines, "\n");
}

std::string voiceInstruction(SpeakingEvaluationAiVoice voice)
{
    return voice == SpeakingEvaluationAiVoice::ThirdPerson
        ? "Write for a parent or guardian, use STD_NAME, "
          "and use they/their rather than guessing gender."
        : "Address the student directly as \"you\" and "
          "use STD_NAME naturally.";
}

std::string commonCommentRequirements(SpeakingEvaluationAiVoice voice)
{
    return "- Write one paragraph of exactly 3 short sentences between 100 "
           "and 420 characters, including spaces.\n"
           "- When the submitted notes provide enough relevant detail, aim "
           "for 300 to 350 characters. Otherwise, stay within the required "
           "range without inventing or repeating information.\n"
           "- Use simple, common English that an elementary ESL student "
           "and a parent with limited English can understand.\n"
           "- Avoid long sentences, complex clauses, idioms, and uncommon "
           "words.\n"
           "- Sentence 1 must give positive feedback, sentence 2 must give "
           "constructive advice, and sentence 3 must give positive "
           "feedback.\n"
           "- Sentences 1 and 3 must emphasize different skill categories. "
           "If sentence 1 emphasizes presentation skills, sentence 3 must "
           "emphasize grammar-related skills. If sentence 1 emphasizes "
           "grammar-related skills, sentence 3 must emphasize presentation "
           "skills.\n"
           "- Aim to praise at least two submitted skills in each positive "
           "sentence. If one category does not contain two suitable items, "
           "supplement it with another submitted strength from the Did well "
           "list. Never invent a skill to reach two items.\n"
           "- Select only the most useful observations. You do not need "
           "to mention every item, and do not cram several items into one "
           "sentence.\n"
           "- End with a short final sentence that praises specific items "
           "from the Did well list.\n"
           "- Use a warm, supportive, professional, and age-appropriate "
           "tone.\n"
           "- Use only the submitted observations; do not invent "
           "abilities, scores, or events.\n"
           "- Include the exact placeholder STD_NAME at least once. "
           "Do not alter or replace it.\n"
           "- "
           + voiceInstruction(voice)
           + "\n"
           "- Do not use headings, bullet points, quotation marks, or "
           "a character-count annotation.";
}

std::string gradeOrdinal(int grade)
{
    switch (grade)
    {
    case 4:
        return "4th";
    case 5:
        return "5th";
    case 6:
        return "6th";
    default:
        return {};
    }
}

struct OpeningMarker
{
    std::size_t end = 0;
    std::string id;
};

std::optional<OpeningMarker> nextOpeningMarker(
    std::string_view response,
    std::size_t start
    )
{
    constexpr std::string_view prefix = "<<<STUDENT_";
    std::size_t position = response.find(prefix, start);
    while (position != std::string_view::npos)
    {
        const std::size_t digits = position + prefix.size();
        const bool hasTwoDigits =
            digits + 5 <= response.size()
            && response[digits] >= '0'
            && response[digits] <= '9'
            && response[digits + 1] >= '0'
            && response[digits + 1] <= '9'
            && response.substr(digits + 2, 3) == ">>>";
        if (hasTwoDigits)
        {
            return OpeningMarker{
                digits + 5,
                "STUDENT_" + std::string(response.substr(digits, 2))
            };
        }

        position = response.find(prefix, position + prefix.size());
    }

    return std::nullopt;
}

std::vector<OpeningMarker> openingMarkers(std::string_view response)
{
    std::vector<OpeningMarker> result;
    std::size_t position = 0;
    while (true)
    {
        const auto marker = nextOpeningMarker(response, position);
        if (!marker)
        {
            return result;
        }

        result.push_back(*marker);
        position = marker->end;
    }
}
} // namespace

std::vector<std::string> SpeakingEvaluationAiPromptService::observationItems(
    std::string_view observations
    )
{
    std::string normalized;
    normalized.reserve(observations.size());
    for (std::size_t index = 0; index < observations.size(); ++index)
    {
        if (observations[index] == '\r')
        {
            if (index + 1 < observations.size() && observations[index + 1] == '\n')
            {
                ++index;
            }
            normalized.push_back('\n');
        }
        else
        {
            normalized.push_back(observations[index]);
        }
    }

    std::vector<std::string> items;
    std::size_t lineStart = 0;
    while (lineStart <= normalized.size())
    {
        const std::size_t lineEnd = normalized.find('\n', lineStart);
        const std::size_t length =
            lineEnd == std::string::npos
                ? normalized.size() - lineStart
                : lineEnd - lineStart;
        const std::string item = withoutListPrefix(
            std::string_view(normalized).substr(lineStart, length)
            );
        if (!item.empty())
        {
            items.push_back(item);
        }

        if (lineEnd == std::string::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return items;
}

bool SpeakingEvaluationAiPromptService::canBuildPrompt(
    const SpeakingEvaluationAiPromptInput& input
    )
{
    return input.grade >= 4
        && input.grade <= 6
        && !observationItems(input.didWell).empty()
        && !observationItems(input.needsImprovement).empty();
}

std::string SpeakingEvaluationAiPromptService::buildCommentPrompt(
    const SpeakingEvaluationAiPromptInput& input
    )
{
    if (!canBuildPrompt(input))
    {
        return {};
    }

    return "Write one polished speaking-evaluation comment for a "
           + gradeOrdinal(input.grade)
           + "-grade elementary ESL student.\n\n"
           "Requirements:\n"
           + commonCommentRequirements(input.voice)
           + "\n- Return only the finished comment.\n\n"
           "Did well:\n"
           + promptItems(
               input.didWell,
               input.englishName,
               input.koreanName
               )
           + "\n\nNeeds improvement:\n"
           + promptItems(
               input.needsImprovement,
               input.englishName,
               input.koreanName
               );
}

std::string SpeakingEvaluationAiPromptService::buildBatchCommentPrompt(
    const SpeakingEvaluationAiBatchPromptInput& input
    )
{
    if (input.students.empty())
    {
        return {};
    }

    std::vector<std::string> namesToRedact = input.additionalNamesToRedact;
    std::unordered_set<std::string> ids;
    for (const auto& student : input.students)
    {
        if (
            trimAsciiWhitespace(student.id).empty()
            || ids.contains(student.id)
            || student.grade < 4
            || student.grade > 6
            || observationItems(student.didWell).empty()
            || observationItems(student.needsImprovement).empty()
            )
        {
            return {};
        }
        ids.insert(student.id);
        namesToRedact.push_back(student.englishName);
        namesToRedact.push_back(student.koreanName);
    }

    std::vector<std::string> records;
    std::vector<std::string> outputExamples;
    for (const auto& student : input.students)
    {
        records.push_back(
            "Student ID: " + student.id + "\n"
            "Grade: " + gradeOrdinal(student.grade)
            + "-grade elementary ESL\n"
            "Did well:\n"
            + promptItems(
                student.didWell,
                student.englishName,
                student.koreanName,
                namesToRedact
                )
            + "\nNeeds improvement:\n"
            + promptItems(
                student.needsImprovement,
                student.englishName,
                student.koreanName,
                namesToRedact
                )
            );
        outputExamples.push_back(
            "<<<" + student.id + ">>>\n"
            "Finished comment containing STD_NAME\n"
            "<<<END_" + student.id + ">>>"
            );
    }

    return "Write one separate polished speaking-evaluation comment for "
           "each anonymized student below.\n\n"
           "Apply every requirement independently to every comment:\n"
           + commonCommentRequirements(input.voice)
           + "\n\nResponse format:\n"
           "- Return exactly one paired block for every Student ID, in the "
           "same order as the input.\n"
           "- Copy each Student ID exactly in both markers.\n"
           "- Put only the finished comment between its markers.\n"
           "- Do not add introductions, explanations, markdown fences, or "
           "any other text.\n\n"
           + joinStrings(outputExamples, "\n")
           + "\n\nStudent records:\n\n"
           + joinStrings(records, "\n\n");
}

SpeakingEvaluationAiBatchParseResult
SpeakingEvaluationAiPromptService::parseBatchResponse(
    std::string_view response,
    const std::vector<std::string>& expectedIds
    )
{
    SpeakingEvaluationAiBatchParseResult result;
    const std::unordered_set<std::string> expected(
        expectedIds.cbegin(),
        expectedIds.cend()
        );

    std::unordered_map<std::string, int> completeCounts;
    std::unordered_map<
        std::string,
        SpeakingEvaluationAiBatchComment
        > parsedById;
    std::size_t position = 0;
    while (true)
    {
        const auto opening = nextOpeningMarker(response, position);
        if (!opening)
        {
            break;
        }

        const std::string endMarker = "<<<END_" + opening->id + ">>>";
        const std::size_t endPosition =
            response.find(endMarker, opening->end);
        if (endPosition == std::string_view::npos)
        {
            position = opening->end;
            continue;
        }

        ++completeCounts[opening->id];
        if (!expected.contains(opening->id))
        {
            if (
                std::ranges::find(result.unknownIds, opening->id)
                == result.unknownIds.end()
                )
            {
                result.unknownIds.push_back(opening->id);
            }
        }
        else
        {
            const std::string comment = trimAsciiWhitespace(
                response.substr(
                    opening->end,
                    endPosition - opening->end
                    )
                );
            parsedById[opening->id] = {
                opening->id,
                comment,
                comment.find("STD_NAME") != std::string::npos
            };
        }

        position = endPosition + endMarker.size();
    }

    std::unordered_set<std::string> openedIds;
    for (const OpeningMarker& marker : openingMarkers(response))
    {
        openedIds.insert(marker.id);
    }

    for (const std::string& id : expectedIds)
    {
        const int count = completeCounts[id];
        if (count > 1)
        {
            result.duplicateIds.push_back(id);
            continue;
        }
        if (count == 1)
        {
            result.comments.push_back(parsedById.at(id));
            continue;
        }
        if (openedIds.contains(id))
        {
            result.malformedIds.push_back(id);
        }
    }

    std::ranges::sort(result.unknownIds);
    return result;
}

} // namespace classmngr::engine
