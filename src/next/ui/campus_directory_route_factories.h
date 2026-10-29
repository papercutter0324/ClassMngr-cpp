#pragma once

#include "next/ui/campus_directory_page.h"
#include "next/ui/page_descriptor.h"

#include <functional>
#include <vector>

namespace ClassMngr::Next::Ui
{

using CampusDirectoryPageActionsFactory =
    std::function<CampusDirectoryPageActions()>;

[[nodiscard]] std::vector<PageDescriptor> campusDirectoryPageDescriptors(
    CampusDirectoryPageActionsFactory actionsFactory
    );

} // namespace ClassMngr::Next::Ui
