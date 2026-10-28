#pragma once

#include "next/application/calendar_page_campus_directory_query_port.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"

namespace ClassMngr::Next::Platform
{

// Reads the small typed campus projection from the Next package catalog. The
// adapter leaves full campus records and their lifetime with ResourceLoader.
class CalendarPageResourceCampusDirectoryQueryAdapter final
    : public Application::CalendarPageCampusDirectoryQueryPort
{
public:
    CalendarPageResourceCampusDirectoryQueryAdapter(
        Resources::ResourceLoader& resourceLoader,
        Resources::ResourceScope resourceScope
        );

    [[nodiscard]] std::vector<Application::CalendarPageCampusMetadata>
    loadCampuses() const override;

private:
    Resources::ResourceLoader* m_resourceLoader;
    Resources::ResourceScope m_resourceScope;
};

} // namespace ClassMngr::Next::Platform
