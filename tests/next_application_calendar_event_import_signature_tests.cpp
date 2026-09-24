#include "next/application/calendar_event_import_signature.h"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace ClassMngr::Next::Application;

namespace
{
using Fields = CalendarEventImportSignatureFields;
using Signature = CalendarEventImportSignature;

template <typename T>
concept HasDatabaseId = requires(const T& value) { value.id; };

template <typename T>
concept HasStartTime = requires(const T& value) { value.startTime; };

template <typename T>
concept HasEndTime = requires(const T& value) { value.endTime; };

template <typename T>
concept HasRepeatSeriesId = requires(const T& value) { value.repeatSeriesId; };

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Fields canonicalFields()
{
    return {
        .simplifiedTitle = u"staff meeting",
        .normalizedEventType = u"Holiday",
        .startDateIso = u"2026-03-02",
        .endDateIso = u"2026-03-06",
        .allDay = true,
        .normalizedTimeStatus = u"Timed"
    };
}

Signature signature(const Fields& fields)
{
    return Signature::fromNormalizedFields(fields);
}

void fieldsKeepLegacyOrderAndFormatting()
{
    require(
        signature(canonicalFields()).value()
            == u"staff meeting|Holiday|2026-03-02|2026-03-06|1|Timed",
        "signature changed the legacy field order or delimiter format"
        );

    Fields notAllDay = canonicalFields();
    notAllDay.allDay = false;
    require(
        signature(notAllDay).value()
            == u"staff meeting|Holiday|2026-03-02|2026-03-06|0|Timed",
        "all-day flag did not retain the legacy 1/0 formatting"
        );
}

void exactUtf16CodeUnitsAreRetained()
{
    Fields fields = canonicalFields();
    fields.simplifiedTitle = {
        u'X',
        static_cast<char16_t>(0xD800),
        static_cast<char16_t>(0xD83D),
        static_cast<char16_t>(0xDE80)
    };

    std::u16string expected = fields.simplifiedTitle;
    expected.append(u"|Holiday|2026-03-02|2026-03-06|1|Timed");
    require(
        signature(fields).value() == expected,
        "signature changed one or more UTF-16 code units"
        );
}

void placeholderLikeTextRemainsOpaque()
{
    Fields fields = canonicalFields();
    fields.simplifiedTitle = u"title %2";

    require(
        signature(fields).value()
            == u"title %2|Holiday|2026-03-02|2026-03-06|1|Timed",
        "placeholder-like field text was rescanned or rewritten"
        );
}

void eachKeyFieldChangesTheSignature()
{
    const Signature original = signature(canonicalFields());

    Fields changed = canonicalFields();
    changed.simplifiedTitle = u"different title";
    require(signature(changed) != original, "title was omitted from signature");

    changed = canonicalFields();
    changed.normalizedEventType = u"Workshop";
    require(
        signature(changed) != original,
        "event type was omitted from signature"
        );

    changed = canonicalFields();
    changed.startDateIso = u"2026-03-03";
    require(
        signature(changed) != original,
        "start date was omitted from signature"
        );

    changed = canonicalFields();
    changed.endDateIso = u"2026-03-07";
    require(
        signature(changed) != original,
        "end date was omitted from signature"
        );

    changed = canonicalFields();
    changed.allDay = false;
    require(
        signature(changed) != original,
        "all-day status was omitted from signature"
        );

    changed = canonicalFields();
    changed.normalizedTimeStatus = u"Unknown";
    require(
        signature(changed) != original,
        "time status was omitted from signature"
        );
}

void signatureFieldsExcludeNonKeyMetadata()
{
    static_assert(!HasDatabaseId<Fields>);
    static_assert(!HasStartTime<Fields>);
    static_assert(!HasEndTime<Fields>);
    static_assert(!HasRepeatSeriesId<Fields>);
}
}

int main()
{
    try
    {
        fieldsKeepLegacyOrderAndFormatting();
        exactUtf16CodeUnitsAreRetained();
        placeholderLikeTextRemainsOpaque();
        eachKeyFieldChangesTheSignature();
        signatureFieldsExcludeNonKeyMetadata();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::cout << "5 Calendar Import signature cases passed\n";
    return 0;
}
