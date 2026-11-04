#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class UpdatePackageActionKind
{
    InstallOrLaunch,
    RevealInFolder
};

struct UpdatePackageActionResult final
{
    bool succeeded = false;
    std::string message;

    [[nodiscard]] static UpdatePackageActionResult success()
    {
        return {.succeeded = true};
    }

    [[nodiscard]] static UpdatePackageActionResult failure(std::string message)
    {
        return {.succeeded = false, .message = std::move(message)};
    }
};

// The dialog selects localized labels, asks for confirmation, and decides
// what to do after success. This port performs only the platform action.
class UpdatePackageActionPort
{
public:
    virtual ~UpdatePackageActionPort() = default;

    [[nodiscard]] virtual UpdatePackageActionKind primaryAction() const = 0;
    [[nodiscard]] virtual UpdatePackageActionResult launchInstaller(
        const std::string& filePath
        ) = 0;
    [[nodiscard]] virtual UpdatePackageActionResult revealInFolder(
        const std::string& filePath
        ) = 0;
};

} // namespace ClassMngr::Next::Application
