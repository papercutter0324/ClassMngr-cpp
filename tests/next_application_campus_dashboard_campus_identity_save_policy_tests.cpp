#include "next/application/campus_dashboard_campus_identity_save_policy.h"

#include <cassert>
#include <string>
#include <string_view>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Application::CampusDashboardCampusSnapshot snapshotFor(
    const std::string_view id,
    const std::string_view name
    )
{
    return Application::CampusDashboardCampusSnapshot{
        .id = *Domain::CampusId::fromString(id),
        .campusName = std::string(name)
    };
}

void assertInvalidInput(
    const Domain::Result<void>& result,
    const std::string_view operation
    )
{
    assert(!result);
    assert(result.error().code == Domain::ErrorCode::InvalidInput);
    assert(!result.error().recoverable);
    assert(
        result.error().message.find(operation)
        != std::string::npos
        );
    assert(
        result.error().message.find("reserved Default identity")
        != std::string::npos
        );
}

void rejectsReservedIdsForCreateAndUpdate()
{
    for (const std::string_view id : {"default", "DEFAULT", "DeFaUlT"})
    {
        const auto campus = snapshotFor(id, "Alpha");
        const auto create =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                campus,
                Application::CampusDashboardCampusSaveIntent::Create
                );
        assertInvalidInput(create, "create");

        const auto update =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                campus,
                Application::CampusDashboardCampusSaveIntent::Update
                );
        assertInvalidInput(update, "update");
    }
}

void rejectsReservedNamesForCreateAndUpdate()
{
    for (const std::string_view name : {"Default", "DEFAULT", "dEfAuLt"})
    {
        const auto campus = snapshotFor("alpha", name);
        const auto create =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                campus,
                Application::CampusDashboardCampusSaveIntent::Create
                );
        assertInvalidInput(create, "create");

        const auto update =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                campus,
                Application::CampusDashboardCampusSaveIntent::Update
                );
        assertInvalidInput(update, "update");
    }
}

void acceptsOrdinaryIdsAndNamesForCreateAndUpdate()
{
    for (const auto intent : {
             Application::CampusDashboardCampusSaveIntent::Create,
             Application::CampusDashboardCampusSaveIntent::Update
         })
    {
        const auto result =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                snapshotFor("alpha", "Alpha"),
                intent
                );
        assert(result);
    }
}

void reservedIdentityComparisonIsExact()
{
    for (const auto intent : {
             Application::CampusDashboardCampusSaveIntent::Create,
             Application::CampusDashboardCampusSaveIntent::Update
         })
    {
        const auto result =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                snapshotFor("default-campus", "Default Campus"),
                intent
                );
        assert(result);
    }
}

}

int main()
{
    rejectsReservedIdsForCreateAndUpdate();
    rejectsReservedNamesForCreateAndUpdate();
    acceptsOrdinaryIdsAndNamesForCreateAndUpdate();
    reservedIdentityComparisonIsExact();
}
