#include "roster_model.h"

#include "features/roster/ui/roster_constants.h"

#include <QRegularExpression>

#include <algorithm>

RosterModel::RosterModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
    Roster roster;
    setRoster(roster);
}

int RosterModel::rowCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return RosterUi::RowCount;
}

int RosterModel::columnCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_columns.size();
}

QVariant RosterModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (!index.isValid())
    {
        return {};
    }

    if (
        index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= m_columns.size()
        )
    {
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return m_rows[index.row()][index.column()];
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    if (role == Qt::ToolTipRole)
    {
        const QStringList errors =
            errorsForCell(
                index.row(),
                index.column()
                );

        if (!errors.isEmpty())
        {
            return errors.join(QLatin1Char('\n'));
        }
    }

    return {};
}

bool RosterModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role
    )
{
    if (
        role != Qt::EditRole
        || !index.isValid()
        || index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= m_columns.size()
        )
    {
        return false;
    }

    const QHash<QString, QStringList> oldErrors =
        m_validationErrors;

    const QString normalized =
        normalizeCell(
            value.toString(),
            index.column()
            );

    const bool changed =
        m_rows[index.row()][index.column()] != normalized;

    if (changed)
    {
        m_rows[index.row()][index.column()] =
            normalized;

        m_dirtyCells.insert(
            cellKey(
                index.row(),
                index.column()
                )
            );
    }

    validateCellAt(
        index.row(),
        index.column()
        );
    validateRawInput(
        index.row(),
        index.column(),
        value.toString()
        );
    validateDuplicateNames();

    const bool validationChanged =
        oldErrors != m_validationErrors;

    if (!changed && !validationChanged)
    {
        return false;
    }

    emit dataChanged(
        index,
        index,
        {
            Qt::DisplayRole,
            Qt::EditRole,
            Qt::ToolTipRole
        }
        );

    if (changed)
    {
        setDirty(true);
    }

    return true;
}

Qt::ItemFlags RosterModel::flags(
    const QModelIndex& index
    ) const
{
    Qt::ItemFlags itemFlags =
        QAbstractTableModel::flags(index);

    if (index.isValid())
    {
        const QString column =
            columnName(
                index.column()
                );

        if (!RosterUi::isEvaluationColumn(column))
        {
            itemFlags |= Qt::ItemIsEditable;
        }
    }

    return itemFlags;
}

QVariant RosterModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role
    ) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation == Qt::Horizontal)
    {
        return columnName(section);
    }

    return section + 1;
}

void RosterModel::setRoster(
    const Roster& roster
    )
{
    beginResetModel();

    m_columns =
        Roster::BaseColumns;

    for (const QString& column : roster.columns)
    {
        const QString normalized =
            normalizedColumnName(column);

        if (
            normalized.isEmpty()
            || isRequiredColumn(normalized)
            || findColumn(normalized, m_columns) >= 0
            )
        {
            continue;
        }

        m_columns.append(normalized);
    }

    rebuildRows(roster);
    validateAll();

    m_dirtyCells.clear();
    m_dirty = false;

    endResetModel();

    emit dirtyChanged(false);
}

Roster RosterModel::toRoster() const
{
    Roster roster;
    roster.columns = m_columns;
    roster.rows = m_rows;

    return roster;
}

QString RosterModel::columnName(
    int column
    ) const
{
    if (column < 0 || column >= m_columns.size())
    {
        return {};
    }

    return m_columns[column];
}

bool RosterModel::isRequiredColumn(
    int column
    ) const
{
    return isRequiredColumn(
        columnName(column)
        );
}

bool RosterModel::isRequiredColumn(
    const QString& name
    ) const
{
    return RosterUi::isRequiredColumn(name);
}

bool RosterModel::canAddColumn(
    const QString& name,
    QString* reason
    ) const
{
    const QString normalized =
        normalizedColumnName(name);

    if (normalized.isEmpty())
    {
        if (reason)
        {
            *reason = tr("Column name cannot be empty.");
        }

        return false;
    }

    if (findColumn(normalized, m_columns) >= 0)
    {
        if (reason)
        {
            *reason = tr("A column with that name already exists.");
        }

        return false;
    }

    if (isRequiredColumn(normalized))
    {
        if (reason)
        {
            *reason = tr("Required roster columns already exist.");
        }

        return false;
    }

    return true;
}

bool RosterModel::insertCustomColumn(
    const QString& name
    )
{
    QString reason;

    if (!canAddColumn(name, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    const QString normalized =
        normalizedColumnName(name);

    const int column =
        m_columns.size();

    beginInsertColumns(
        QModelIndex(),
        column,
        column
        );

    m_columns.append(normalized);

    for (QStringList& row : m_rows)
    {
        row.append(QString());
    }

    endInsertColumns();

    validateAll();
    setDirty(true);

    return true;
}

bool RosterModel::canRemoveColumn(
    int column,
    QString* reason
    ) const
{
    if (column < 0 || column >= m_columns.size())
    {
        if (reason)
        {
            *reason = tr("Select a custom column to remove.");
        }

        return false;
    }

    if (isRequiredColumn(column))
    {
        if (reason)
        {
            *reason = tr("Required roster columns cannot be removed.");
        }

        return false;
    }

    return true;
}

bool RosterModel::removeRosterColumn(
    int column
    )
{
    QString reason;

    if (!canRemoveColumn(column, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    beginRemoveColumns(
        QModelIndex(),
        column,
        column
        );

    m_columns.removeAt(column);

    for (QStringList& row : m_rows)
    {
        if (column >= 0 && column < row.size())
        {
            row.removeAt(column);
        }
    }

    endRemoveColumns();

    validateAll();
    setDirty(true);

    return true;
}

bool RosterModel::canRemoveRow(
    int row,
    QString* reason
    ) const
{
    if (row < 0 || row >= m_rows.size())
    {
        if (reason)
        {
            *reason = tr("Select a student row to remove.");
        }

        return false;
    }

    const bool hasData =
        std::any_of(
            m_rows[row].constBegin(),
            m_rows[row].constEnd(),
            [](const QString& value)
            {
                return !value.trimmed().isEmpty();
            }
            );

    if (!hasData)
    {
        if (reason)
        {
            *reason = tr("Selected row is already empty.");
        }

        return false;
    }

    return true;
}

bool RosterModel::removeRosterRow(
    int row
    )
{
    QString reason;

    if (!canRemoveRow(row, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    const int lastRow =
        m_rows.size() - 1;

    for (int sourceRow = row + 1; sourceRow <= lastRow; ++sourceRow)
    {
        m_rows[sourceRow - 1] =
            m_rows[sourceRow];
    }

    m_rows[lastRow] =
        QStringList(
            m_columns.size(),
            QString()
            );

    validateAll();

    if (!m_columns.isEmpty())
    {
        emit dataChanged(
            index(
                0,
                0
                ),
            index(
                lastRow,
                m_columns.size() - 1
                ),
            {
                Qt::DisplayRole,
                Qt::EditRole,
                Qt::ToolTipRole
            }
            );
    }

    setDirty(true);

    return true;
}

bool RosterModel::canMoveRow(
    int sourceRow,
    int destinationRow,
    QString* reason
    ) const
{
    if (sourceRow < 0 || sourceRow >= m_rows.size())
    {
        if (reason)
        {
            *reason = tr("Select a student row to move.");
        }

        return false;
    }

    if (destinationRow < 0 || destinationRow >= m_rows.size())
    {
        if (reason)
        {
            *reason = tr("Drop the student on another roster row.");
        }

        return false;
    }

    if (sourceRow == destinationRow)
    {
        if (reason)
        {
            *reason = tr("Drop the student on a different row.");
        }

        return false;
    }

    const bool hasData =
        std::any_of(
            m_rows[sourceRow].constBegin(),
            m_rows[sourceRow].constEnd(),
            [](const QString& value)
            {
                return !value.trimmed().isEmpty();
            }
            );

    if (!hasData)
    {
        if (reason)
        {
            *reason = tr("Selected row is empty.");
        }

        return false;
    }

    return true;
}

bool RosterModel::moveRosterRow(
    int sourceRow,
    int destinationRow
    )
{
    QString reason;

    if (!canMoveRow(sourceRow, destinationRow, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    const QStringList movedRow =
        m_rows.takeAt(sourceRow);

    m_rows.insert(
        destinationRow,
        movedRow
        );

    validateAll();

    if (!m_columns.isEmpty())
    {
        emit dataChanged(
            index(
                0,
                0
                ),
            index(
                m_rows.size() - 1,
                m_columns.size() - 1
                ),
            {
                Qt::DisplayRole,
                Qt::EditRole,
                Qt::ToolTipRole
            }
            );
    }

    setDirty(true);

    return true;
}

bool RosterModel::isDirty() const
{
    return m_dirty;
}

void RosterModel::clearDirty()
{
    m_dirtyCells.clear();
    setDirty(false);
}

QStringList RosterModel::errorsForCell(
    int row,
    int column
    ) const
{
    return m_validationErrors.value(
        cellKey(row, column)
        );
}

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
    QList<int> rows;

    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    if (
        row < 0
        || row >= m_rows.size()
        || englishColumn < 0
        || koreanColumn < 0
        )
    {
        return rows;
    }

    const QString englishName =
        m_rows[row][englishColumn].trimmed();

    const QString koreanName =
        m_rows[row][koreanColumn].trimmed();

    if (englishName.isEmpty() || koreanName.isEmpty())
    {
        return rows;
    }

    const QString key =
        namePairKey(
            englishName,
            koreanName
            );

    for (int candidateRow = 0; candidateRow < m_rows.size(); ++candidateRow)
    {
        if (candidateRow == row)
        {
            continue;
        }

        if (
            namePairKey(
                m_rows[candidateRow][englishColumn],
                m_rows[candidateRow][koreanColumn]
                ) == key
            )
        {
            rows.append(candidateRow);
        }
    }

    return rows;
}

QString RosterModel::suggestedKoreanNameWithSuffix(
    int row
    ) const
{
    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    if (
        row < 0
        || row >= m_rows.size()
        || englishColumn < 0
        || koreanColumn < 0
        )
    {
        return {};
    }

    const QString englishName =
        m_rows[row][englishColumn].trimmed();

    const QString baseName =
        baseKoreanName(
            m_rows[row][koreanColumn]
            );

    if (englishName.isEmpty() || baseName.isEmpty())
    {
        return {};
    }

    QSet<QChar> usedSuffixes;

    for (const QStringList& candidateRow : m_rows)
    {
        if (
            englishName.compare(
                candidateRow.value(englishColumn).trimmed(),
                Qt::CaseSensitive
                ) != 0
            || baseKoreanName(
                candidateRow.value(koreanColumn)
                ) != baseName
            )
        {
            continue;
        }

        const QString suffix =
            koreanNameSuffix(
                candidateRow.value(koreanColumn)
                );

        if (suffix.size() == 1)
        {
            usedSuffixes.insert(suffix.front());
        }
    }

    for (int suffix = 'A'; suffix <= 'Z'; ++suffix)
    {
        const QChar suffixCharacter(suffix);

        if (!usedSuffixes.contains(suffixCharacter))
        {
            return QStringLiteral("%1(%2)")
                .arg(baseName)
                .arg(suffixCharacter);
        }
    }

    return {};
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

QString RosterModel::normalizedColumnName(
    const QString& name
    ) const
{
    const QString normalized =
        name.simplified();

    if (normalized.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0)
    {
        return QStringLiteral("Fall");
    }

    return normalized;
}

int RosterModel::findColumn(
    const QString& name,
    const QStringList& columns
    ) const
{
    for (int index = 0; index < columns.size(); ++index)
    {
        if (
            normalizedColumnName(columns[index])
                .compare(
                    normalizedColumnName(name),
                    Qt::CaseInsensitive
                    ) == 0
            )
        {
            return index;
        }
    }

    return -1;
}

void RosterModel::rebuildRows(
    const Roster& roster
    )
{
    m_rows.clear();

    for (int rowIndex = 0; rowIndex < RosterUi::RowCount; ++rowIndex)
    {
        QStringList row;
        row.reserve(m_columns.size());

        const QStringList sourceRow =
            rowIndex < roster.rows.size()
                ? roster.rows[rowIndex]
                : QStringList();

        for (int column = 0; column < m_columns.size(); ++column)
        {
            const int sourceColumn =
                findColumn(
                    m_columns[column],
                    roster.columns
                    );

            const QString value =
                sourceColumn >= 0 && sourceColumn < sourceRow.size()
                    ? sourceRow[sourceColumn]
                    : QString();

            row.append(
                normalizeCell(
                    value,
                    column
                    )
                );
        }

        m_rows.append(row);
    }
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
        return normalizeKorean(value);
    }

    return value.simplified();
}

QString RosterModel::normalizeEnglish(
    const QString& value
    ) const
{
    QString filtered;
    filtered.reserve(value.size());

    for (const QChar& character : value)
    {
        const ushort code =
            character.unicode();

        if (
            (code >= 'A' && code <= 'Z')
            || (code >= 'a' && code <= 'z')
            || code == '.'
            || code == '-'
            )
        {
            filtered.append(character);
            continue;
        }

        if (character.isSpace())
        {
            filtered.append(QLatin1Char(' '));
        }
    }

    QString normalized =
        filtered.simplified();

    normalized.replace(
        QRegularExpression(QStringLiteral("\\s*-\\s*")),
        QStringLiteral("-")
        );

    normalized.replace(
        QRegularExpression(QStringLiteral("\\s*\\.\\s*")),
        QStringLiteral(".")
        );

    normalized.replace(
        QRegularExpression(QStringLiteral("-+")),
        QStringLiteral("-")
        );

    normalized.replace(
        QRegularExpression(QStringLiteral("\\.+")),
        QStringLiteral(".")
        );

    QStringList tokens =
        normalized.split(
            QLatin1Char(' '),
            Qt::SkipEmptyParts
            );

    for (QString& token : tokens)
    {
        token =
            normalizeEnglishToken(token);
    }

    return tokens.join(QLatin1Char(' '));
}

QString RosterModel::normalizeEnglishToken(
    const QString& token
    ) const
{
    QStringList hyphenParts =
        token.split(
            QLatin1Char('-'),
            Qt::SkipEmptyParts
            );

    for (QString& part : hyphenParts)
    {
        if (part.contains(QLatin1Char('.')))
        {
            const QStringList dotParts =
                part.split(
                    QLatin1Char('.'),
                    Qt::SkipEmptyParts
                    );

            bool initials = !dotParts.isEmpty();

            for (const QString& dotPart : dotParts)
            {
                if (dotPart.size() != 1)
                {
                    initials = false;
                    break;
                }
            }

            if (initials)
            {
                QString rebuilt;

                for (const QString& dotPart : dotParts)
                {
                    rebuilt += dotPart.left(1).toUpper();
                    rebuilt += QLatin1Char('.');
                }

                part = rebuilt;
                continue;
            }
        }

        const QString lower =
            part.toLower();

        part =
            lower.left(1).toUpper()
            + lower.mid(1);
    }

    return hyphenParts.join(QLatin1Char('-'));
}

QString RosterModel::normalizeKorean(
    const QString& value
    ) const
{
    const QRegularExpression suffixExpression(
        QStringLiteral("\\(([A-Za-z])\\)\\s*$")
        );

    const QRegularExpressionMatch suffixMatch =
        suffixExpression.match(value);

    const QString suffix =
        suffixMatch.hasMatch()
            ? suffixMatch.captured(1).toUpper()
            : QString();

    const QString source =
        suffixMatch.hasMatch()
            ? value.left(suffixMatch.capturedStart())
            : value;

    QString normalized;
    normalized.reserve(source.size());

    for (const QChar& character : source)
    {
        const ushort code =
            character.unicode();

        if (code >= 0xAC00 && code <= 0xD7A3)
        {
            normalized.append(character);
        }
    }

    if (!normalized.isEmpty() && suffix.size() == 1)
    {
        normalized += QStringLiteral("(%1)")
            .arg(suffix);
    }

    return normalized;
}

QString RosterModel::baseKoreanName(
    const QString& value
    ) const
{
    QString normalized =
        normalizeKorean(value);

    normalized.remove(
        QRegularExpression(QStringLiteral("\\([A-Z]\\)$"))
        );

    return normalized;
}

QString RosterModel::koreanNameSuffix(
    const QString& value
    ) const
{
    const QRegularExpression suffixExpression(
        QStringLiteral("\\(([A-Z])\\)$")
        );

    const QRegularExpressionMatch match =
        suffixExpression.match(
            normalizeKorean(value)
            );

    return match.hasMatch()
        ? match.captured(1)
        : QString();
}

QString RosterModel::namePairKey(
    const QString& englishName,
    const QString& koreanName
    ) const
{
    if (englishName.trimmed().isEmpty() || koreanName.trimmed().isEmpty())
    {
        return {};
    }

    return englishName.trimmed()
        + QChar(0x001F)
        + koreanName.trimmed();
}

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

QString RosterModel::cellKey(
    int row,
    int column
    ) const
{
    return QStringLiteral("%1:%2")
        .arg(row)
        .arg(column);
}

void RosterModel::setDirty(
    bool dirty
    )
{
    if (m_dirty == dirty)
    {
        return;
    }

    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}
