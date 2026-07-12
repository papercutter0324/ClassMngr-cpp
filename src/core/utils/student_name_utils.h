#pragma once

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace StudentNameUtils
{

inline QString normalizeKoreanName(
    const QString& value
    )
{
    static const QRegularExpression suffixExpression(
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

    for (const QChar character : source)
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

inline QString baseKoreanName(
    const QString& value
    )
{
    static const QRegularExpression suffixExpression(
        QStringLiteral("\\([A-Z]\\)$")
        );

    QString normalized =
        normalizeKoreanName(value);

    normalized.remove(suffixExpression);

    return normalized;
}

inline QString koreanNameSuffix(
    const QString& value
    )
{
    static const QRegularExpression suffixExpression(
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

inline QString namePairKey(
    const QString& englishName,
    const QString& koreanName
    )
{
    const QString normalizedEnglishName =
        englishName.trimmed();
    const QString normalizedKoreanName =
        koreanName.trimmed();

    if (
        normalizedEnglishName.isEmpty()
        || normalizedKoreanName.isEmpty()
        )
    {
        return {};
    }

    return normalizedEnglishName
        + QChar(0x001F)
        + normalizedKoreanName;
}

inline QHash<QString, QList<int>> rowsByNamePair(
    const QList<QStringList>& rows,
    int englishColumn,
    int koreanColumn
    )
{
    QHash<QString, QList<int>> rowsByPair;
    rowsByPair.reserve(rows.size());

    for (int row = 0; row < rows.size(); ++row)
    {
        const QString key =
            namePairKey(
                rows[row].value(englishColumn),
                rows[row].value(koreanColumn)
                );

        if (!key.isEmpty())
        {
            rowsByPair[key].append(row);
        }
    }

    return rowsByPair;
}

inline QList<int> duplicateNameRows(
    const QList<QStringList>& rows,
    int row,
    int englishColumn,
    int koreanColumn
    )
{
    QList<int> duplicates;

    if (
        row < 0
        || row >= rows.size()
        || englishColumn < 0
        || koreanColumn < 0
        )
    {
        return duplicates;
    }

    const QString key =
        namePairKey(
            rows[row].value(englishColumn),
            rows[row].value(koreanColumn)
            );

    if (key.isEmpty())
    {
        return duplicates;
    }

    for (int candidateRow = 0; candidateRow < rows.size(); ++candidateRow)
    {
        if (
            candidateRow != row
            && namePairKey(
                rows[candidateRow].value(englishColumn),
                rows[candidateRow].value(koreanColumn)
                ) == key
            )
        {
            duplicates.append(candidateRow);
        }
    }

    return duplicates;
}

inline QString suggestedKoreanNameWithSuffix(
    const QList<QStringList>& rows,
    int row,
    int englishColumn,
    int koreanColumn
    )
{
    if (
        row < 0
        || row >= rows.size()
        || englishColumn < 0
        || koreanColumn < 0
        )
    {
        return {};
    }

    const QString englishName =
        rows[row].value(englishColumn).trimmed();
    const QString baseName =
        baseKoreanName(
            rows[row].value(koreanColumn)
            );

    if (englishName.isEmpty() || baseName.isEmpty())
    {
        return {};
    }

    QSet<QChar> usedSuffixes;

    for (const QStringList& candidateRow : rows)
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

} // namespace StudentNameUtils
