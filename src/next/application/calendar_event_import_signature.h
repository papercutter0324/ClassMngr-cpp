#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// The legacy duplicate key is built only from these six normalized values.
// Keep Qt normalization and date formatting at the feature adapters.
struct CalendarEventImportSignatureFields final
{
    std::u16string simplifiedTitle;
    std::u16string normalizedEventType;
    std::u16string startDateIso;
    std::u16string endDateIso;
    bool allDay = false;
    std::u16string normalizedTimeStatus;
};

// Opaque UTF-16 key matching the importer's established six-field identity.
// Field contents are appended verbatim, including placeholder-like text.
class CalendarEventImportSignature final
{
public:
    [[nodiscard]] static CalendarEventImportSignature fromNormalizedFields(
        const CalendarEventImportSignatureFields& fields
        )
    {
        std::u16string value;
        value.reserve(
            fields.simplifiedTitle.size()
            + fields.normalizedEventType.size()
            + fields.startDateIso.size()
            + fields.endDateIso.size()
            + fields.normalizedTimeStatus.size()
            + 6
            );

        value.append(fields.simplifiedTitle);
        value.push_back(u'|');
        value.append(fields.normalizedEventType);
        value.push_back(u'|');
        value.append(fields.startDateIso);
        value.push_back(u'|');
        value.append(fields.endDateIso);
        value.push_back(u'|');
        value.push_back(fields.allDay ? u'1' : u'0');
        value.push_back(u'|');
        value.append(fields.normalizedTimeStatus);

        return CalendarEventImportSignature(std::move(value));
    }

    [[nodiscard]] const std::u16string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const CalendarEventImportSignature&,
        const CalendarEventImportSignature&
        ) = default;

private:
    explicit CalendarEventImportSignature(std::u16string value)
        : m_value(std::move(value))
    {
    }

    std::u16string m_value;
};

} // namespace ClassMngr::Next::Application
