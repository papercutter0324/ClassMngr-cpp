#include "pch.h"

#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"
#include "winui_build_info.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

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

    m_appTitleBar = RootGrid().FindName(L"AppTitleBar").as<
        Microsoft::UI::Xaml::Controls::Grid>();
    m_navigationView = RootGrid().FindName(L"RootNavigationView").as<
        Microsoft::UI::Xaml::Controls::NavigationView>();
    m_homeNavigationItem = RootGrid().FindName(L"HomeNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_subPrepNavigationItem = RootGrid().FindName(
        L"SubPrepNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classesNavigationItem = RootGrid().FindName(L"ClassesNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_aboutNavigationItem = RootGrid().FindName(L"AboutNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusInformationNavigationItem = RootGrid().FindName(
        L"CampusInformationNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusDirectionsNavigationItem = RootGrid().FindName(
        L"CampusDirectionsNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusAddressNavigationItem = RootGrid().FindName(
        L"CampusAddressNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusHousingNavigationItem = RootGrid().FindName(
        L"CampusHousingNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusMapNavigationItem = RootGrid().FindName(
        L"CampusMapNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_koreanTeachersNavigationItem = RootGrid().FindName(
        L"KoreanTeachersNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_nativeEnglishTeachersNavigationItem = RootGrid().FindName(
        L"NativeEnglishTeachersNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_gsTeamNavigationItem = RootGrid().FindName(
        L"GsTeamNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_contentFrame = RootGrid().FindName(L"ContentFrame").as<
        Microsoft::UI::Xaml::Controls::Frame>();
    m_shellInfoButton = RootGrid().FindName(L"ShellInfoButton").as<
        Microsoft::UI::Xaml::Controls::Button>();
    m_recentFilesMenu = RootGrid().FindName(L"RecentFilesMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutSubItem>();
    m_shellDatabaseStatusText = RootGrid().FindName(
        L"ShellDatabaseStatusText"
        ).as<Microsoft::UI::Xaml::Controls::TextBlock>();
    m_saveFileMenu = RootGrid().FindName(L"SaveFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_saveAsFileMenu = RootGrid().FindName(L"SaveAsFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_exportFileMenu = RootGrid().FindName(L"ExportFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_closeFileMenu = RootGrid().FindName(L"CloseFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_printCurrentPageMenu = RootGrid().FindName(
        L"PrintCurrentPageMenuItem"
        ).as<Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_saveCurrentPageMenu = RootGrid().FindName(
        L"SaveCurrentPageMenuItem"
        ).as<Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_exportCampusResourcesMenu = RootGrid().FindName(
        L"ExportCampusResourcesMenuItem"
        ).as<Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();

    m_aboutNavigationItem.Content(winrt::box_value(winrt::hstring(
        m_localizer.getString(L"ActionRegistry", L"About")
        )));
    m_shellInfoButton.Content(winrt::box_value(winrt::hstring(
        m_localizer.getString(
            L"ActionRegistry",
            L"Show application information"
            )
        )));

    ExtendsContentIntoTitleBar(true);
    SetTitleBar(m_appTitleBar);
    try
    {
        const auto appWindow = AppWindow();
        appWindow.Resize(
            Windows::Graphics::SizeInt32{
                defaultShellWidth,
                defaultShellHeight
                }
            );

        const auto presenter = appWindow.Presenter().try_as<
            Microsoft::UI::Windowing::OverlappedPresenter>();
        if (presenter)
        {
            presenter.PreferredMinimumWidth(
                winrt::box_value(minimumShellWidth).as<
                    Windows::Foundation::IReference<int32_t>>()
                );
            presenter.PreferredMinimumHeight(
                winrt::box_value(minimumShellHeight).as<
                    Windows::Foundation::IReference<int32_t>>()
                );
        }
    }
    catch (...)
    {
        // Persisted bounds and XAML minimums remain the safe fallback when
        // the windowing presenter is unavailable during early startup.
    }
    m_contentFrame.CacheSize(3);
    m_contentFrame.IsNavigationStackEnabled(true);

    m_selectionChangedToken = m_navigationView.SelectionChanged(
        {this, &MainWindow::NavigationView_SelectionChanged}
        );
    m_backRequestedToken = m_navigationView.BackRequested(
        {this, &MainWindow::NavigationView_BackRequested}
        );
    m_navigatedToken = m_contentFrame.Navigated(
        {this, &MainWindow::ContentFrame_Navigated}
        );
    m_activatedToken = Activated({this, &MainWindow::Window_Activated});
    m_closedToken = Closed({this, &MainWindow::Window_Closed});

    restoreShellState();
    refreshRecentDatabaseMenu();
    updateFileCommandState();
}

MainWindow::~MainWindow()
{
    closeShell();
}

ClassNavigationLocation MainWindow::classNavigationLocation() const noexcept
{
    return m_openDatabase
        ? m_classNavigationLocation
        : ClassNavigationLocation::Top;
}

void MainWindow::classNavigationLocation(ClassNavigationLocation location)
{
    if (location != ClassNavigationLocation::Top
        && location != ClassNavigationLocation::Bottom)
    {
        location = ClassNavigationLocation::Top;
    }
    if (!m_openDatabase)
    {
        location = ClassNavigationLocation::Top;
    }

    m_classNavigationLocation = location;
    applyClassNavigationLayout();

    if (m_openDatabase)
    {
        const std::string value = location == ClassNavigationLocation::Bottom
            ? "bottom"
            : "top";
        classmngr::engine::ApplicationSettingsService settings(
            *m_openDatabase
            );
        static_cast<void>(settings.save(
            classNavigationLocationKey,
            classmngr::engine::SettingValue{value}
            ));
    }
}

ClassNavigationLocation MainWindow::getClassNavigationLocation() const noexcept
{
    return classNavigationLocation();
}

void MainWindow::setClassNavigationLocation(ClassNavigationLocation location)
{
    classNavigationLocation(location);
}

} // namespace winrt::ClassMngrWinUI::implementation
