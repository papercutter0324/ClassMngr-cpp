#include "speaking_eval_validator.h"

#include "core/utils/student_name_utils.h"
#include "domain/validation/shared_validation.h"
#include "domain/validation/validation_rules.h"

#include <QRegularExpression>

#include <utility>

namespace
{
ValidationLocation cellLocation(
    int row,
    int column,
    const QString& field
    )
{
    return {
        .field = field,
        .row = row,
        .column = column
    };
}

QString fieldName(int row, SpeakingEvalColumn column)
{
    return QStringLiteral("rows[%1].%2")
        .arg(row)
        .arg(SpeakingEval::header(column));
}

bool rowHasEditableData(const QStringList& row)
{
    for (int column = 1; column < row.size(); ++column)
    {
        if (!row[column].trimmed().isEmpty())
        {
            return true;
        }
    }

    return false;
}

ValidationResult requiredName(
    const QStringList& row,
    int rowIndex,
    SpeakingEvalColumn column
    )
{
    const int columnIndex = SpeakingEval::toInt(column);
    if (!row.value(columnIndex).trimmed().isEmpty())
    {
        return {};
    }

    return ValidationResult(ValidationRules::issue(
        QStringLiteral("speaking_evaluation.student_name.required"),
        cellLocation(rowIndex, columnIndex, fieldName(rowIndex, column)),
        ValidationSeverity::Error,
        {{QStringLiteral("column"), SpeakingEval::header(column)}}
        ));
}
}

QString SpeakingEvalValidator::normalizedScore(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    QString compact = trimmed.toUpper();
    compact.remove(QRegularExpression(QStringLiteral("\\s+")));

    const QString koreanC(QChar(0x314A));
    const QString koreanB(QChar(0x3160));
    const QString koreanA(QChar(0x3141));

    if (compact == QStringLiteral("1") || compact == koreanC)
    {
        return QStringLiteral("C");
    }
    if (compact == QStringLiteral("2") || compact == koreanB)
    {
        return QStringLiteral("B");
    }
    if (compact == QStringLiteral("3") || compact == koreanB + QLatin1Char('+'))
    {
        return QStringLiteral("B+");
    }
    if (compact == QStringLiteral("4") || compact == koreanA)
    {
        return QStringLiteral("A");
    }
    if (compact == QStringLiteral("5") || compact == koreanA + QLatin1Char('+'))
    {
        return QStringLiteral("A+");
    }
    if (SpeakingEval::scoreValues().contains(compact))
    {
        return compact;
    }

    // A malformed score must remain recognizable to validation; never turn
    // it into another value merely because it contains whitespace.
    return trimmed;
}

SpeakingEvalRows SpeakingEvalValidator::normalized(const SpeakingEvalRows& rows)
{
    SpeakingEvalRows normalized;
    normalized.reserve(rows.size());
    for (const QStringList& sourceRow : rows)
    {
        QStringList row;
        row.reserve(sourceRow.size());
        for (int column = 0; column < sourceRow.size(); ++column)
        {
            if (column >= SpeakingEval::ColumnCount)
            {
                // Keep the structural error visible to validate() instead of
                // silently dropping pasted values outside the supported grid.
                row.append(sourceRow[column]);
                continue;
            }

            const SpeakingEvalColumn columnId =
                SpeakingEval::columnFromInt(column);
            const QString& value = sourceRow[column];
            switch (columnId)
            {
            case SpeakingEvalColumn::EnglishName:
                row.append(StudentNameUtils::normalizeEnglishName(value));
                break;
            case SpeakingEvalColumn::KoreanName:
                row.append(StudentNameUtils::normalizeKoreanName(value));
                break;
            case SpeakingEvalColumn::Grammar:
            case SpeakingEvalColumn::Pronunciation:
            case SpeakingEvalColumn::Fluency:
            case SpeakingEvalColumn::Manner:
            case SpeakingEvalColumn::Content:
            case SpeakingEvalColumn::OverallEffort:
                row.append(normalizedScore(value));
                break;
            case SpeakingEvalColumn::Index:
                row.append(value.trimmed());
                break;
            case SpeakingEvalColumn::Comments:
            case SpeakingEvalColumn::Notes:
                row.append(value);
                break;
            }
        }
        normalized.append(std::move(row));
    }

    return normalized;
}

ValidationResult SpeakingEvalValidator::validate(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    bool allowQuestionableKoreanNameLengths
    )
{
    ValidationResult result;
    const QString normalizedName = evaluationName.trimmed();

    if (classId <= 0)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("speaking_evaluation.class_id.invalid"),
            {.field = QStringLiteral("classId")},
            ValidationSeverity::Error,
            {{QStringLiteral("value"), classId}}
            ));
    }
    result.merge(ValidationRules::textLength(
        normalizedName,
        1,
        MaximumEvaluationNameLength,
        {.field = QStringLiteral("evaluationName")}
        ));

    if (rows.size() > SpeakingEval::RowCount)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("speaking_evaluation.rows.too_many"),
            {.field = QStringLiteral("rows")},
            ValidationSeverity::Error,
            {{QStringLiteral("maximumRows"), SpeakingEval::RowCount},
             {QStringLiteral("rowCount"), rows.size()}}
            ));
    }

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const QStringList& row = rows[rowIndex];
        if (row.size() > SpeakingEval::ColumnCount)
        {
            result.add(ValidationRules::issue(
                QStringLiteral("speaking_evaluation.row.too_many_cells"),
                {.field = QStringLiteral("rows[%1]").arg(rowIndex),
                 .row = rowIndex},
                ValidationSeverity::Error,
                {{QStringLiteral("cellCount"), row.size()},
                 {QStringLiteral("columnCount"), SpeakingEval::ColumnCount}}
                ));
        }

        if (!rowHasEditableData(row))
        {
            continue;
        }

        result.merge(requiredName(
            row, rowIndex, SpeakingEvalColumn::EnglishName));
        result.merge(requiredName(
            row, rowIndex, SpeakingEvalColumn::KoreanName));

        const int englishColumn = SpeakingEval::toInt(
            SpeakingEvalColumn::EnglishName);
        const int koreanColumn = SpeakingEval::toInt(
            SpeakingEvalColumn::KoreanName);
        if (!row.value(englishColumn).trimmed().isEmpty())
        {
            result.merge(SharedValidation::englishName(
                row.value(englishColumn),
                cellLocation(
                    rowIndex,
                    englishColumn,
                    fieldName(rowIndex, SpeakingEvalColumn::EnglishName))));
        }
        if (!row.value(koreanColumn).trimmed().isEmpty())
        {
            result.merge(SharedValidation::koreanName(
                row.value(koreanColumn),
                cellLocation(
                    rowIndex,
                    koreanColumn,
                    fieldName(rowIndex, SpeakingEvalColumn::KoreanName)),
                allowQuestionableKoreanNameLengths));
        }

        for (int column = 0; column < row.size()
             && column < SpeakingEval::ColumnCount; ++column)
        {
            const SpeakingEvalColumn columnId =
                SpeakingEval::columnFromInt(column);
            const QString field = fieldName(rowIndex, columnId);
            const ValidationLocation location = cellLocation(
                rowIndex, column, field);
            if (SpeakingEval::isScoringColumn(columnId)
                && !row[column].trimmed().isEmpty())
            {
                result.merge(ValidationRules::stringEnumValue(
                    row[column], SpeakingEval::scoreValues(), location));
            }
            else if (columnId == SpeakingEvalColumn::Comments)
            {
                result.merge(ValidationRules::textLength(
                    row[column], 0, SpeakingEval::CommentMaxLength, location));
            }
            else if (columnId == SpeakingEvalColumn::Notes)
            {
                result.merge(ValidationRules::textLength(
                    row[column], 0, MaximumNotesLength, location));
            }
        }
    }

    const int englishColumn = SpeakingEval::toInt(
        SpeakingEvalColumn::EnglishName);
    const int koreanColumn = SpeakingEval::toInt(
        SpeakingEvalColumn::KoreanName);
    result.merge(SharedValidation::duplicateNamePairs(
        rows,
        englishColumn,
        koreanColumn,
        QStringLiteral("English Name"),
        QStringLiteral("Korean Name")
        ));

    return result;
}
