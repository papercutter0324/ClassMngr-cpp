#pragma once

#include "App.xaml.g.h"
#include "pch.h"

namespace winrt::ClassMngrWinUI::implementation
{

struct App : AppT<App>
{
    App();

    void OnLaunched(
        Microsoft::UI::Xaml::LaunchActivatedEventArgs const& arguments
        );

private:
    Microsoft::UI::Xaml::Window m_window{nullptr};
};

} // namespace winrt::ClassMngrWinUI::implementation

namespace winrt::ClassMngrWinUI::factory_implementation
{

struct App : AppT<App, implementation::App>
{
};

} // namespace winrt::ClassMngrWinUI::factory_implementation
