#include "speaking_eval_model.h"

#include "core/utils/student_name_utils.h"

#include <QRegularExpression>

SpeakingEvalModel::SpeakingEvalModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
    , m_rows(SpeakingEval::emptyRows())
{
    markSaved();
}

int SpeakingEvalModel::rowCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return SpeakingEval::RowCount;
}

int SpeakingEvalModel::columnCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return SpeakingEval::ColumnCount;
}

QVariant SpeakingEvalModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (
        !index.isValid()
        || index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= SpeakingEval::ColumnCount
        )
    {
        return {};
    }

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (
        role == Qt::DisplayRole
        && column == SpeakingEvalColumn::Index
        )
    {
        return index.row() + 1;
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

bool SpeakingEvalModel::setData(
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
        || index.column() >= SpeakingEval::ColumnCount
        )
    {
        return false;
    }

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (!SpeakingEval::isEditableColumn(column))
    {
        return false;
    }

    const QHash<QString, QStringList> oldErrors =
        m_errors;

    const QString oldValue =
        m_rows[index.row()][index.column()];

    const ProcessedValue processed =
        processValue(
            index.row(),
            index.column(),
            value.toString()
            );

    if (oldValue != processed.normalized)
    {
        m_rows[index.row()][index.column()] =
            processed.normalized;

        m_dirtyCells.insert(
            cellKey(
                index.row(),
                index.column()
                )
            );
        setDirtyState(true);
    }

    revalidateAll();

    if (
        oldValue == processed.normalized
        && oldErrors == m_errors
        )
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

    emit dataModified();

    return true;
}

Qt::ItemFlags SpeakingEvalModel::flags(
    const QModelIndex& index
    ) const
{
    Qt::ItemFlags itemFlags =
        QAbstractTableModel::flags(index);

    if (!index.isValid())
    {
        return itemFlags;
    }

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (SpeakingEval::isEditableColumn(column))
    {
        itemFlags |= Qt::ItemIsEditable;
    }

    return itemFlags;
}

QVariant SpeakingEvalModel::headerData(
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
        switch (SpeakingEval::columnFromInt(section))
        {
        case SpeakingEvalColumn::Index:
            return {};
        case SpeakingEvalColumn::EnglishName:
            return tr("English Name");
        case SpeakingEvalColumn::KoreanName:
            return tr("Korean Name");
        case SpeakingEvalColumn::Grammar:
            return tr("Grammar");
        case SpeakingEvalColumn::Pronunciation:
            return tr("Pronunciation");
        case SpeakingEvalColumn::Fluency:
            return tr("Fluency");
        case SpeakingEvalColumn::Manner:
            return tr("Manner");
        case SpeakingEvalColumn::Content:
            return tr("Content");
        case SpeakingEvalColumn::OverallEffort:
            return tr("Overall Effort");
        case SpeakingEvalColumn::Comments:
            return tr("Comments");
        case SpeakingEvalColumn::Notes:
            return tr("Notes");
        }
    }

    return section + 1;
}

void SpeakingEvalModel::loadData(
    const SpeakingEvalRows& rows
    )
{
    beginResetModel();

    m_rows =
        normalizeStructure(rows);

    revalidateAll();
    markSaved();

    endResetModel();
}

SpeakingEvalRows SpeakingEvalModel::rows() const
{
    return m_rows;
}

QList<SpeakingEvalCellChange> SpeakingEvalModel::changedCells() const
{
    QList<SpeakingEvalCellChange> changes;

    if (m_lastSaved.isEmpty())
    {
        for (int row = 0; row < m_rows.size(); ++row)
        {
            for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
            {
                changes.append({ row, column });
            }
        }

        return changes;
    }

    for (int row = 0; row < m_rows.size(); ++row)
    {
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            const QString oldValue =
                row < m_lastSaved.size()
                && column < m_lastSaved[row].size()
                    ? m_lastSaved[row][column]
                    : QString();

            if (oldValue != m_rows[row][column])
            {
                changes.append({ row, column });
            }
        }
    }

    return changes;
}

QSet<QString> SpeakingEvalModel::dirtyCellKeys() const
{
    return m_dirtyCells;
}

QStringList SpeakingEvalModel::errorsForCell(
    int row,
    int column
    ) const
{
    return m_errors.value(
        cellKey(row, column)
        );
}

QList<int> SpeakingEvalModel::duplicateNameRows(
    int row
    ) const
{
    return StudentNameUtils::duplicateNameRows(
        m_rows,
        row,
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName),
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
        );
}

QString SpeakingEvalModel::suggestedKoreanNameWithSuffix(
    int row
    ) const
{
    return StudentNameUtils::suggestedKoreanNameWithSuffix(
        m_rows,
        row,
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName),
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
        );
}

bool SpeakingEvalModel::containsNamePair(
    const QString& englishName,
    const QString& koreanName
    ) const
{
    const QString key =
        StudentNameUtils::namePairKey(
            StudentNameUtils::normalizeEnglishName(englishName),
            StudentNameUtils::normalizeKoreanName(koreanName)
            );

    if (key.isEmpty())
    {
        return false;
    }

    for (const QStringList& row : m_rows)
    {
        if (
            StudentNameUtils::namePairKey(
                row.value(SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)),
                row.value(SpeakingEval::toInt(SpeakingEvalColumn::KoreanName))
                ) == key
            )
        {
            return true;
        }
    }

    return false;
}

bool SpeakingEvalModel::hasErrors() const
{
    return !m_errors.isEmpty();
}

QStringList SpeakingEvalModel::errorList() const
{
    QStringList errors;

    for (auto it = m_errors.constBegin(); it != m_errors.constEnd(); ++it)
    {
        const QStringList parts =
            it.key().split(QLatin1Char(':'));

        const int row =
            parts.value(0).toInt() + 1;

        const int column =
            parts.value(1).toInt() + 1;

        for (const QString& error : it.value())
        {
            errors.append(
                tr("Row %1, Col %2: %3")
                    .arg(row)
                    .arg(column)
                    .arg(error)
                );
        }
    }

    return errors;
}

bool SpeakingEvalModel::isDirty() const
{
    return m_dirty;
}

void SpeakingEvalModel::markSaved()
{
    m_lastSaved =
        m_rows;

    m_dirtyCells.clear();
    setDirtyState(false);
}

void SpeakingEvalModel::revalidateAll()
{
    m_errors.clear();

    for (int row = 0; row < m_rows.size(); ++row)
    {
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            const ProcessedValue processed =
                processValue(
                    row,
                    column,
                    m_rows[row][column]
                    );

            m_rows[row][column] =
                processed.normalized;

            if (!processed.errors.isEmpty())
            {
                m_errors.insert(
                    cellKey(row, column),
                    processed.errors
                    );
            }
        }
    }

    validateDuplicateNames();
}

SpeakingEvalRows SpeakingEvalModel::normalizeStructure(
    const SpeakingEvalRows& rows
    ) const
{
    SpeakingEvalRows normalized;

    for (int row = 0; row < SpeakingEval::RowCount; ++row)
    {
        QStringList outputRow;

        const QStringList sourceRow =
            row < rows.size()
                ? rows[row]
                : QStringList();

        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            const QString value =
                column < sourceRow.size()
                    ? sourceRow[column]
                    : QString();

            outputRow.append(
                processValue(
                    row,
                    column,
                    value
                    ).normalized
                );
        }

        normalized.append(outputRow);
    }

    return normalized;
}

SpeakingEvalModel::ProcessedValue SpeakingEvalModel::processValue(
    int row,
    int column,
    const QString& value
    ) const
{
    const auto columnId =
        SpeakingEval::columnFromInt(column);

    QString normalized =
        value;

    switch (columnId)
    {
    case SpeakingEvalColumn::EnglishName:
        normalized =
            StudentNameUtils::normalizeEnglishName(value);
        break;
    case SpeakingEvalColumn::KoreanName:
        normalized =
            StudentNameUtils::normalizeKoreanName(value);
        break;
    case SpeakingEvalColumn::Grammar:
    case SpeakingEvalColumn::Pronunciation:
    case SpeakingEvalColumn::Fluency:
    case SpeakingEvalColumn::Manner:
    case SpeakingEvalColumn::Content:
    case SpeakingEvalColumn::OverallEffort:
        normalized =
            normalizeScore(value);
        break;
    case SpeakingEvalColumn::Comments:
        normalized =
            normalizeComment(value);
        break;
    case SpeakingEvalColumn::Notes:
        normalized = value;
        break;
    default:
        normalized =
            value.trimmed();
        break;
    }

    return {
        normalized,
        validateValue(
            row,
            column,
            normalized
            )
    };
}


QString SpeakingEvalModel::normalizeScore(
    const QString& value
    ) const
{
    if (value.trimmed().isEmpty())
    {
        return {};
    }

    QString normalized =
        value.trimmed().toUpper();

    normalized.remove(
        QRegularExpression(QStringLiteral("\\s+"))
        );

    const QString koreanC(QChar(0x314A));
    const QString koreanB(QChar(0x3160));
    const QString koreanA(QChar(0x3141));

    if (normalized == QStringLiteral("1") || normalized == koreanC)
    {
        return QStringLiteral("C");
    }

    if (normalized == QStringLiteral("2") || normalized == koreanB)
    {
        return QStringLiteral("B");
    }

    if (normalized == QStringLiteral("3") || normalized == koreanB + QLatin1Char('+'))
    {
        return QStringLiteral("B+");
    }

    if (normalized == QStringLiteral("4") || normalized == koreanA)
    {
        return QStringLiteral("A");
    }

    if (normalized == QStringLiteral("5") || normalized == koreanA + QLatin1Char('+'))
    {
        return QStringLiteral("A+");
    }

    return normalized;
}

QString SpeakingEvalModel::normalizeComment(
    const QString& value
    ) const
{
    return value;
}

QStringList SpeakingEvalModel::validateValue(
    int row,
    int column,
    const QString& value
    ) const
{
    Q_UNUSED(row);

    QStringList errors;

    if (value.trimmed().isEmpty())
    {
        return errors;
    }

    const auto columnId =
        SpeakingEval::columnFromInt(column);

    if (columnId == SpeakingEvalColumn::EnglishName)
    {
        const auto issues = StudentNameUtils::validateEnglishName(value);
        if (issues.contains(StudentNameUtils::ValidationIssue::EnglishTooLong))
        {
            errors.append(
                tr("English name must be 20 characters or fewer.")
                );
        }

        if (issues.contains(
                StudentNameUtils::ValidationIssue::EnglishContainsNonAscii
                ))
        {
            errors.append(
                tr("Only standard English letters are allowed.")
                );
        }
    }
    else if (columnId == SpeakingEvalColumn::KoreanName)
    {
        const auto issues = StudentNameUtils::validateKoreanName(value);
        if (issues.contains(StudentNameUtils::ValidationIssue::KoreanTooShort)
            || issues.contains(StudentNameUtils::ValidationIssue::KoreanTooLong))
        {
            errors.append(
                tr("Invalid Korean name length.")
                );
        }
        else if (issues.contains(
                     StudentNameUtils::ValidationIssue::KoreanUnusualLength
                     ))
        {
            errors.append(
                tr("Uncommon Korean name length. Please verify.")
                );
        }
    }
    else if (SpeakingEval::isScoringColumn(columnId))
    {
        if (!SpeakingEval::scoreValues().contains(value))
        {
            errors.append(
                tr("Invalid score '%1'.")
                    .arg(value)
                );
        }
    }
    else if (columnId == SpeakingEvalColumn::Comments)
    {
        if (value.size() > SpeakingEval::CommentMaxLength)
        {
            errors.append(
                tr("Comment must be <= %1 characters.")
                    .arg(SpeakingEval::CommentMaxLength)
                );
        }
    }

    return errors;
}

QString SpeakingEvalModel::cellKey(
    int row,
    int column
    ) const
{
    return QStringLiteral("%1:%2")
        .arg(row)
        .arg(column);
}

void SpeakingEvalModel::validateDuplicateNames()
{
    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);

    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    const QHash<QString, QList<int>> rowsByPair =
        StudentNameUtils::duplicateRowsByNamePair(
            m_rows,
            englishColumn,
            koreanColumn
            );

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
                message
                );

            appendValidationError(
                row,
                koreanColumn,
                message
                );
        }
    }
}

void SpeakingEvalModel::appendValidationError(
    int row,
    int column,
    const QString& error
    )
{
    const QString key =
        cellKey(
            row,
            column
            );

    QStringList errors =
        m_errors.value(key);

    if (!errors.contains(error))
    {
        errors.append(error);
    }

    m_errors.insert(
        key,
        errors
        );
}

void SpeakingEvalModel::setDirtyState(
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
