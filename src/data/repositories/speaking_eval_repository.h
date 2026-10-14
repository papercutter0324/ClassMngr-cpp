#pragma once

#include "core/result.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <expected>

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
