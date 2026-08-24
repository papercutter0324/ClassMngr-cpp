#include "student_name_utils.h"

#include <QRegularExpression>
#include <QSet>

namespace StudentNameUtils
{
namespace
{
bool containsNonAsciiCharacters(const QString& value)
{
    for (const QChar character : value)
    {
        if (character.unicode() > 127)
        {
            return true;
        }
    }

    return false;
}

bool containsInvalidEnglishCharacters(const QString& value)
{
    for (const QChar character : value)
    {
        const ushort code = character.unicode();
        if ((code >= 'A' && code <= 'Z')
            || (code >= 'a' && code <= 'z')
            || character == QLatin1Char('.')
            || character == QLatin1Char('-')
            || character.isSpace()
            || code > 127)
        {
            continue;
        }

        return true;
    }

    return false;
}

bool containsInvalidKoreanCharacters(const QString& value)
{
    static const QRegularExpression validNameExpression(
        QStringLiteral("^[\\s\\x{AC00}-\\x{D7A3}]+(?:\\s*\\([A-Za-z]\\))?\\s*$")
        );

    return !value.trimmed().isEmpty()
        && !validNameExpression.match(value).hasMatch();
}
}

QString normalizeEnglishName(const QString& value)
{
    static const QRegularExpression spacedHyphenExpression(
        QStringLiteral("\\s*-\\s*")
        );
    static const QRegularExpression repeatedHyphenExpression(
        QStringLiteral("-{2,}")
        );
    static const QRegularExpression repeatedPeriodExpression(
        QStringLiteral("\\.{2,}")
        );
    static const QRegularExpression spacedPeriodExpression(
        QStringLiteral("\\s*\\.\\s*")
        );
    static const QRegularExpression joinedInitialsExpression(
        QStringLiteral("\\b([A-Za-z])[.-]+-?[.-]*([A-Za-z])\\b")
        );
    static const QRegularExpression adjacentInitialsExpression(
        QStringLiteral("\\b([A-Za-z])\\. ?([A-Za-z])\\.")
        );

    if (value.trimmed().isEmpty())
    {
        return {};
    }

    // Preserve malformed input so callers that normalize before validating do
    // not accidentally turn it into a different, valid name.
    if (containsNonAsciiCharacters(value)
        || containsInvalidEnglishCharacters(value))
    {
        return value.trimmed();
    }

    QString filtered;
    filtered.reserve(value.size());
    for (const QChar character : value)
    {
        const ushort code = character.unicode();
        if ((code >= 'A' && code <= 'Z')
            || (code >= 'a' && code <= 'z')
            || code == '.' || code == '-')
        {
            filtered.append(character);
        }
        else if (character.isSpace())
        {
            filtered.append(QLatin1Char(' '));
        }
    }

    QString cleaned = filtered.simplified();
    cleaned.replace(spacedHyphenExpression, QStringLiteral("-"));
    cleaned.replace(repeatedHyphenExpression, QStringLiteral("-"));
    cleaned.replace(repeatedPeriodExpression, QStringLiteral("."));
    cleaned.replace(spacedPeriodExpression, QStringLiteral("."));
    cleaned.replace(joinedInitialsExpression, QStringLiteral("\\1.\\2"));

    QString result;
    QString token;
    QChar previousSeparator;
    const auto flushToken = [&result, &token, &previousSeparator]()
    {
        if (token.isEmpty())
        {
            return;
        }
        const QString lower = token.toLower();
        result += result.isEmpty()
                || previousSeparator == QLatin1Char(' ')
                || previousSeparator == QLatin1Char('.')
            ? lower.left(1).toUpper() + lower.mid(1)
            : lower;
        token.clear();
    };

    for (const QChar character : cleaned)
    {
        if (character == QLatin1Char(' ')
            || character == QLatin1Char('.')
            || character == QLatin1Char('-'))
        {
            flushToken();
            result.append(character);
            previousSeparator = character;
        }
        else
        {
            token.append(character);
        }
    }
    flushToken();
    result.replace(adjacentInitialsExpression, QStringLiteral("\\1.\\2."));
    return result.trimmed();
}


QString normalizeKoreanName(
    const QString& value
    )
{
    static const QRegularExpression suffixExpression(
        QStringLiteral("\\(([A-Za-z])\\)\\s*$")
        );

    // As with English names, invalid input must remain visible to validation.
    if (containsInvalidKoreanCharacters(value))
    {
        return value.trimmed();
    }

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

QString baseKoreanName(
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

QString koreanNameSuffix(
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

QString namePairKey(
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

QHash<QString, QList<int>> rowsByNamePair(
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

QList<ValidationIssue> validateEnglishName(
    const QString& value,
    qsizetype maximumLength
    )
{
    QList<ValidationIssue> issues;
    if (value.size() > maximumLength)
    {
        issues.append(ValidationIssue::EnglishTooLong);
    }
    if (containsNonAsciiCharacters(value))
    {
        issues.append(ValidationIssue::EnglishContainsNonAscii);
    }
    if (containsInvalidEnglishCharacters(value))
    {
        issues.append(ValidationIssue::EnglishContainsInvalidCharacters);
    }
    return issues;
}

QList<ValidationIssue> validateKoreanName(const QString& value)
{
    QList<ValidationIssue> issues;
    if (containsInvalidKoreanCharacters(value))
    {
        issues.append(ValidationIssue::KoreanContainsInvalidCharacters);
    }

    const int length = baseKoreanName(value).size();
    if (length == 0 || length == 3)
    {
        return issues;
    }
    if (length <= 1)
    {
        issues.append(ValidationIssue::KoreanTooShort);
        return issues;
    }
    if (length >= 5)
    {
        issues.append(ValidationIssue::KoreanTooLong);
        return issues;
    }
    issues.append(ValidationIssue::KoreanUnusualLength);
    return issues;
}

QHash<QString, QList<int>> duplicateRowsByNamePair(
    const QList<QStringList>& rows,
    int englishColumn,
    int koreanColumn
    )
{
    QHash<QString, QList<int>> duplicates = rowsByNamePair(
        rows,
        englishColumn,
        koreanColumn
        );
    for (auto it = duplicates.begin(); it != duplicates.end();)
    {
        if (it.value().size() < 2)
        {
            it = duplicates.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return duplicates;
}

QList<int> duplicateNameRows(
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

QString suggestedKoreanNameWithSuffix(
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
