#pragma once

#include "ui/shared/constants/options.h"

#include <QList>
#include <QString>
#include <QStringList>

struct SpeakingEvalAiPromptInput
{
    int grade = 0;
    QString englishName;
    QString koreanName;
    QString didWell;
    QString needsImprovement;
    AiCommentVoice voice =
        AiCommentVoice::DirectToStudent;
};

struct SpeakingEvalAiBatchStudentInput
{
    QString id;
    int grade = 0;
    QString englishName;
    QString koreanName;
    QString didWell;
    QString needsImprovement;
};

struct SpeakingEvalAiBatchPromptInput
{
    QList<SpeakingEvalAiBatchStudentInput> students;
    QStringList additionalNamesToRedact;
    AiCommentVoice voice =
        AiCommentVoice::DirectToStudent;
};

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

[[nodiscard]] int speakingEvalElementaryGrade(
    const QString& classGrade
    );

[[nodiscard]] QStringList speakingEvalAiObservationItems(
    const QString& observations
    );

[[nodiscard]] bool canBuildSpeakingEvalAiPrompt(
    const SpeakingEvalAiPromptInput& input
    );

[[nodiscard]] QString buildSpeakingEvalAiCommentPrompt(
    const SpeakingEvalAiPromptInput& input
    );

[[nodiscard]] QString buildSpeakingEvalAiBatchCommentPrompt(
    const SpeakingEvalAiBatchPromptInput& input
    );

[[nodiscard]] SpeakingEvalAiBatchParseResult
parseSpeakingEvalAiBatchResponse(
    const QString& response,
    const QStringList& expectedIds
    );
