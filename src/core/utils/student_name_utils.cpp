#include "student_name_utils.h"

#include "classmngr/engine/student_name.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QSet>

#include <string_view>

namespace StudentNameUtils
{
namespace
{
std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

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
    return fromUtf8(
        classmngr::engine::StudentNameService::normalizeEnglish(
            toUtf8(value)
            )
        );
}


QString normalizeKoreanName(
    const QString& value
    )
{
    return fromUtf8(
        classmngr::engine::StudentNameService::normalizeKorean(
            toUtf8(value)
            )
        );
}

QString baseKoreanName(
    const QString& value
    )
{
    return fromUtf8(
        classmngr::engine::StudentNameService::baseKorean(
            toUtf8(value)
            )
        );
}

QString koreanNameSuffix(
    const QString& value
    )
{
    return fromUtf8(
        classmngr::engine::StudentNameService::koreanSuffix(
            toUtf8(value)
            )
        );
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
