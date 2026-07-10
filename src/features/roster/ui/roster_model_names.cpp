#include "roster_model.h"

#include <QRegularExpression>

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

