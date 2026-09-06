#include "speaking_eval_validator.h"

#include "classmngr/engine/speaking_evaluation_validator.h"

#include <QByteArray>

#include <string_view>
#include <utility>
#include <vector>

namespace
{
std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::SpeakingEvaluationRows toEngine(
    const SpeakingEvalRows& rows
    )
{
    classmngr::engine::SpeakingEvaluationRows result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& sourceRow : rows)
    {
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& value : sourceRow)
        {
            row.push_back(toUtf8(value));
        }
        result.push_back(std::move(row));
    }
    return result;
}

SpeakingEvalRows fromEngine(
    const classmngr::engine::SpeakingEvaluationRows& rows
    )
{
    SpeakingEvalRows result;
    result.reserve(static_cast<qsizetype>(rows.size()));
    for (const std::vector<std::string>& sourceRow : rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::string& value : sourceRow)
        {
            row.append(fromUtf8(value));
        }
        result.append(row);
    }
    return result;
}

ValidationResult fromEngine(
    const classmngr::engine::ValidationResult& validation
    )
{
    ValidationResult result;
    for (const classmngr::engine::ValidationIssue& source :
         validation.issues())
    {
        result.add({
            .code = fromUtf8(source.code),
            .field = fromUtf8(source.field),
            .row = source.row,
            .column = source.column,
            .severity = source.isWarning()
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
        });
    }

    return result;
}
} // namespace

QString SpeakingEvalValidator::normalizedScore(const QString& value)
{
    return fromUtf8(
        classmngr::engine::SpeakingEvaluationValidator::normalizedScore(
            toUtf8(value)
            )
        );
}

SpeakingEvalRows SpeakingEvalValidator::normalized(
    const SpeakingEvalRows& rows
    )
{
    return fromEngine(
        classmngr::engine::SpeakingEvaluationValidator::normalized(
            toEngine(rows)
            )
        );
}

ValidationResult SpeakingEvalValidator::validate(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    bool allowQuestionableKoreanNameLengths
    )
{
    return fromEngine(
        classmngr::engine::SpeakingEvaluationValidator::validate(
            classId,
            toUtf8(evaluationName),
            toEngine(rows),
            allowQuestionableKoreanNameLengths
            )
        );
}
