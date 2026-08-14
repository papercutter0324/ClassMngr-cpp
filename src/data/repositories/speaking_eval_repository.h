#pragma once

#include "core/result.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

class SpeakingEvalRepository
{
public:
    explicit SpeakingEvalRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Status saveSpeakingEval(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    SpeakingEvalRows loadSpeakingEval(
        int classId,
        const QString& evaluationName
        );

    QList<SpeakingEvalScore> buildRosterScoreImport(
        int classId,
        const QString& evaluationName
        );

private:
    QSqlDatabase& m_database;
};
