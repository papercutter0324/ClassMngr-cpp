#include "next/ui/campus_directory_route_factories.h"

#include <QCoreApplication>

#include <array>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

struct Route final
{
    const char* id;
    const char* label;
    CampusDirectorySection section;
};

PageDescriptor descriptorFor(
    const Route& route,
    const CampusDirectoryPageActionsFactory& actionsFactory
    )
{
    const auto id = PageId::fromString(QString::fromLatin1(route.id));
    return PageDescriptor{
        .id = *id,
        .label = QCoreApplication::translate("NextNavigation", route.label),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = [section = route.section, actionsFactory](
                       const PageContext& context
                       ) mutable -> PageOperationResult<std::unique_ptr<Page>> {
            return std::unique_ptr<Page>(std::make_unique<CampusDirectoryPage>(
                context,
                section,
                actionsFactory()
                ));
        },
        .resourcePolicy = PageResourcePolicy::PageScoped,
        .initialDataPolicy = PageInitialDataPolicy::None
    };
}

} // namespace

std::vector<PageDescriptor> campusDirectoryPageDescriptors(
    CampusDirectoryPageActionsFactory actionsFactory
    )
{
    constexpr std::array<Route, 5> routes{{
        {"campus_information", "Information", CampusDirectorySection::Information},
        {"campus_directions", "Directions", CampusDirectorySection::Directions},
        {"campus_address", "Address", CampusDirectorySection::Address},
        {"campus_housing", "Housing", CampusDirectorySection::Housing},
        {"campus_map", "Maps", CampusDirectorySection::Maps}
    }};
    std::vector<PageDescriptor> descriptors;
    descriptors.reserve(routes.size());
    for (const Route& route : routes)
    {
        descriptors.push_back(descriptorFor(route, actionsFactory));
    }
    return descriptors;
}

} // namespace ClassMngr::Next::Ui
