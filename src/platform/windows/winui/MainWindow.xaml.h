#pragma once

#include "MainWindow.g.h"
#include "pch.h"

#include "classmngr/engine/semantic_version.h"

namespace winrt::ClassMngrWinUI::implementation
{

struct MainWindow : MainWindowT<MainWindow>
{
    MainWindow();

    [[nodiscard]] bool runPhase1SmokeChecks();
    [[nodiscard]] bool runPhase1InputChecks();
    [[nodiscard]] bool runPhase1ThemeChecks();

    void ContinueButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );

private:
    classmngr::engine::SemanticVersion m_engineVersion;
};

} // namespace winrt::ClassMngrWinUI::implementation

namespace winrt::ClassMngrWinUI::factory_implementation
{

struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};

} // namespace winrt::ClassMngrWinUI::factory_implementation
