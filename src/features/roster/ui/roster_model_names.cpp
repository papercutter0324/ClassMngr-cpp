#include "roster_model.h"

#include "core/utils/student_name_utils.h"

int RosterModel::englishNameColumn() const
{
    return findColumn(
        QStringLiteral("English"),
        m_columns
        );
}

int RosterModel::koreanNameColumn() const
{
    return findColumn(
        QStringLiteral("Korean"),
        m_columns
        );
}

bool RosterModel::isNameColumn(
    int column
    ) const
{
    return column == englishNameColumn()
        || column == koreanNameColumn();
}

QList<int> RosterModel::duplicateNameRows(
    int row
    ) const
{
    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    return StudentNameUtils::duplicateNameRows(
        m_rows,
        row,
        englishColumn,
        koreanColumn
        );
}

QString RosterModel::suggestedKoreanNameWithSuffix(
    int row
    ) const
{
    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    return StudentNameUtils::suggestedKoreanNameWithSuffix(
        m_rows,
        row,
        englishColumn,
        koreanColumn
        );
}

bool RosterModel::hasDuplicateNameErrors() const
{
    return !m_duplicateNameErrorCells.isEmpty();
}

QStringList RosterModel::duplicateNameErrorList() const
{
    QStringList errors;
    QSet<QString> seenRows;

    const int englishColumn =
        englishNameColumn();

    if (englishColumn < 0)
    {
        return errors;
    }

    for (const QString& key : m_duplicateNameErrorCells)
    {
        const QStringList parts =
            key.split(QLatin1Char(':'));

        const int row =
            parts.value(0).toInt();

        if (seenRows.contains(QString::number(row)))
        {
            continue;
        }

        seenRows.insert(QString::number(row));

        errors.append(
            tr("Row %1: duplicate English/Korean student name pair.")
                .arg(row + 1)
            );
    }

    return errors;
}


QString RosterModel::normalizeCell(
    const QString& value,
    int column
    ) const
{
    const QString name =
        columnName(column);

    if (name.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0)
    {
        return normalizeEnglish(value);
    }

    if (name.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0)
    {
        return StudentNameUtils::normalizeKoreanName(value);
    }

    return value.simplified();
}

QString RosterModel::normalizeEnglish(
    const QString& value
    ) const
{
    return StudentNameUtils::normalizeEnglishName(value);
}
