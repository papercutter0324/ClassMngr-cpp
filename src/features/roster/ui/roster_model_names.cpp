#include "roster_model.h"

#include "core/utils/student_name_utils.h"
#include "ui/shared/qt_text_adapter.h"
#include "next/application/student_name_pair_lookup.h"
#include "next/application/student_korean_name_suffix_suggestion.h"

#include <optional>
#include <vector>

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

    if (englishColumn < 0 || koreanColumn < 0)
    {
        return {};
    }

    std::vector<ClassMngr::Next::Application::StudentNamePairText>
        namePairs;
    namePairs.reserve(static_cast<std::size_t>(m_rows.size()));
    for (const QStringList& values : m_rows)
    {
        namePairs.push_back({
            Ui::QtTextAdapter::toUtf16String(values.value(englishColumn)),
            Ui::QtTextAdapter::toUtf16String(values.value(koreanColumn))
        });
    }

    const std::vector<int> peerRows =
        ClassMngr::Next::Application::lookupStudentNamePairPeers(
            namePairs,
            row
            );
    QList<int> result;
    result.reserve(static_cast<qsizetype>(peerRows.size()));
    for (const int peerRow : peerRows)
    {
        result.append(peerRow);
    }
    return result;
}

QString RosterModel::suggestedKoreanNameWithSuffix(
    int row
    ) const
{
    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    std::vector<ClassMngr::Next::Application::StudentKoreanNameSuggestionRow>
        nameRows;
    nameRows.reserve(static_cast<std::size_t>(m_rows.size()));
    for (const QStringList& values : m_rows)
    {
        const QString koreanName =
            koreanColumn >= 0 && koreanColumn < values.size()
                ? values[koreanColumn]
                : QString();
        const QString koreanNameSuffix =
            StudentNameUtils::koreanNameSuffix(koreanName);

        nameRows.push_back({
            .englishName = englishColumn >= 0 && englishColumn < values.size()
                ? Ui::QtTextAdapter::toUtf16String(values[englishColumn])
                : std::u16string(),
            .koreanBaseName = Ui::QtTextAdapter::toUtf16String(
                StudentNameUtils::baseKoreanName(koreanName)
                ),
            .koreanNameSuffix = koreanNameSuffix.size() == 1
                ? std::optional<char16_t>(static_cast<char16_t>(
                    koreanNameSuffix.front().unicode()
                    ))
                : std::nullopt
        });
    }

    return Ui::QtTextAdapter::fromUtf16String(
        ClassMngr::Next::Application::suggestStudentKoreanNameSuffix(
            nameRows,
            row,
            englishColumn >= 0,
            koreanColumn >= 0
        )
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
    return StudentNameUtils::normalizeEnglishName(value);
}
