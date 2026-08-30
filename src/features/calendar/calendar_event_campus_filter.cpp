#include "calendar_event_campus_filter.h"

#include <string>
#include <vector>

namespace
{
std::vector<std::string> toPortableCodes(
    const QStringList& codes
    )
{
    std::vector<std::string> values;
    values.reserve(static_cast<std::size_t>(codes.size()));

    for (const QString& code : codes)
    {
        const QByteArray utf8 = code.toUtf8();
        if (!utf8.trimmed().isEmpty())
        {
            values.emplace_back(utf8.constData(),
                                static_cast<std::size_t>(utf8.size()));
        }
    }

    return values;
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
    return classmngr::engine::CalendarEventRules::eventMatchesCampus(
        event.title.toUtf8().toStdString(),
        toPortableCodes(currentCampusCodes),
        toPortableCodes(allCampusCodes),
        showAllCampuses
        );
}
}
