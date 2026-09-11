#pragma once

#include "pch.h"

#include <memory>
#include <optional>
#include <string>

namespace classmngr::windows::winui
{

// A deliberately isolated harness: it never creates MainWindow and therefore
// cannot restore or persist normal shell state.
class Phase4LargeDataDiagnostics
{
public:
    struct LaunchOptions
    {
        std::wstring outputPath;
        std::wstring runId;
    };

    [[nodiscard]] static bool requested(wchar_t const* commandLine);
    [[nodiscard]] static std::optional<LaunchOptions> parse(
        wchar_t const* commandLine
        );

    explicit Phase4LargeDataDiagnostics(LaunchOptions options);
    ~Phase4LargeDataDiagnostics();

    void start(winrt::Microsoft::UI::Xaml::Window const& window);

private:
    struct Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

} // namespace classmngr::windows::winui
