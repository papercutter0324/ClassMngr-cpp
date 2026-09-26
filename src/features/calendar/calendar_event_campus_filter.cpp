#include "calendar_event_campus_filter.h"

#include "next/application/calendar_event_campus_visibility_policy.h"

#include <string>
#include <vector>

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

QString simpleCaseFolded(
    const QString& value
    )
{
    // Use one-to-one Unicode folding to retain QRegularExpression's
    // case-insensitive range behavior without multi-character expansions.
    QString folded;
    folded.reserve(value.size());

    for (qsizetype index = 0; index < value.size();)
    {
        char32_t codePoint = value.at(index).unicode();
        ++index;
        if (QChar::isHighSurrogate(codePoint) && index < value.size())
        {
            const char32_t lowSurrogate = value.at(index).unicode();
            if (QChar::isLowSurrogate(lowSurrogate))
            {
                codePoint = QChar::surrogateToUcs4(
                    value.at(index - 1),
                    value.at(index)
                    );
                ++index;
            }
        }

        const char32_t caseFoldedCodePoint =
            QChar::toCaseFolded(codePoint);
        if (QChar::requiresSurrogates(caseFoldedCodePoint))
        {
            folded.append(QChar(QChar::highSurrogate(caseFoldedCodePoint)));
            folded.append(QChar(QChar::lowSurrogate(caseFoldedCodePoint)));
        }
        else
        {
            folded.append(QChar(
                static_cast<char16_t>(caseFoldedCodePoint)
                ));
        }
    }

    return folded;
}

std::vector<ClassMngr::Next::Application::CalendarEventCampusCode>
utf8Codes(
    const QStringList& codes
    )
{
    using CampusCode =
        ClassMngr::Next::Application::CalendarEventCampusCode;
    std::vector<CampusCode> values;
    values.reserve(static_cast<std::size_t>(codes.size()));

    for (const QString& code : codes)
    {
        values.push_back({
            code.toUtf8().toStdString(),
            simpleCaseFolded(code).toUtf8().toStdString()
            });
    }

    return values;
}
}

namespace CalendarEventCampusFilter
{
bool eventMatchesCampus(
    const QString& title,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    )
{
    const QStringList allCodes =
        normalizedCodes(allCampusCodes);
    const QStringList currentCodes =
        normalizedCodes(currentCampusCodes);

    const std::string normalizedTitle =
        simpleCaseFolded(title.trimmed()).toUtf8().toStdString();
    return ClassMngr::Next::Application::
        CalendarEventCampusVisibilityPolicy::eventMatchesCampus(
            normalizedTitle,
            utf8Codes(currentCodes),
            utf8Codes(allCodes),
            showAllCampuses
            );
}

bool eventMatchesCampus(
    const ClassMngr::Next::Application::CalendarEventSummary& event,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    )
{
    const QString title = QString::fromUtf8(
        event.title.data(),
        static_cast<qsizetype>(event.title.size())
        );
    return eventMatchesCampus(
        title,
        currentCampusCodes,
        allCampusCodes,
        showAllCampuses
        );
}

bool eventMatchesCampus(
    const CalendarEvent& event,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    )
{
    return eventMatchesCampus(
        event.title,
        currentCampusCodes,
        allCampusCodes,
        showAllCampuses
        );
}
}
