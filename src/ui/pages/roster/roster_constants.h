#pragma once

#include "models/roster.h"

#include <QColor>
#include <QString>

namespace RosterUi
{

inline constexpr int RowCount = 25;
inline constexpr int RowHeight = 50;
inline constexpr int HeaderGroupsHeight = 40;
inline constexpr int HeaderColumnHeight = 30;
inline constexpr int HeaderHeight = HeaderGroupsHeight + HeaderColumnHeight;
inline constexpr int CustomColumnDefaultWidth = 100;
inline constexpr int StudentInformationMinWidth = 220;

inline QString studentNamesGroup()
{
    return QStringLiteral("Student Names");
}

inline QString evaluationsGroup()
{
    return QStringLiteral("Evaluations");
}

inline QString studentInformationGroup()
{
    return QStringLiteral("Student Information");
}

inline bool isRequiredColumn(
    const QString& name
    )
{
    return Roster::BaseColumns.contains(
        name,
        Qt::CaseInsensitive
        );
}

inline bool isEvaluationColumn(
    const QString& name
    )
{
    return name.compare(QStringLiteral("Winter"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("Speech Contest"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("Summer"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0;
}

inline QString columnGroup(
    const QString& name
    )
{
    if (
        name.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0
        )
    {
        return studentNamesGroup();
    }

    if (isEvaluationColumn(name))
    {
        return evaluationsGroup();
    }

    return studentInformationGroup();
}

inline int defaultColumnWidth(
    const QString& name
    )
{
    if (name.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0)
    {
        return 170;
    }

    if (name.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0)
    {
        return 120;
    }

    if (isEvaluationColumn(name))
    {
        return 130;
    }

    return CustomColumnDefaultWidth;
}

inline QColor groupColor(
    const QString& group
    )
{
    if (group == studentNamesGroup())
    {
        return QColor(100, 160, 255);
    }

    if (group == evaluationsGroup())
    {
        return QColor(120, 200, 120);
    }

    return QColor(200, 200, 200);
}

inline QColor soften(
    const QColor& color,
    double amount
    )
{
    const auto channel =
        [amount](int value)
        {
            return static_cast<int>(
                value + ((255 - value) * amount)
                );
        };

    return QColor(
        channel(color.red()),
        channel(color.green()),
        channel(color.blue())
        );
}

inline QColor contrastTextColor(
    const QColor& color
    )
{
    const int luminance =
        ((color.red() * 299)
         + (color.green() * 587)
         + (color.blue() * 114)) / 1000;

    if (luminance >= 150)
    {
        return QColor(31, 41, 55);
    }

    return QColor(255, 255, 255);
}

} // namespace RosterUi
