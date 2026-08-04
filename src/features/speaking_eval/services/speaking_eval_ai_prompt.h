#pragma once

#include "ui/shared/constants/options.h"

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
