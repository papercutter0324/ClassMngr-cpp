#pragma once

#include "core/result.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>
#include <expected>
#include <functional>

struct SpeakingEvalNamedRows
{
    QString evaluationName;
    SpeakingEvalRows rows;
};

class SpeakingEvalRepository
{
public:
    struct SaveFailure final
    {
        enum class Kind
        {
            ParentClassMissing,
            Technical
        };

        Kind kind = Kind::Technical;
        QString diagnostic;
    };

    using SaveResult = std::expected<void, SaveFailure>;

    struct ReadFailure final
    {
        enum class Kind
        {
            ParentClassMissing,
            Technical
        };

        Kind kind = Kind::Technical;
        QString diagnostic;
    };

    using ReadResult = std::expected<SpeakingEvalRows, ReadFailure>;
    using BatchReadResult =
        std::expected<QList<SpeakingEvalNamedRows>, ReadFailure>;
    using ReadStreamResult = std::expected<void, ReadFailure>;
    using BatchReadStreamResult = std::expected<void, ReadFailure>;
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

    [[nodiscard]] Status saveSpeakingEval(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    // Typed callers can distinguish a missing parent class without changing
    // the legacy Status/QString API or its foreign-key failure behavior.
    [[nodiscard]] SaveResult saveSpeakingEvalDetailed(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows,
        const QList<SpeakingEvalCellChange>& dirtyCells = {}
        );

    [[nodiscard]] Result<SpeakingEvalRows> loadSpeakingEval(
        int classId,
        const QString& evaluationName
        );

    // Typed callers can distinguish a parent class confirmed absent by the
    // result query without changing the legacy QString-error API.
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

    [[nodiscard]] Result<QList<SpeakingEvalNamedRows>> loadSpeakingEvalBatch(
        int classId,
        const QStringList& evaluationNames
        );

    // Typed callers can distinguish a parent class confirmed absent by the
    // batch result query without changing the legacy batch API behavior.
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
    [[nodiscard]] Status forEachClassAnalyticsEvaluationRow(
        int classId,
        const std::array<QString, 4>& evaluationNames,
        const std::function<void(const ClassAnalyticsEvaluationRow&)>& consumer
        );

    [[nodiscard]] Result<QList<SpeakingEvalScore>> buildRosterScoreImport(
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
