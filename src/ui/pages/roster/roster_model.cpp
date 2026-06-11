#include "roster_model.h"

#include "ui/pages/roster/roster_constants.h"

#include <QRegularExpression>

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

    const QString normalized =
        normalizeCell(
            value.toString(),
            index.column()
            );

    if (m_rows[index.row()][index.column()] == normalized)
    {
        validateCellAt(
            index.row(),
            index.column()
            );

        validateRawInput(
            index.row(),
            index.column(),
            value.toString()
            );

        emit dataChanged(
            index,
            index,
            { Qt::ToolTipRole }
            );

        return true;
    }

    m_rows[index.row()][index.column()] =
        normalized;

    m_dirtyCells.insert(
        cellKey(
            index.row(),
            index.column()
            )
        );

    validateCellAt(
        index.row(),
        index.column()
        );

    validateRawInput(
        index.row(),
        index.column(),
        value.toString()
        );

    emit dataChanged(
        index,
        index,
        {
            Qt::DisplayRole,
            Qt::EditRole,
            Qt::ToolTipRole
        }
        );

    setDirty(true);

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
        itemFlags |= Qt::ItemIsEditable;
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

QString RosterModel::normalizedColumnName(
    const QString& name
    ) const
{
    return name.simplified();
}

int RosterModel::findColumn(
    const QString& name,
    const QStringList& columns
    ) const
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
    QString normalized;
    normalized.reserve(value.size());

    for (const QChar& character : value)
    {
        const ushort code =
            character.unicode();

        if (code >= 0xAC00 && code <= 0xD7A3)
        {
            normalized.append(character);
        }
    }

    return normalized;
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
            value.size();

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
