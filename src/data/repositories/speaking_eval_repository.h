#pragma once

#include "core/result.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

struct SpeakingEvalNamedRows
{
    QString evaluationName;
    SpeakingEvalRows rows;
};

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

    [[nodiscard]] Result<SpeakingEvalRows> loadSpeakingEval(
        int classId,
        const QString& evaluationName
        );

    [[nodiscard]] Result<QList<SpeakingEvalNamedRows>> loadSpeakingEvalBatch(
        int classId,
        const QStringList& evaluationNames
        );

    [[nodiscard]] Result<QList<SpeakingEvalScore>> buildRosterScoreImport(
        int classId,
        const QString& evaluationName
        );

private:
    QSqlDatabase& m_database;
};
