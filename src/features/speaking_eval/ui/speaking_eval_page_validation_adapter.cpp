#include "speaking_eval_page_validation_adapter.h"

#include <QVariant>
#include <QVariantList>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

QString fromUtf16(const std::u16string& value)
{
    return QString::fromStdU16String(value);
}

QVariant toVariant(
    const ClassMngr::Next::Application::SpeakingEvaluationValidationArgument& value
    )
{
    using Argument =
        ClassMngr::Next::Application::SpeakingEvaluationValidationArgument;

    return std::visit(
        [](const auto& argument) -> QVariant
        {
            using Value = std::decay_t<decltype(argument)>;
            if constexpr (std::is_same_v<Value, std::int64_t>)
            {
                return QVariant::fromValue(static_cast<qlonglong>(argument));
            }
            else if constexpr (std::is_same_v<Value, std::u16string>)
            {
                return fromUtf16(argument);
            }
            else
            {
                QVariantList converted;
                converted.reserve(static_cast<qsizetype>(argument.size()));
                for (const auto& item : argument)
                {
                    if constexpr (std::is_same_v<
                                      typename Value::value_type,
                                      int
                                      >)
                    {
                        converted.append(item);
                    }
                    else
                    {
                        converted.append(fromUtf16(item));
                    }
                }
                return converted;
            }
        },
        value
        );
}

} // namespace

namespace SpeakingEvalPageValidationAdapter
{

ClassMngr::Next::Application::SpeakingEvaluationSaveRequest makeSaveRequest(
    const int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& changedCells,
    const bool allowQuestionableKoreanNameLengths
    )
{
    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    Q_ASSERT(typedClassId.has_value());

    ClassMngr::Next::Application::SpeakingEvaluationSnapshot snapshot;
    snapshot.rows.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& sourceRow : rows)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& cell : sourceRow)
        {
            row.push_back(cell.toStdU16String());
        }
        snapshot.rows.push_back(std::move(row));
    }

    snapshot.changedCells.reserve(
        static_cast<std::size_t>(changedCells.size())
        );
    for (const SpeakingEvalCellChange& change : changedCells)
    {
        snapshot.changedCells.push_back({change.row, change.column});
    }

    return {
        .classId = *typedClassId,
        .evaluationName = evaluationName.toStdU16String(),
        .evaluation = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = allowQuestionableKoreanNameLengths
    };
}

ValidationResult toFormValidation(
    const ClassMngr::Next::Application::SpeakingEvaluationValidationResult& result
    )
{
    using namespace ClassMngr::Next::Application;

    ValidationIssues issues;
    issues.reserve(static_cast<qsizetype>(result.issues.size()));
    for (const SpeakingEvaluationValidationIssue& issue : result.issues)
    {
        QVariantMap arguments;
        for (const auto& [name, value] : issue.arguments)
        {
            arguments.insert(
                QString::fromStdString(name),
                toVariant(value)
                );
        }

        issues.append({
            .code = QString::fromStdString(issue.code),
            .field = QString::fromStdString(issue.field),
            .row = issue.row,
            .column = issue.column,
            .severity = issue.severity
                    == SpeakingEvaluationValidationSeverity::Warning
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error,
            .arguments = std::move(arguments)
        });
    }
    return ValidationResult(std::move(issues));
}

} // namespace SpeakingEvalPageValidationAdapter
