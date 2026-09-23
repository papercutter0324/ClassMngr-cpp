#include "next/application/calendar_event_import_signature_query_port.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

class FakeImportSignatureQueryPort final
    : public CalendarEventImportSignatureQueryPort
{
public:
    bool available = true;
    mutable CalendarEventImportSignatureRangeRequest lastRequest;

    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return available;
    }

    [[nodiscard]] CalendarEventImportSignatureQueryResult
    loadSignaturesInRange(
        const CalendarEventImportSignatureRangeRequest& request
        ) const override
    {
        lastRequest = request;
        return CalendarEventImportSignatureQueryResult::success({
            u"Caf\u00E9|Meeting|2026-09-23|2026-09-24|0|Timed"
        });
    }
};

} // namespace

class NextApplicationCalendarEventImportSignatureQueryPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void contractUsesOwnedDatesAndUtf16Signatures();
};

void NextApplicationCalendarEventImportSignatureQueryPortTests::
contractUsesOwnedDatesAndUtf16Signatures()
{
    static_assert(std::is_copy_constructible_v<
        CalendarEventImportSignatureRangeRequest
        >);
    static_assert(std::is_same_v<
        CalendarEventImportSignatureKeys,
        std::vector<std::u16string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventImportSignatureQueryPort&>()
                     .loadSignaturesInRange(
                         std::declval<const CalendarEventImportSignatureRangeRequest&>()
                         )),
        CalendarEventImportSignatureQueryResult
        >);
    static_assert(!std::is_copy_constructible_v<
        CalendarEventImportSignatureQueryPort
        >);

    FakeImportSignatureQueryPort port;
    const CalendarEventImportSignatureRangeRequest request{
        CalendarEventDate("2026-09-23"),
        CalendarEventDate("2026-09-24")
    };
    QVERIFY(port.isAvailable());
    const auto result = port.loadSignaturesInRange(request);
    QVERIFY(result);
    QVERIFY(port.lastRequest == request);
    QCOMPARE(result.value().size(), std::size_t{1});
    QCOMPARE(
        result.value().front(),
        std::u16string(u"Caf\u00E9|Meeting|2026-09-23|2026-09-24|0|Timed")
        );

    port.available = false;
    QVERIFY(!port.isAvailable());
}

QTEST_APPLESS_MAIN(NextApplicationCalendarEventImportSignatureQueryPortTests)

#include "next_application_calendar_event_import_signature_query_port_tests.moc"
