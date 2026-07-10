#include "roster_model.h"

#include <algorithm>

QStringList RosterModel::validateCell(
    const QString& value,
    int column
    ) const
{
    QStringList errors;

    const QString name =
        columnName(column);

    if (name.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0)
    {
        if (value.size() > 20)
        {
            errors.append(
                tr("English name should be 20 characters or less.")
                );
        }

        for (const QChar& character : value)
        {
            if (character.unicode() > 127)
            {
                errors.append(
                    tr("English name should use ASCII characters.")
                    );

                break;
            }
        }
    }
    else if (name.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0)
    {
        const int length =
            baseKoreanName(value).size();

        if (length == 0 || length == 3)
        {
            return errors;
        }

        if (length <= 1)
        {
            errors.append(
                tr("Korean name looks too short.")
                );
        }
        else if (length >= 6)
        {
            errors.append(
                tr("Korean name looks too long.")
                );
        }
        else
        {
            errors.append(
                tr("Korean name length is unusual.")
                );
        }
    }

    return errors;
}

void RosterModel::validateAll()
{
    m_validationErrors.clear();
    m_duplicateNameErrorCells.clear();

    for (int row = 0; row < m_rows.size(); ++row)
    {
        for (int column = 0; column < m_columns.size(); ++column)
        {
            validateCellAt(
                row,
                column
            );
        }
    }

    validateDuplicateNames();
}

void RosterModel::validateCellAt(
    int row,
    int column
    )
{
    if (
        row < 0
        || row >= m_rows.size()
        || column < 0
        || column >= m_columns.size()
        )
    {
        return;
    }

    const QString key =
        cellKey(
            row,
            column
            );

    const QStringList errors =
        validateCell(
            m_rows[row][column],
            column
            );

    if (errors.isEmpty())
    {
        m_validationErrors.remove(key);
        return;
    }

    m_validationErrors.insert(
        key,
        errors
        );
}

void RosterModel::validateRawInput(
    int row,
    int column,
    const QString& rawValue
    )
{
    if (columnName(column).compare(QStringLiteral("English"), Qt::CaseInsensitive) != 0)
    {
        return;
    }

    bool hasNonAscii = false;

    for (const QChar& character : rawValue)
    {
        if (character.unicode() > 127)
        {
            hasNonAscii = true;
            break;
        }
    }

    if (!hasNonAscii)
    {
        return;
    }

    const QString key =
        cellKey(
            row,
            column
            );

    QStringList errors =
        m_validationErrors.value(key);

    const QString message =
        tr("English name should use ASCII characters.");

    if (!errors.contains(message))
    {
        errors.append(message);
    }

    m_validationErrors.insert(
        key,
        errors
        );
}

void RosterModel::validateDuplicateNames()
{
    clearDuplicateNameErrors();

    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    if (englishColumn < 0 || koreanColumn < 0)
    {
        return;
    }

    QHash<QString, QList<int>> rowsByPair;

    for (int row = 0; row < m_rows.size(); ++row)
    {
        const QString key =
            namePairKey(
                m_rows[row][englishColumn],
                m_rows[row][koreanColumn]
                );

        if (!key.isEmpty())
        {
            rowsByPair[key].append(row);
        }
    }

    for (auto it = rowsByPair.constBegin(); it != rowsByPair.constEnd(); ++it)
    {
        if (it.value().size() < 2)
        {
            continue;
        }

        for (int row : it.value())
        {
            QStringList duplicateRows;

            for (int otherRow : it.value())
            {
                if (otherRow != row)
                {
                    duplicateRows.append(
                        QString::number(otherRow + 1)
                        );
                }
            }

            const QString message =
                tr("Duplicate student name pair. Also used on row(s): %1.")
                    .arg(duplicateRows.join(QStringLiteral(", ")));

            appendValidationError(
                row,
                englishColumn,
                message,
                true
                );

            appendValidationError(
                row,
                koreanColumn,
                message,
                true
                );
        }
    }
}

void RosterModel::clearDuplicateNameErrors()
{
    const QString messagePrefix =
        tr("Duplicate student name pair.");

    for (const QString& key : m_duplicateNameErrorCells)
    {
        QStringList errors =
            m_validationErrors.value(key);

        errors.erase(
            std::remove_if(
                errors.begin(),
                errors.end(),
                [&messagePrefix](const QString& error)
                {
                    return error.startsWith(messagePrefix);
                }
                ),
            errors.end()
            );

        if (errors.isEmpty())
        {
            m_validationErrors.remove(key);
        }
        else
        {
            m_validationErrors.insert(
                key,
                errors
                );
        }
    }

    m_duplicateNameErrorCells.clear();
}

void RosterModel::appendValidationError(
    int row,
    int column,
    const QString& error,
    bool duplicateNameError
    )
{
    const QString key =
        cellKey(
            row,
            column
            );

    QStringList errors =
        m_validationErrors.value(key);

    if (!errors.contains(error))
    {
        errors.append(error);
    }

    m_validationErrors.insert(
        key,
        errors
        );

    if (duplicateNameError)
    {
        m_duplicateNameErrorCells.insert(key);
    }
}

