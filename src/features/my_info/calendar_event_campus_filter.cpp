#include "calendar_event_campus_filter.h"

#include <QRegularExpression>

namespace
{
QStringList normalizedCodes(
    const QStringList& codes
    )
{
    QStringList values;

    for (const QString& code : codes)
    {
        const QString normalized =
            code.trimmed().toUpper();

        if (!normalized.isEmpty())
        {
            values.append(normalized);
        }
    }

    values.removeDuplicates();
    values.sort();

    return values;
}

bool containsCode(
    const QString& title,
    const QString& code
    )
{
    if (code.isEmpty())
    {
        return false;
    }

    const QRegularExpression pattern(
        QStringLiteral("(^|[^A-Z0-9])%1([^A-Z0-9]|$)")
            .arg(QRegularExpression::escape(code)),
        QRegularExpression::CaseInsensitiveOption
        );

    return pattern.match(title).hasMatch();
}
}

namespace CalendarEventCampusFilter
{
bool eventMatchesCampus(
    const CalendarEvent& event,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    )
{
    if (showAllCampuses)
    {
        return true;
    }

    const QString title =
        event.title.trimmed();
    if (title.isEmpty())
    {
        return true;
    }

    const QStringList allCodes =
        normalizedCodes(allCampusCodes);
    const QStringList currentCodes =
        normalizedCodes(currentCampusCodes);

    if (allCodes.isEmpty() || currentCodes.isEmpty())
    {
        return true;
    }

    bool containsAnyKnownCampus = false;

    for (const QString& code : allCodes)
    {
        if (!containsCode(title, code))
        {
            continue;
        }

        containsAnyKnownCampus = true;
        if (currentCodes.contains(code))
        {
            return true;
        }
    }

    return !containsAnyKnownCampus;
}
}
