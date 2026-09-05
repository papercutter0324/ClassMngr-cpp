#pragma once

#include "pch.h"
#include "MainWindow.g.h"

#include "classmngr/engine/semantic_version.h"
#include "winui_localization.h"
#include "winui_view_model.h"

#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

#include <string>
#include <string_view>

namespace winrt::ClassMngrWinUI::implementation
{

struct MainWindow : MainWindowT<MainWindow>
{
    MainWindow();
    ~MainWindow();

    [[nodiscard]] bool runPhase1SmokeChecks();
    [[nodiscard]] bool runPhase1InputChecks();
    [[nodiscard]] bool runPhase1ThemeChecks();
    [[nodiscard]] bool runPhase1DpiChecks();
    [[nodiscard]] bool runPhase3NavigationChecks();
    [[nodiscard]] bool runPhase3LocalizationChecks();
    [[nodiscard]] Windows::Foundation::IAsyncOperation<bool>
        runPhase3ViewModelChecks();

    void ContinueButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ShellInfoButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );

private:
    void NavigationView_SelectionChanged(
        Microsoft::UI::Xaml::Controls::NavigationView const& sender,
        Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& arguments
        );
    void NavigationView_BackRequested(
        Microsoft::UI::Xaml::Controls::NavigationView const& sender,
        Microsoft::UI::Xaml::Controls::NavigationViewBackRequestedEventArgs const& arguments
        );
    void ContentFrame_Navigated(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& arguments
        );
    void Window_Activated(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::WindowActivatedEventArgs const& arguments
        );
    void Window_Closed(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::WindowEventArgs const& arguments
        );

    void navigateTo(std::wstring_view pageId);
    void populatePage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        std::wstring_view pageId
        );
    void populateHomePage(
        Microsoft::UI::Xaml::Controls::Page const& page
        );
    void populateAboutPage(
        Microsoft::UI::Xaml::Controls::Page const& page
        );
    void restoreShellState();
    void restoreWindowBounds() noexcept;
    void saveShellState() noexcept;
    void updateNavigationState();
    void showOwnedDialog();
    void closeShell() noexcept;

    [[nodiscard]] std::wstring selectedPageId() const;
    [[nodiscard]] bool ensureHomePage();

    Microsoft::UI::Xaml::Controls::Grid m_appTitleBar{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationView m_navigationView{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_homeNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_aboutNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::Frame m_contentFrame{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_shellInfoButton{nullptr};

    Microsoft::UI::Xaml::Controls::TextBlock m_engineVersionText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_continueButton{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_statusText{nullptr};

    classmngr::engine::SemanticVersion m_engineVersion;
    WinUILocalizer m_localizer;
    std::wstring m_currentPageId;
    Microsoft::UI::Xaml::Controls::ContentDialog m_ownedDialog{nullptr};

    winrt::event_token m_selectionChangedToken{};
    winrt::event_token m_backRequestedToken{};
    winrt::event_token m_navigatedToken{};
    winrt::event_token m_activatedToken{};
    winrt::event_token m_closedToken{};
    bool m_restoringState{};
    bool m_selectionChanging{};
    bool m_windowBoundsRestored{};
};

} // namespace winrt::ClassMngrWinUI::implementation

namespace winrt::ClassMngrWinUI::factory_implementation
{

struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};

} // namespace winrt::ClassMngrWinUI::factory_implementation
