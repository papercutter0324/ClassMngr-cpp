#pragma once

#include "pch.h"
#include "App.xaml.g.h"

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
