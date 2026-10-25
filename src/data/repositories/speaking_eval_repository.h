#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>
#include <functional>

struct SpeakingEvalNamedRows
{
    QString evaluationName;
    SpeakingEvalRows rows;
};

class SpeakingEvalRepository
{
public:
    using SaveResult = RepositoryStatus;
    using ReadResult = RepositoryResult<SpeakingEvalRows>;
    using BatchReadResult = RepositoryResult<QList<SpeakingEvalNamedRows>>;
    using ReadStreamResult = RepositoryStatus;
    using BatchReadStreamResult = RepositoryStatus;
    using ReadRowConsumer = std::function<void(const QStringList&)>;
    using BatchReadRowConsumer =
        std::function<void(qsizetype, const QStringList&)>;

    struct ClassAnalyticsEvaluationRow final
    {
        std::size_t evaluationIndex = 0;
        QString englishName;
        QString koreanName;
        std::array<QString, 6> scores;
    };

    explicit SpeakingEvalRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryStatus saveSpeakingEval(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    // Detailed callers distinguish a parent class confirmed absent by an
    // existence query; the legacy save path keeps its foreign-key diagnostic.
    [[nodiscard]] SaveResult saveSpeakingEvalDetailed(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    [[nodiscard]] RepositoryResult<SpeakingEvalRows> loadSpeakingEval(
        int classId,
        const QString& evaluationName
        );

    // Detailed callers distinguish a parent class confirmed absent by the
    // result query; the legacy read keeps missing results as successful empty
    // rows.
    [[nodiscard]] ReadResult loadSpeakingEvalDetailed(
        int classId,
        const QString& evaluationName
        );

    // Streams validated rows in persisted row order. The const QStringList&
    // row is borrowed and valid only for the callback duration; copy it if it
    // must be retained. Rows delivered before a failure are provisional, so
    // consumers must only publish accumulated results after success. Consumer
    // exceptions propagate.
    [[nodiscard]] ReadStreamResult forEachSpeakingEvalRowDetailed(
        int classId,
        const QString& evaluationName,
        const ReadRowConsumer& consumer
        );

    [[nodiscard]] RepositoryResult<QList<SpeakingEvalNamedRows>>
    loadSpeakingEvalBatch(
        int classId,
        const QStringList& evaluationNames
        );

    // Detailed callers distinguish a parent class confirmed absent by the
    // batch result query; the legacy batch keeps missing results as empty
    // requested rows.
    [[nodiscard]] BatchReadResult loadSpeakingEvalBatchDetailed(
        int classId,
        const QStringList& evaluationNames
        );

    // Streams validated rows in requested evaluation order and persisted row
    // order. Each callback receives the zero-based requested evaluation
    // position and a borrowed const QStringList& row that is valid only for
    // the callback duration; copy it if it must be retained. Rows delivered
    // before a failure are provisional. Consumer exceptions propagate.
    [[nodiscard]] BatchReadStreamResult forEachSpeakingEvalBatchRowDetailed(
        int classId,
        const QStringList& evaluationNames,
        const BatchReadRowConsumer& consumer
        );

    // Streams the eight dashboard cells in request order, then persisted row
    // order, without materializing sparse gaps or requiring a parent class.
    // Consumer exceptions propagate to the caller.
    [[nodiscard]] RepositoryStatus forEachClassAnalyticsEvaluationRow(
        int classId,
        const std::array<QString, 4>& evaluationNames,
        const std::function<void(const ClassAnalyticsEvaluationRow&)>& consumer
        );

    [[nodiscard]] RepositoryResult<QList<SpeakingEvalScore>>
    buildRosterScoreImport(
        int classId,
        const QString& evaluationName
        );

private:
    enum class ParentClassPolicy
    {
        PreserveLegacyForeignKey,
        ClassifyMissingParent
    };

    [[nodiscard]] SaveResult saveSpeakingEvalInternal(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells,
        ParentClassPolicy parentClassPolicy
        );

    QSqlDatabase& m_database;
};
