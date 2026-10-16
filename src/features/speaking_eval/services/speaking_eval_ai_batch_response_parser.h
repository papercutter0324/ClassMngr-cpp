#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct SpeakingEvalAiBatchComment
{
    QString id;
    QString comment;
    bool hadNamePlaceholder = false;
};

struct SpeakingEvalAiBatchParseResult
{
    QList<SpeakingEvalAiBatchComment> comments;
    QStringList duplicateIds;
    QStringList malformedIds;
    QStringList unknownIds;
};

[[nodiscard]] SpeakingEvalAiBatchParseResult parseSpeakingEvalAiBatchResponse(
    const QString& response,
    const QStringList& expectedIds
    );
