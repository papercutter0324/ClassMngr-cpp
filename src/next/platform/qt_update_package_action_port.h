#pragma once

#include "next/application/update_package_action.h"

namespace ClassMngr::Next::Platform
{

class QtUpdatePackageActionPort final : public Application::UpdatePackageActionPort
{
public:
    [[nodiscard]] Application::UpdatePackageActionKind primaryAction() const override;
    [[nodiscard]] Application::UpdatePackageActionResult launchInstaller(
        const std::string& filePath
        ) override;
    [[nodiscard]] Application::UpdatePackageActionResult revealInFolder(
        const std::string& filePath
        ) override;
};

} // namespace ClassMngr::Next::Platform
