#include "next/application/campus_dashboard_selected_campus_read_query.h"

#include <cassert>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Domain::CampusId typedCampusId(const std::string& id)
{
    return Domain::CampusId::fromString(id).value();
}

class FakeCampusDashboardSelectedCampusReadPort final
    : public Application::CampusDashboardSelectedCampusReadPort
{
public:
    mutable int callCount = 0;
    mutable Domain::CampusId lastCampusId =
        *Domain::CampusId::fromString("initial");
    std::optional<Application::CampusDashboardSelectedCampusSnapshot> result;
    bool fail = false;
    bool returnDifferentId = false;
    std::string differentId = "other";

    [[nodiscard]] Application::CampusDashboardSelectedCampusReadResult
    loadCampus(const Domain::CampusId& campusId) const override
    {
        ++callCount;
        lastCampusId = campusId;
        if (fail)
        {
            return Application::CampusDashboardSelectedCampusReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected campus read failure",
                .recoverable = true
            });
        }

        if (returnDifferentId)
        {
            auto mismatched = snapshotFor(differentId);
            return Application::CampusDashboardSelectedCampusReadResult::success(
                std::move(mismatched)
                );
        }

        return Application::CampusDashboardSelectedCampusReadResult::success(
            result
            );
    }

    static Application::CampusDashboardSelectedCampusSnapshot snapshotFor(
        const std::string& id
        )
    {
        return Application::CampusDashboardSelectedCampusSnapshot{
            .id = *Domain::CampusId::fromString(id),
            .campusName = "Sample Campus"
        };
    }
};

void rejectsBlankAndOversizedIdsBeforeCallingPort()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    for (const std::string id : {
             std::string(" \t\r\n"),
             std::string("\xC2\xA0"),
             std::string("\xE3\x80\x80"),
             std::string(
                 Application::kCampusDashboardCampusIdMaxLength + 1,
                 'x'
                 )
         })
    {
        const auto result =
            Application::CampusDashboardSelectedCampusReadQuery::execute(
                {.campusId = typedCampusId(id)},
                port
                );
        assert(!result);
        assert(result.error().code == Domain::ErrorCode::InvalidInput);
        assert(
            result.error().message
            == "Campus identifier must be non-blank and bounded."
            );
        assert(!result.error().recoverable);
    }
    assert(port.callCount == 0);
}

void acceptsIdentifierAtExactByteLimit()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    std::string id(
        Application::kCampusDashboardCampusIdMaxLength - 4,
        'x'
        );
    id += "\xF0\x9F\x8C\x8D";
    assert(id.size() == Application::kCampusDashboardCampusIdMaxLength);
    port.result = FakeCampusDashboardSelectedCampusReadPort::snapshotFor(id);

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId(id)},
            port
            );

    assert(result);
    assert(port.callCount == 1);
    assert(port.lastCampusId.value() == id);
    assert(result.value().has_value());
    assert(result.value()->id.value() == id);
}

void preservesNonAsciiCampusIdBytes()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    const std::string id(
        "\xEC\x84\x9C\xEC\x9A\xB8\x20"
        "\xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"
        );
    port.result = FakeCampusDashboardSelectedCampusReadPort::snapshotFor(id);

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId(id)},
            port
            );

    assert(result);
    assert(port.callCount == 1);
    assert(port.lastCampusId.value() == id);
    assert(result.value().has_value());
    assert(result.value()->id.value() == id);
}

void preservesSurroundingWhitespaceInCampusId()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    const std::string id = " Alpha ";
    port.result = FakeCampusDashboardSelectedCampusReadPort::snapshotFor(id);

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId(id)},
            port
            );

    assert(result);
    assert(port.callCount == 1);
    assert(port.lastCampusId.value() == id);
    assert(result.value().has_value());
    assert(result.value()->id.value() == id);
}

void doesNotRejectMalformedUtf8CampusIdBytes()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    const std::string id("\xC3", 1);
    port.result = FakeCampusDashboardSelectedCampusReadPort::snapshotFor(id);

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId(id)},
            port
            );

    assert(result);
    assert(port.callCount == 1);
    assert(port.lastCampusId.value() == id);
}

void callsPortOnceAndReturnsTypedSnapshot()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    port.result = FakeCampusDashboardSelectedCampusReadPort::snapshotFor("alpha");

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId("alpha")},
            port
            );

    assert(result);
    assert(port.callCount == 1);
    assert(port.lastCampusId == *Domain::CampusId::fromString("alpha"));
    assert(result.value().has_value());
    assert(result.value()->id == port.lastCampusId);
    assert(result.value()->campusName == "Sample Campus");
}

void propagatesPortErrorAndSuccessfulAbsence()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    port.fail = true;

    const auto failed =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId("alpha")},
            port
            );
    assert(!failed);
    assert(failed.error().code == Domain::ErrorCode::Technical);
    assert(failed.error().message == "injected campus read failure");
    assert(failed.error().recoverable);
    assert(port.callCount == 1);

    port.fail = false;
    const auto missing =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId("alpha")},
            port
            );
    assert(missing);
    assert(!missing.value().has_value());
    assert(port.callCount == 2);
}

void rejectsMismatchedReturnedId()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    port.returnDifferentId = true;
    port.differentId = "alpha";

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = typedCampusId("Alpha")},
            port
            );

    assert(!result);
    assert(result.error().code == Domain::ErrorCode::Technical);
    assert(!result.error().recoverable);
    assert(
        result.error().message
        == "Campus detail result did not match the requested campus."
        );
    assert(port.callCount == 1);
}

}

int main()
{
    rejectsBlankAndOversizedIdsBeforeCallingPort();
    acceptsIdentifierAtExactByteLimit();
    preservesNonAsciiCampusIdBytes();
    preservesSurroundingWhitespaceInCampusId();
    doesNotRejectMalformedUtf8CampusIdBytes();
    callsPortOnceAndReturnsTypedSnapshot();
    propagatesPortErrorAndSuccessfulAbsence();
    rejectsMismatchedReturnedId();
}
