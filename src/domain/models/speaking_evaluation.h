#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <utility>

enum class SpeakingEvalColumn
{
    Index = 0,
    EnglishName = 1,
    KoreanName = 2,
    Grammar = 3,
    Pronunciation = 4,
    Fluency = 5,
    Manner = 6,
    Content = 7,
    OverallEffort = 8,
    Comments = 9,
    Notes = 10
};

struct SpeakingEvalCellChange
{
    int row = -1;
    int column = -1;
};

struct SpeakingEvalScore
{
    QString englishName;
    QString koreanName;
    QString finalGrade;
};

using SpeakingEvalRows = QList<QStringList>;

namespace SpeakingEval
{

inline constexpr int RowCount = 25;
inline constexpr int ColumnCount = 11;
inline constexpr int RowHeight = 50;
inline constexpr int CommentMinLength = 80;
inline constexpr int CommentMaxLength = 900;

inline int toInt(
    SpeakingEvalColumn column
    )
{
    return std::to_underlying(column);
}

inline SpeakingEvalColumn columnFromInt(
    int column
    )
{
    return static_cast<SpeakingEvalColumn>(column);
}

inline QStringList scoreValues()
{
    return {
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };
}

inline bool isScoringColumn(
    SpeakingEvalColumn column
    )
{
    constexpr std::array scoringColumns{
        SpeakingEvalColumn::Grammar,
        SpeakingEvalColumn::Pronunciation,
        SpeakingEvalColumn::Fluency,
        SpeakingEvalColumn::Manner,
        SpeakingEvalColumn::Content,
        SpeakingEvalColumn::OverallEffort
    };

    return std::ranges::any_of(
        scoringColumns,
        [column](SpeakingEvalColumn scoringColumn)
        {
            return scoringColumn == column;
        }
        );
}

inline bool isEditableColumn(
    SpeakingEvalColumn column
    )
{
    return column != SpeakingEvalColumn::Index;
}

inline QString header(
    SpeakingEvalColumn column
    )
{
    switch (column)
    {
    case SpeakingEvalColumn::Index:
        return {};
    case SpeakingEvalColumn::EnglishName:
        return QStringLiteral("English Name");
    case SpeakingEvalColumn::KoreanName:
        return QStringLiteral("Korean Name");
    case SpeakingEvalColumn::Grammar:
        return QStringLiteral("Grammar");
    case SpeakingEvalColumn::Pronunciation:
        return QStringLiteral("Pronunciation");
    case SpeakingEvalColumn::Fluency:
        return QStringLiteral("Fluency");
    case SpeakingEvalColumn::Manner:
        return QStringLiteral("Manner");
    case SpeakingEvalColumn::Content:
        return QStringLiteral("Content");
    case SpeakingEvalColumn::OverallEffort:
        return QStringLiteral("Overall Effort");
    case SpeakingEvalColumn::Comments:
        return QStringLiteral("Comments");
    case SpeakingEvalColumn::Notes:
        return QStringLiteral("Notes");
    }

    return {};
}

inline QColor columnColor(
    SpeakingEvalColumn column
    )
{
    switch (column)
    {
    case SpeakingEvalColumn::Index:
        return QColor(QStringLiteral("#d9d9d9"));
    case SpeakingEvalColumn::EnglishName:
    case SpeakingEvalColumn::KoreanName:
        return QColor(QStringLiteral("#ffffff"));
    case SpeakingEvalColumn::Grammar:
        return QColor(QStringLiteral("#d9d2e9"));
    case SpeakingEvalColumn::Pronunciation:
        return QColor(QStringLiteral("#cfe2f3"));
    case SpeakingEvalColumn::Fluency:
        return QColor(QStringLiteral("#f4cccc"));
    case SpeakingEvalColumn::Manner:
        return QColor(QStringLiteral("#fce5cd"));
    case SpeakingEvalColumn::Content:
        return QColor(QStringLiteral("#d9ead3"));
    case SpeakingEvalColumn::OverallEffort:
        return QColor(QStringLiteral("#cfe2f3"));
    case SpeakingEvalColumn::Comments:
        return QColor(QStringLiteral("#eeeeee"));
    case SpeakingEvalColumn::Notes:
        return QColor(QStringLiteral("#e6e0c9"));
    }

    return QColor(Qt::white);
}

inline int columnWidth(
    SpeakingEvalColumn column
    )
{
    switch (column)
    {
    case SpeakingEvalColumn::Index:
        return 40;
    case SpeakingEvalColumn::EnglishName:
    case SpeakingEvalColumn::KoreanName:
        return 180;
    case SpeakingEvalColumn::Grammar:
    case SpeakingEvalColumn::Pronunciation:
    case SpeakingEvalColumn::Fluency:
    case SpeakingEvalColumn::Manner:
    case SpeakingEvalColumn::Content:
    case SpeakingEvalColumn::OverallEffort:
        return 150;
    case SpeakingEvalColumn::Comments:
        return 500;
    case SpeakingEvalColumn::Notes:
        return 300;
    }

    return 100;
}

inline bool hasThickBorderAfter(
    SpeakingEvalColumn column
    )
{
    return column == SpeakingEvalColumn::Index
        || column == SpeakingEvalColumn::KoreanName
        || column == SpeakingEvalColumn::OverallEffort
        || column == SpeakingEvalColumn::Comments;
}

inline QColor contrastTextColor(
    const QColor& color
    )
{
    const int luminance =
        ((color.red() * 299)
         + (color.green() * 587)
         + (color.blue() * 114)) / 1000;

    return luminance >= 150
        ? QColor(31, 41, 55)
        : QColor(255, 255, 255);
}

inline SpeakingEvalRows emptyRows()
{
    SpeakingEvalRows rows;

    for (int row = 0; row < RowCount; ++row)
    {
        QStringList values;

        for (int column = 0; column < ColumnCount; ++column)
        {
            values.append(QString());
        }

        rows.append(values);
    }

    return rows;
}

} // namespace SpeakingEval
