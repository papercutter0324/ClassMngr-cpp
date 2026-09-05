#pragma once

#include "pch.h"
#include "App.xaml.g.h"

#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>

namespace winrt::ClassMngrWinUI::implementation
{

struct App : AppT<App>
{
    App();
    ~App();

    void OnLaunched(
        Microsoft::UI::Xaml::LaunchActivatedEventArgs const& arguments
        );

private:
    [[nodiscard]] bool initializeSingleInstance();
    void OnAppInstanceActivated(
        Microsoft::Windows::AppLifecycle::AppActivationArguments const& arguments
        );
    void handleActivation(
        Microsoft::Windows::AppLifecycle::AppActivationArguments const& arguments
        );
    void OnSuspending(
        Windows::Foundation::IInspectable const& sender,
        Windows::ApplicationModel::SuspendingEventArgs const& arguments
        );
    void OnResuming(
        Windows::Foundation::IInspectable const& sender,
        Windows::Foundation::IInspectable const& arguments
        );
    void OnWindowClosed(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::WindowEventArgs const& arguments
        );
    void OnUnhandledException(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& arguments
        );
    void closeLifecycle() noexcept;
    void terminateAfterFatalError() noexcept;

    Microsoft::UI::Xaml::Window m_window{nullptr};
    Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{nullptr};
    Microsoft::Windows::AppLifecycle::AppInstance m_instance{nullptr};
    winrt::event_token m_activationToken{};
    winrt::event_token m_suspendingToken{};
    winrt::event_token m_resumingToken{};
    winrt::event_token m_windowClosedToken{};
    winrt::event_token m_unhandledExceptionToken{};
    bool m_isSuspended{};
};

} // namespace winrt::ClassMngrWinUI::implementation
