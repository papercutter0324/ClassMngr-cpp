#include "pch.h"

#include "MainWindow.xaml.h"
#include "winui_build_info.h"

#include <string>

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace winrt::ClassMngrWinUI::implementation
{

MainWindow::MainWindow()
{
    InitializeComponent();

    const auto parsedVersion = classmngr::engine::SemanticVersion::parse(
        ::ClassMngrWinUI::BuildInfo::Version
        );
    if (parsedVersion)
    {
        m_engineVersion = *parsedVersion;
    }

    EngineVersionText().Text(
        winrt::to_hstring(
            std::string("Engine version: ") + m_engineVersion.toString()
            )
        );
}

bool MainWindow::runPhase1SmokeChecks()
{
    return m_engineVersion.isValid()
        && static_cast<bool>(RootGrid())
        && static_cast<bool>(EngineVersionText())
        && static_cast<bool>(NameTextBox())
        && static_cast<bool>(ContinueButton())
        && static_cast<bool>(StatusText())
        && EngineVersionText().Text()
            == winrt::to_hstring(
                std::string("Engine version: ")
                    + m_engineVersion.toString()
                );
}

bool MainWindow::runPhase1InputChecks()
{
    NameTextBox().Text(L"한글 입력");
    // Request focus so an interactive run exercises the real control. A
    // process launched by a headless test runner cannot always receive OS
    // focus, so the deterministic gate checks the tab-stop contract below.
    NameTextBox().Focus(
        Microsoft::UI::Xaml::FocusState::Programmatic
        );

    return NameTextBox().Text() == winrt::hstring(L"한글 입력")
        && NameTextBox().IsTabStop()
        && NameTextBox().TabIndex() == 0
        && ContinueButton().IsTabStop()
        && ContinueButton().TabIndex() == 1;
}

bool MainWindow::runPhase1ThemeChecks()
{
    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Light);
    const bool lightTheme = RootGrid().ActualTheme()
        == Microsoft::UI::Xaml::ElementTheme::Light;

    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Dark);
    const bool darkTheme = RootGrid().ActualTheme()
        == Microsoft::UI::Xaml::ElementTheme::Dark;

    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Default);
    return lightTheme && darkTheme;
}

void MainWindow::ContinueButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);

    StatusText().Text(
        NameTextBox().Text().empty()
            ? L"Enter a name."
            : L"Continue clicked"
        );
}

} // namespace winrt::ClassMngrWinUI::implementation
