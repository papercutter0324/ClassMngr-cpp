#include "speaking_eval_model.h"

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
        return SpeakingEval::header(
            SpeakingEval::columnFromInt(section)
            );
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
    QList<int> rows;

    if (row < 0 || row >= m_rows.size())
    {
        return rows;
    }

    const QString englishName =
        m_rows[row][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)].trimmed();

    const QString koreanName =
        m_rows[row][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)].trimmed();

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
                m_rows[candidateRow][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)],
                m_rows[candidateRow][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)]
                ) == key
            )
        {
            rows.append(candidateRow);
        }
    }

    return rows;
}

QString SpeakingEvalModel::suggestedKoreanNameWithSuffix(
    int row
    ) const
{
    if (row < 0 || row >= m_rows.size())
    {
        return {};
    }

    const QString englishName =
        m_rows[row][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)].trimmed();

    const QString baseName =
        baseKoreanName(
            m_rows[row][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)]
            );

    if (englishName.isEmpty() || baseName.isEmpty())
    {
        return {};
    }

    QSet<QChar> usedSuffixes;

    for (const QStringList& rowValues : m_rows)
    {
        if (
            englishName.compare(
                rowValues.value(
                    SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
                    ).trimmed(),
                Qt::CaseSensitive
                ) != 0
            || baseKoreanName(
                rowValues.value(
                    SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
                    )
                ) != baseName
            )
        {
            continue;
        }

        const QString suffix =
            koreanNameSuffix(
                rowValues.value(
                    SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
                    )
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

bool SpeakingEvalModel::containsNamePair(
    const QString& englishName,
    const QString& koreanName
    ) const
{
    const QString key =
        namePairKey(
            normalizeEnglishName(englishName),
            normalizeKoreanName(koreanName)
            );

    if (key.isEmpty())
    {
        return false;
    }

    for (const QStringList& row : m_rows)
    {
        if (
            namePairKey(
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
            normalizeEnglishName(value);
        break;
    case SpeakingEvalColumn::KoreanName:
        normalized =
            normalizeKoreanName(value);
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

QString SpeakingEvalModel::normalizeEnglishName(
    const QString& value
    ) const
{
    if (value.trimmed().isEmpty())
    {
        return {};
    }

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
        }
        else if (character.isSpace())
        {
            filtered.append(QLatin1Char(' '));
        }
    }

    QString cleaned =
        filtered.simplified();

    cleaned.replace(
        QRegularExpression(QStringLiteral("\\s*-\\s*")),
        QStringLiteral("-")
        );

    cleaned.replace(
        QRegularExpression(QStringLiteral("-{2,}")),
        QStringLiteral("-")
        );

    cleaned.replace(
        QRegularExpression(QStringLiteral("\\.{2,}")),
        QStringLiteral(".")
        );

    cleaned.replace(
        QRegularExpression(QStringLiteral("\\s*\\.\\s*")),
        QStringLiteral(".")
        );

    cleaned.replace(
        QRegularExpression(QStringLiteral("\\b([A-Za-z])[.-]+-?[.-]*([A-Za-z])\\b")),
        QStringLiteral("\\1.\\2")
        );

    QString result;
    QString token;
    QChar previousSeparator;

    const auto flushToken =
        [&result, &token, &previousSeparator]()
        {
            if (token.isEmpty())
            {
                return;
            }

            const QString lower =
                token.toLower();

            if (
                result.isEmpty()
                || previousSeparator == QLatin1Char(' ')
                || previousSeparator == QLatin1Char('.')
                )
            {
                result +=
                    lower.left(1).toUpper()
                    + lower.mid(1);
            }
            else
            {
                result += lower;
            }

            token.clear();
        };

    for (const QChar& character : cleaned)
    {
        if (
            character == QLatin1Char(' ')
            || character == QLatin1Char('.')
            || character == QLatin1Char('-')
            )
        {
            flushToken();
            result.append(character);
            previousSeparator = character;
            continue;
        }

        token.append(character);
    }

    flushToken();

    result.replace(
        QRegularExpression(QStringLiteral("\\b([A-Za-z])\\. ?([A-Za-z])\\.")),
        QStringLiteral("\\1.\\2.")
        );

    return result.trimmed();
}

QString SpeakingEvalModel::normalizeKoreanName(
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

    return normalized.trimmed();
}

QString SpeakingEvalModel::baseKoreanName(
    const QString& value
    ) const
{
    QString normalized =
        normalizeKoreanName(value);

    normalized.remove(
        QRegularExpression(QStringLiteral("\\([A-Z]\\)$"))
        );

    return normalized;
}

QString SpeakingEvalModel::koreanNameSuffix(
    const QString& value
    ) const
{
    const QRegularExpression suffixExpression(
        QStringLiteral("\\(([A-Z])\\)$")
        );

    const QRegularExpressionMatch match =
        suffixExpression.match(
            normalizeKoreanName(value)
            );

    return match.hasMatch()
        ? match.captured(1)
        : QString();
}

QString SpeakingEvalModel::namePairKey(
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
    return value.trimmed();
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
        if (value.size() > 20)
        {
            errors.append(
                tr("English name must be 20 characters or fewer.")
                );
        }

        for (const QChar& character : value)
        {
            if (character.unicode() > 127)
            {
                errors.append(
                    tr("Only standard English letters are allowed.")
                    );

                break;
            }
        }
    }
    else if (columnId == SpeakingEvalColumn::KoreanName)
    {
        const int length =
            baseKoreanName(value).size();

        if (length == 3)
        {
            return errors;
        }

        if (length <= 1 || length >= 6)
        {
            errors.append(
                tr("Invalid Korean name length.")
                );
        }
        else
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
        else if (value.size() < SpeakingEval::CommentMinLength)
        {
            errors.append(
                tr("Comment must be >= %1 characters.")
                    .arg(SpeakingEval::CommentMinLength)
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
