#include "speaking_eval_ai_prompt.h"

#include "classmngr/engine/speaking_evaluation_report_model.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace
{

QString withoutListPrefix(
    QString line
    )
{
    line = line.trimmed();
    while (
        line.startsWith(QChar(0x2022))
        || line.startsWith(QLatin1Char('-'))
        || line.startsWith(QLatin1Char('*'))
        )
    {
        line.remove(0, 1);
        line = line.trimmed();
    }
    return line;
}

QString promptItems(
    QString observations,
    const QString& englishName,
    const QString& koreanName,
    const QStringList& otherNames = {}
    )
{
    for (
        const QString& studentName :
        {
            englishName.trimmed(),
            koreanName.trimmed()
        }
        )
    {
        if (!studentName.isEmpty())
        {
            observations.replace(
                studentName,
                QStringLiteral("STD_NAME"),
                Qt::CaseInsensitive
            );
        }
    }

    QStringList redactedNames = otherNames;
    std::ranges::sort(
        redactedNames,
        [](const QString& left, const QString& right)
        {
            return left.size() > right.size();
        }
        );
    for (const QString& name : redactedNames)
    {
        const QString trimmedName = name.trimmed();
        if (
            trimmedName.isEmpty()
            || trimmedName.compare(
                englishName.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            || trimmedName.compare(
                koreanName.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            continue;
        }
        observations.replace(
            trimmedName,
            QStringLiteral("CLASSMATE"),
            Qt::CaseInsensitive
            );
    }

    QStringList lines;
    for (
        const QString& item :
        speakingEvalAiObservationItems(observations)
        )
    {
        lines.append(
            QStringLiteral("- %1").arg(item)
            );
    }
    return lines.join(QLatin1Char('\n'));
}

QString voiceInstruction(
    AiCommentVoice voice
    )
{
    return voice == AiCommentVoice::ThirdPerson
        ? QStringLiteral(
            "Write for a parent or guardian, use STD_NAME, "
            "and use they/their rather than guessing gender."
            )
        : QStringLiteral(
            "Address the student directly as \"you\" and "
            "use STD_NAME naturally."
            );
}

QString commonCommentRequirements(
    AiCommentVoice voice
    )
{
    return QStringLiteral(
        "- Write one paragraph of exactly 3 short sentences between 100 "
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
        "- %1\n"
        "- Do not use headings, bullet points, quotation marks, or "
        "a character-count annotation."
        ).arg(voiceInstruction(voice));
}

QString gradeOrdinal(
    int grade
    )
{
    switch (grade)
    {
    case 4:
        return QStringLiteral("4th");
    case 5:
        return QStringLiteral("5th");
    case 6:
        return QStringLiteral("6th");
    default:
        return {};
    }
}

}

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
    QString normalized = observations;
    normalized.replace(
        QStringLiteral("\r\n"),
        QStringLiteral("\n")
        );
    normalized.replace(
        QLatin1Char('\r'),
        QLatin1Char('\n')
        );

    QStringList items;
    for (
        const QString& line :
        normalized.split(
            QLatin1Char('\n'),
            Qt::KeepEmptyParts
            )
        )
    {
        const QString item =
            withoutListPrefix(line);
        if (!item.isEmpty())
        {
            items.append(item);
        }
    }
    return items;
}

bool canBuildSpeakingEvalAiPrompt(
    const SpeakingEvalAiPromptInput& input
    )
{
    return input.grade >= 4
        && input.grade <= 6
        && !speakingEvalAiObservationItems(
            input.didWell
            ).isEmpty()
        && !speakingEvalAiObservationItems(
            input.needsImprovement
            ).isEmpty();
}

QString buildSpeakingEvalAiCommentPrompt(
    const SpeakingEvalAiPromptInput& input
    )
{
    if (!canBuildSpeakingEvalAiPrompt(input))
    {
        return {};
    }

    return QStringLiteral(
        "Write one polished speaking-evaluation comment for a %1-grade "
        "elementary ESL student.\n\n"
        "Requirements:\n"
        "%2\n"
        "- Return only the finished comment.\n\n"
        "Did well:\n"
        "%3\n\n"
        "Needs improvement:\n"
        "%4"
        ).arg(
            gradeOrdinal(input.grade),
            commonCommentRequirements(input.voice),
            promptItems(
                input.didWell,
                input.englishName,
                input.koreanName
                ),
            promptItems(
                input.needsImprovement,
                input.englishName,
                input.koreanName
                )
            );
}

QString buildSpeakingEvalAiBatchCommentPrompt(
    const SpeakingEvalAiBatchPromptInput& input
    )
{
    if (input.students.isEmpty())
    {
        return {};
    }

    QStringList namesToRedact =
        input.additionalNamesToRedact;
    QSet<QString> ids;
    for (const auto& student : input.students)
    {
        if (
            student.id.trimmed().isEmpty()
            || ids.contains(student.id)
            || student.grade < 4
            || student.grade > 6
            || speakingEvalAiObservationItems(
                student.didWell
                ).isEmpty()
            || speakingEvalAiObservationItems(
                student.needsImprovement
                ).isEmpty()
            )
        {
            return {};
        }
        ids.insert(student.id);
        namesToRedact.append(student.englishName);
        namesToRedact.append(student.koreanName);
    }

    QStringList records;
    QStringList outputExamples;
    for (const auto& student : input.students)
    {
        records.append(
            QStringLiteral(
                "Student ID: %1\n"
                "Grade: %2-grade elementary ESL\n"
                "Did well:\n%3\n"
                "Needs improvement:\n%4"
                ).arg(
                    student.id,
                    gradeOrdinal(student.grade),
                    promptItems(
                        student.didWell,
                        student.englishName,
                        student.koreanName,
                        namesToRedact
                        ),
                    promptItems(
                        student.needsImprovement,
                        student.englishName,
                        student.koreanName,
                        namesToRedact
                        )
                    )
            );
        outputExamples.append(
            QStringLiteral(
                "<<<%1>>>\n"
                "Finished comment containing STD_NAME\n"
                "<<<END_%1>>>"
                ).arg(student.id)
            );
    }

    return QStringLiteral(
        "Write one separate polished speaking-evaluation comment for "
        "each anonymized student below.\n\n"
        "Apply every requirement independently to every comment:\n"
        "%1\n\n"
        "Response format:\n"
        "- Return exactly one paired block for every Student ID, in the "
        "same order as the input.\n"
        "- Copy each Student ID exactly in both markers.\n"
        "- Put only the finished comment between its markers.\n"
        "- Do not add introductions, explanations, markdown fences, or "
        "any other text.\n\n"
        "%2\n\n"
        "Student records:\n\n"
        "%3"
        ).arg(
            commonCommentRequirements(input.voice),
            outputExamples.join(QStringLiteral("\n")),
            records.join(QStringLiteral("\n\n"))
            );
}

SpeakingEvalAiBatchParseResult
parseSpeakingEvalAiBatchResponse(
    const QString& response,
    const QStringList& expectedIds
    )
{
    SpeakingEvalAiBatchParseResult result;
    const QSet<QString> expected(
        expectedIds.cbegin(),
        expectedIds.cend()
        );

    const QRegularExpression completeBlock(
        QStringLiteral(
            R"(<<<(STUDENT_[0-9]{2})>>>\s*(.*?)\s*<<<END_\1>>>)"
            ),
        QRegularExpression::DotMatchesEverythingOption
        );
    const QRegularExpression openingMarker(
        QStringLiteral(R"(<<<(STUDENT_[0-9]{2})>>>)")
        );

    QHash<QString, int> completeCounts;
    QHash<QString, SpeakingEvalAiBatchComment> parsedById;
    auto matches =
        completeBlock.globalMatch(response);
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match =
            matches.next();
        const QString id =
            match.captured(1);
        ++completeCounts[id];
        if (!expected.contains(id))
        {
            if (!result.unknownIds.contains(id))
            {
                result.unknownIds.append(id);
            }
            continue;
        }

        const QString comment =
            match.captured(2).trimmed();
        parsedById.insert(
            id,
            {
                id,
                comment,
                comment.contains(
                    QStringLiteral("STD_NAME"),
                    Qt::CaseSensitive
                    )
            }
            );
    }

    auto openings =
        openingMarker.globalMatch(response);
    QSet<QString> openedIds;
    while (openings.hasNext())
    {
        openedIds.insert(
            openings.next().captured(1)
            );
    }

    for (const QString& id : expectedIds)
    {
        const int count =
            completeCounts.value(id);
        if (count > 1)
        {
            result.duplicateIds.append(id);
            continue;
        }
        if (count == 1)
        {
            result.comments.append(
                parsedById.value(id)
                );
            continue;
        }
        if (openedIds.contains(id))
        {
            result.malformedIds.append(id);
        }
    }

    std::ranges::sort(result.unknownIds);
    return result;
}
