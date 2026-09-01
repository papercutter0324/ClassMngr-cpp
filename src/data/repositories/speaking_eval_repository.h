#pragma once

#include "core/result.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class SpeakingEvalRepository
{
public:
    explicit SpeakingEvalRepository(
        QSqlDatabase& database
        );
    ~SpeakingEvalRepository();

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

    [[nodiscard]] Result<QList<SpeakingEvalScore>> buildRosterScoreImport(
        int classId,
        const QString& evaluationName
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        ) const;

    QSqlDatabase& m_database;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    mutable QString m_engineDatabasePath;
};
