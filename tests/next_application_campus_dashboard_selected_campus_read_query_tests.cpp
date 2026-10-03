#include "next/application/campus_dashboard_selected_campus_read_query.h"

#include <cassert>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

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
            auto mismatched = snapshotFor("other");
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
             std::string(),
             std::string(" \t\r\n"),
             std::string(
                 Application::kCampusDashboardCampusIdMaxLength + 1,
                 'x'
                 )
         })
    {
        const auto result =
            Application::CampusDashboardSelectedCampusReadQuery::execute(
                {.campusId = id},
                port
                );
        assert(!result);
        assert(result.error().code == Domain::ErrorCode::InvalidInput);
    }
    assert(port.callCount == 0);
}

void callsPortOnceAndReturnsTypedSnapshot()
{
    FakeCampusDashboardSelectedCampusReadPort port;
    port.result = FakeCampusDashboardSelectedCampusReadPort::snapshotFor("alpha");

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = "alpha"},
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
            {.campusId = "alpha"},
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
            {.campusId = "alpha"},
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

    const auto result =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            {.campusId = "alpha"},
            port
            );

    assert(!result);
    assert(result.error().code == Domain::ErrorCode::Technical);
    assert(port.callCount == 1);
}

}

int main()
{
    rejectsBlankAndOversizedIdsBeforeCallingPort();
    callsPortOnceAndReturnsTypedSnapshot();
    propagatesPortErrorAndSuccessfulAbsence();
    rejectsMismatchedReturnedId();
}
