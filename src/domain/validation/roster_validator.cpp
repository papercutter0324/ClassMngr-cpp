#include "roster_validator.h"

#include "core/utils/student_name_utils.h"
#include "domain/validation/shared_validation.h"
#include "domain/validation/validation_rules.h"

#include <QStringList>

#include <utility>

namespace
{
ValidationLocation columnLocation(int column)
{
    return {
        .field = QStringLiteral("columns[%1]").arg(column),
        .column = column
    };
}

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

QString canonicalColumnName(const QString& value)
{
    const QString simplified = value.simplified();
    for (const QString& requiredColumn : Roster::BaseColumns)
    {
        if (requiredColumn.compare(simplified, Qt::CaseInsensitive) == 0)
        {
            return requiredColumn;
        }
    }

    if (simplified.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0)
    {
        return QStringLiteral("Fall");
    }

    return simplified;
}

int columnIndex(const QStringList& columns, const QString& name)
{
    for (int index = 0; index < columns.size(); ++index)
    {
        if (columns[index].compare(name, Qt::CaseInsensitive) == 0)
        {
            return index;
        }
    }

    return -1;
}

bool rowHasData(const QStringList& row)
{
    for (const QString& value : row)
    {
        if (!value.trimmed().isEmpty())
        {
            return true;
        }
    }

    return false;
}

ValidationResult requiredName(
    const QStringList& row,
    int rowIndex,
    int column,
    const QString& field
    )
{
    if (column >= 0 && !row.value(column).trimmed().isEmpty())
    {
        return {};
    }

    return ValidationResult(ValidationRules::issue(
        QStringLiteral("roster.student_name.required"),
        cellLocation(rowIndex, column, field),
        ValidationSeverity::Error,
        {{QStringLiteral("field"), field}}
        ));
}
}

Roster RosterValidator::normalized(const Roster& roster)
{
    Roster normalized = roster;
    normalized.columns.clear();
    normalized.columns.reserve(roster.columns.size());
    for (const QString& column : roster.columns)
    {
        normalized.columns.append(canonicalColumnName(column));
    }

    const int englishColumn = columnIndex(
        normalized.columns, QStringLiteral("English"));
    const int koreanColumn = columnIndex(
        normalized.columns, QStringLiteral("Korean"));

    normalized.rows.clear();
    normalized.rows.reserve(roster.rows.size());
    for (const QStringList& sourceRow : roster.rows)
    {
        QStringList row;
        row.reserve(sourceRow.size());
        for (int column = 0; column < sourceRow.size(); ++column)
        {
            const QString& value = sourceRow[column];
            if (column == englishColumn)
            {
                row.append(StudentNameUtils::normalizeEnglishName(value));
            }
            else if (column == koreanColumn)
            {
                row.append(StudentNameUtils::normalizeKoreanName(value));
            }
            else
            {
                row.append(value.simplified());
            }
        }
        normalized.rows.append(std::move(row));
    }

    return normalized;
}

ValidationResult RosterValidator::validate(const Roster& roster)
{
    ValidationResult result;
    const int englishColumn = columnIndex(
        roster.columns, QStringLiteral("English"));
    const int koreanColumn = columnIndex(
        roster.columns, QStringLiteral("Korean"));

    for (const QString& requiredColumn : Roster::BaseColumns)
    {
        if (columnIndex(roster.columns, requiredColumn) < 0)
        {
            result.add(ValidationRules::issue(
                QStringLiteral("roster.column.required"),
                {.field = QStringLiteral("columns")},
                ValidationSeverity::Error,
                {{QStringLiteral("column"), requiredColumn}}
                ));
        }
    }

    for (int column = 0; column < roster.columns.size(); ++column)
    {
        const QString& name = roster.columns[column];
        const ValidationLocation location = columnLocation(column);
        result.merge(ValidationRules::textLength(
            name, 1, MaximumColumnNameLength, location));

        for (int previous = 0; previous < column; ++previous)
        {
            if (name.compare(roster.columns[previous], Qt::CaseInsensitive) == 0)
            {
                result.add(ValidationRules::issue(
                    QStringLiteral("roster.column.duplicate"),
                    location,
                    ValidationSeverity::Error,
                    {{QStringLiteral("duplicateColumn"), previous}}
                    ));
                break;
            }
        }
    }

    if (roster.columnWidths.size() > roster.columns.size())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("roster.column_widths.invalid_count"),
            {.field = QStringLiteral("columnWidths")},
            ValidationSeverity::Error,
            {{QStringLiteral("widthCount"), roster.columnWidths.size()},
             {QStringLiteral("columnCount"), roster.columns.size()}}
            ));
    }

    if (roster.rows.size() > MaximumRows)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("roster.rows.too_many"),
            {.field = QStringLiteral("rows")},
            ValidationSeverity::Error,
            {{QStringLiteral("maximumRows"), MaximumRows},
             {QStringLiteral("rowCount"), roster.rows.size()}}
            ));
    }

    for (int rowIndex = 0; rowIndex < roster.rows.size(); ++rowIndex)
    {
        const QStringList& row = roster.rows[rowIndex];
        if (row.size() > roster.columns.size())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("roster.row.too_many_cells"),
                {.field = QStringLiteral("rows[%1]").arg(rowIndex),
                 .row = rowIndex},
                ValidationSeverity::Error,
                {{QStringLiteral("cellCount"), row.size()},
                 {QStringLiteral("columnCount"), roster.columns.size()}}
                ));
        }

        for (int column = 0; column < row.size(); ++column)
        {
            const QString field = column < roster.columns.size()
                ? QStringLiteral("rows[%1].%2")
                    .arg(rowIndex)
                    .arg(roster.columns[column])
                : QStringLiteral("rows[%1].cells[%2]").arg(rowIndex).arg(column);
            result.merge(ValidationRules::textLength(
                row[column], 0, MaximumCellLength,
                cellLocation(rowIndex, column, field)));
        }

        if (!rowHasData(row))
        {
            continue;
        }

        result.merge(requiredName(
            row, rowIndex, englishColumn,
            QStringLiteral("rows[%1].English").arg(rowIndex)));
        result.merge(requiredName(
            row, rowIndex, koreanColumn,
            QStringLiteral("rows[%1].Korean").arg(rowIndex)));

        if (englishColumn >= 0 && !row.value(englishColumn).trimmed().isEmpty())
        {
            result.merge(SharedValidation::englishName(
                row.value(englishColumn),
                cellLocation(
                    rowIndex,
                    englishColumn,
                    QStringLiteral("rows[%1].English").arg(rowIndex))));
        }
        if (koreanColumn >= 0 && !row.value(koreanColumn).trimmed().isEmpty())
        {
            result.merge(SharedValidation::koreanName(
                row.value(koreanColumn),
                cellLocation(
                    rowIndex,
                    koreanColumn,
                    QStringLiteral("rows[%1].Korean").arg(rowIndex))));
        }
    }

    if (englishColumn >= 0 && koreanColumn >= 0)
    {
        result.merge(SharedValidation::duplicateNamePairs(
            roster.rows,
            englishColumn,
            koreanColumn,
            QStringLiteral("English"),
            QStringLiteral("Korean")
            ));
    }

    return result;
}
