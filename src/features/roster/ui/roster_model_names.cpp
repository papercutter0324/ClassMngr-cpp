#include "roster_model.h"

#include "core/utils/student_name_utils.h"

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
    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    return StudentNameUtils::duplicateNameRows(
        m_rows,
        row,
        englishColumn,
        koreanColumn
        );
}

QString RosterModel::suggestedKoreanNameWithSuffix(
    int row
    ) const
{
    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    return StudentNameUtils::suggestedKoreanNameWithSuffix(
        m_rows,
        row,
        englishColumn,
        koreanColumn
        );
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
        return StudentNameUtils::normalizeKoreanName(value);
    }

    return value.simplified();
}

QString RosterModel::normalizeEnglish(
    const QString& value
    ) const
{
    static const QRegularExpression spacedHyphenExpression(
        QStringLiteral("\\s*-\\s*")
        );
    static const QRegularExpression spacedPeriodExpression(
        QStringLiteral("\\s*\\.\\s*")
        );
    static const QRegularExpression repeatedHyphenExpression(
        QStringLiteral("-+")
        );
    static const QRegularExpression repeatedPeriodExpression(
        QStringLiteral("\\.+")
        );

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
        spacedHyphenExpression,
        QStringLiteral("-")
        );

    normalized.replace(
        spacedPeriodExpression,
        QStringLiteral(".")
        );

    normalized.replace(
        repeatedHyphenExpression,
        QStringLiteral("-")
        );

    normalized.replace(
        repeatedPeriodExpression,
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
