#include "student_name_utils.h"

#include "classmngr/engine/student_name.h"

#include <QByteArray>
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

ValidationIssue mapIssue(classmngr::engine::StudentNameIssue issue)
{
    using EngineIssue = classmngr::engine::StudentNameIssue;
    switch (issue)
    {
    case EngineIssue::EnglishTooLong:
        return ValidationIssue::EnglishTooLong;
    case EngineIssue::EnglishContainsNonAscii:
        return ValidationIssue::EnglishContainsNonAscii;
    case EngineIssue::EnglishContainsInvalidCharacters:
        return ValidationIssue::EnglishContainsInvalidCharacters;
    case EngineIssue::KoreanTooShort:
        return ValidationIssue::KoreanTooShort;
    case EngineIssue::KoreanUnusualLength:
        return ValidationIssue::KoreanUnusualLength;
    case EngineIssue::KoreanTooLong:
        return ValidationIssue::KoreanTooLong;
    case EngineIssue::KoreanContainsInvalidCharacters:
        return ValidationIssue::KoreanContainsInvalidCharacters;
    }
    return ValidationIssue::EnglishContainsInvalidCharacters;
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
    return fromUtf8(
        classmngr::engine::StudentNameService::namePairKey(
            toUtf8(englishName),
            toUtf8(koreanName)
            )
        );
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
    for (const auto issue : classmngr::engine::StudentNameService::validateEnglish(
             toUtf8(value),
             static_cast<std::size_t>(maximumLength)
             ))
    {
        issues.append(mapIssue(issue));
    }
    return issues;
}

QList<ValidationIssue> validateKoreanName(const QString& value)
{
    QList<ValidationIssue> issues;
    for (const auto issue : classmngr::engine::StudentNameService::validateKorean(
             toUtf8(value)
             ))
    {
        issues.append(mapIssue(issue));
    }
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
