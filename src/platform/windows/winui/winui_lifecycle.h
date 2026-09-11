#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ClassMngrWinUILifecycle
{

enum class InstanceDisposition
{
    Primary,
    Secondary,
};

struct CommandLineActivation
{
    std::vector<std::wstring> arguments;
    std::vector<std::wstring> openTargets;
    bool lifecycleTest{};
};

[[nodiscard]] CommandLineActivation parseArguments(
    std::span<std::wstring_view const> arguments
    );
[[nodiscard]] CommandLineActivation parseWindowsCommandLine(
    std::wstring_view commandLine
    );
[[nodiscard]] CommandLineActivation parseCommandLineActivationArguments(
    std::wstring_view arguments
    );
[[nodiscard]] bool hasArgument(
    CommandLineActivation const& activation,
    std::wstring_view argument
    );
[[nodiscard]] InstanceDisposition instanceDisposition(bool isCurrent) noexcept;
[[nodiscard]] bool runLifecycleContractChecks() noexcept;

void reportFatalError(std::wstring_view message) noexcept;

} // namespace ClassMngrWinUILifecycle
