#include "speaking_eval_ai_batch_response_parser.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

SpeakingEvalAiBatchParseResult parseSpeakingEvalAiBatchResponse(
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
    auto matches = completeBlock.globalMatch(response);
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        const QString id = match.captured(1);
        ++completeCounts[id];
        if (!expected.contains(id))
        {
            if (!result.unknownIds.contains(id))
            {
                result.unknownIds.append(id);
            }
            continue;
        }

        const QString comment = match.captured(2).trimmed();
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

    auto openings = openingMarker.globalMatch(response);
    QSet<QString> openedIds;
    while (openings.hasNext())
    {
        openedIds.insert(openings.next().captured(1));
    }

    for (const QString& id : expectedIds)
    {
        const int count = completeCounts.value(id);
        if (count > 1)
        {
            result.duplicateIds.append(id);
            continue;
        }
        if (count == 1)
        {
            result.comments.append(parsedById.value(id));
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
