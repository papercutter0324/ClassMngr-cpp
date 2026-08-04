#include "speaking_eval_ai_prompt.h"

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
    const SpeakingEvalAiPromptInput& input
    )
{
    for (
        const QString& studentName :
        {
            input.englishName.trimmed(),
            input.koreanName.trimmed()
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
    QString normalized =
        classGrade.trimmed().toUpper();
    if (normalized.startsWith(QLatin1Char('E')))
    {
        normalized.remove(0, 1);
    }

    bool validNumber = false;
    const int grade =
        normalized.toInt(&validNumber);
    return validNumber && grade >= 4 && grade <= 6
        ? grade
        : 0;
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

    const QString voiceInstruction =
        input.voice == AiCommentVoice::ThirdPerson
        ? QStringLiteral(
            "Write for a parent or guardian, use STD_NAME, "
            "and use they/their rather than guessing gender."
            )
        : QStringLiteral(
            "Address the student directly as \"you\" and "
            "use STD_NAME naturally."
            );

    return QStringLiteral(
        "Write one polished speaking-evaluation comment for a %1-grade "
        "elementary ESL student.\n\n"
        "Requirements:\n"
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
        "- %2\n"
        "- Do not use headings, bullet points, quotation marks, or "
        "a character-count annotation.\n"
        "- Return only the finished comment.\n\n"
        "Did well:\n"
        "%3\n\n"
        "Needs improvement:\n"
        "%4"
        ).arg(
            gradeOrdinal(input.grade),
            voiceInstruction,
            promptItems(input.didWell, input),
            promptItems(input.needsImprovement, input)
            );
}
