#pragma once

#include "pch.h"
#include "MainWindow.g.h"

#include "classmngr/engine/semantic_version.h"
#include "classmngr/engine/campus_record.h"
#include "classmngr/engine/validation_result.h"
#include "winui_dialogs.h"
#include "winui_localization.h"
#include "winui_view_model.h"

#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace classmngr::engine
{
class SqliteDatabase;
}

namespace winrt::ClassMngrWinUI::implementation
{

struct CampusAddressView
{
    std::wstring buildingName;
    std::wstring province;
    std::wstring city;
    std::wstring cityDistrict;
    std::wstring district;
    std::wstring line1;
    std::wstring line2;
    std::wstring postalCode;
    std::wstring addressSystem;
};

struct CampusHousingView
{
    std::wstring name;
    CampusAddressView englishAddress;
    CampusAddressView koreanAddress;
    std::wstring addressNote;
    std::vector<std::string> imagePaths;
    std::wstring naverMapUrl;
    std::wstring kakaoMapUrl;
};

// This is the read-only Windows presentation model for the same resource
// catalog used by the Qt Campus Directory.  Database-backed phase checks are
// converted into this shape at the UI boundary so the production page does
// not accidentally substitute the .tps campuses table for the catalog.
struct CampusResourceView
{
    std::wstring id;
    std::wstring campusName;
    std::wstring campusCode;
    std::wstring buildingName;
    std::wstring buildingNameKr;
    std::wstring address;
    std::wstring phoneNumber;
    std::wstring officeNumber;
    CampusAddressView englishAddress;
    CampusAddressView koreanAddress;
    std::wstring directionsNote;
    std::vector<std::wstring> transitSteps;
    std::wstring arrivalInfo;
    std::vector<std::string> mapImagePaths;
    std::wstring naverMapUrl;
    std::wstring kakaoMapUrl;
    std::wstring officeWifi;
    std::wstring officeWifiPassword;
    std::wstring printerName;
    std::wstring printerSteps;
    std::wstring printerDriverUrl;
    bool printerDriverUrlUnavailable = true;
    std::wstring photocopierCode;
    std::vector<CampusHousingView> housingLocations;
};

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
    [[nodiscard]] bool runPhase3DialogChecks();
    [[nodiscard]] bool runPhase4SemanticChecks();
    [[nodiscard]] uint32_t phase4SemanticFailureMask();
    [[nodiscard]] bool runPhase5CampusChecks();
    void preparePhase5CampusScenario(std::wstring_view scenario);
    void startPhase5FirstNavigationMeasurement(std::function<void(bool)> completion);
    [[nodiscard]] bool openDatabasePath(std::wstring_view path);
    [[nodiscard]] bool createDatabasePath(std::wstring_view path);
    void openMostRecentDatabase();
    [[nodiscard]] Windows::Foundation::IAsyncOperation<bool>
        runPhase3ViewModelChecks();
    [[nodiscard]] Windows::Foundation::IAsyncOperation<bool>
        runPhase3SemanticChecks();

    void ContinueButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ShellInfoButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ShellInfoMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void OpenDatabaseMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void NewDatabaseMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void SaveDatabaseMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void SaveDatabaseAsMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ExportDatabaseMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CloseDatabaseMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void SaveCurrentPageMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ExportCampusResourcesMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void RecentDatabaseMenuItem_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CancelButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void UnsavedChangesButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ScheduleApplyButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void RosterSource_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void RosterTransferButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void SpeakingPasteButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void SpeakingAnalyticsButton_Click(
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
    void populateClassesPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        std::wstring_view pageId
        );
    void populateAboutPage(
        Microsoft::UI::Xaml::Controls::Page const& page
        );
    void populateCampusPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        std::wstring_view pageId,
        bool refresh
        );
    void refreshCampusInformationPage();
    [[nodiscard]] bool preparePhase5CampusFixture(std::wstring_view scenario);
    void Phase5FirstNavigation_Rendering(
        Windows::Foundation::IInspectable const& sender,
        Windows::Foundation::IInspectable const& arguments
        );
    void completePhase5FirstNavigationMeasurement(std::string_view failure);
    [[nodiscard]] bool writePhase5FirstNavigationResult(
        std::string_view failure
        ) const;
    void CampusSelector_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void presentSelectedCampus(std::wstring_view pageId);
    winrt::fire_and_forget loadCampusImage(
        std::string logicalPath,
        std::uint64_t requestId,
        Microsoft::UI::Xaml::Controls::Image target
        );
    void NameTextBox_TextChanged(
        Microsoft::UI::Xaml::Controls::TextBox const& sender,
        Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
        );
    winrt::fire_and_forget openDatabasePicker();
    winrt::fire_and_forget openNewDatabasePicker();
    winrt::fire_and_forget openSaveDatabasePicker(bool exportOnly);
    winrt::fire_and_forget openCurrentPageSavePicker();
    winrt::fire_and_forget openCampusResourcesFolderPicker();
    [[nodiscard]] bool saveDatabasePath(std::wstring_view path);
    [[nodiscard]] bool exportDatabasePath(std::wstring_view path);
    [[nodiscard]] bool saveCurrentPagePath(std::wstring_view path);
    [[nodiscard]] bool exportCampusResourcesPath(std::wstring_view path);
    [[nodiscard]] std::string currentPageExportJson() const;
    void restoreShellState();
    void restoreWindowBounds() noexcept;
    void saveShellState() noexcept;
    void updateNavigationState();
    void showOwnedDialog();
    void showUnsavedChangesConfirmation();
    void showDialog(
        winrt::hstring const& title,
        winrt::hstring const& content,
        winrt::hstring const& primaryText,
        winrt::hstring const& secondaryText,
        winrt::hstring const& closeText,
        std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
        );
    winrt::fire_and_forget completeOwnedDialog(
        Microsoft::UI::Xaml::Controls::ContentDialog dialog,
        std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
        );
    void updateHomePresentation();
    void presentValidationSummary(
        classmngr::engine::ValidationResult const& validation
        );
    void closeShell() noexcept;
    void refreshRecentDatabaseMenu();
    void addRecentDatabasePath(std::wstring_view path);
    void reportDatabaseOpenError(
        std::wstring_view path,
        std::string_view message
        );
    void reportOutputError(
        std::wstring_view title,
        std::wstring_view path,
        std::string_view message
        );
    void updateFileCommandState();

    [[nodiscard]] std::wstring selectedPageId() const;
    [[nodiscard]] bool ensureHomePage();

    Microsoft::UI::Xaml::Controls::Grid m_appTitleBar{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationView m_navigationView{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_homeNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_workspaceInformationNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_workspaceScheduleNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_workspaceCalendarNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classesNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classDetailsNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classRosterNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classSpeakingEvaluationsNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classAnalyticsNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classNotesNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_aboutNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusInformationNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusDirectionsNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusAddressNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusHousingNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusMapNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::Frame m_contentFrame{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_shellInfoButton{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutSubItem m_recentFilesMenu{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_shellDatabaseStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_saveFileMenu{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_saveAsFileMenu{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_exportFileMenu{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_closeFileMenu{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_saveCurrentPageMenu{nullptr};
    Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_exportCampusResourcesMenu{nullptr};

    Microsoft::UI::Xaml::Controls::TextBlock m_engineVersionText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_continueButton{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_statusText{nullptr};
    Microsoft::UI::Xaml::Controls::ProgressRing m_progressRing{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_cancelButton{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_validationSummaryText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_unsavedChangesButton{nullptr};

    Microsoft::UI::Xaml::Controls::TextBox m_scheduleSlotTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_scheduleStatusText{nullptr};

    Microsoft::UI::Xaml::Controls::ListView m_rosterSourceList{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_rosterTransferredList{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_rosterStatusText{nullptr};

    std::vector<Microsoft::UI::Xaml::Controls::TextBox>
        m_speakingScoreCells;
    Microsoft::UI::Xaml::Controls::TextBox m_speakingPasteTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_speakingStatusText{nullptr};

    Microsoft::UI::Xaml::Controls::ComboBox m_campusSelector{nullptr};
    Microsoft::UI::Xaml::Controls::Pivot m_campusTabs{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel m_campusDetailsPanel{nullptr};
    Microsoft::UI::Xaml::Controls::Image m_campusImage{nullptr};
    std::vector<Microsoft::UI::Xaml::Controls::Image> m_campusImages;
    std::vector<classmngr::engine::CampusRecord> m_campusRecords;
    std::vector<CampusResourceView> m_campusResourceRecords;
    int32_t m_selectedCampusIndex{};
    std::wstring m_campusInformationState;
    std::wstring m_phase5CampusScenario;
    std::uint64_t m_campusImageRequest{};
    std::chrono::steady_clock::time_point m_phase5FirstNavigationStart{};
    std::chrono::steady_clock::time_point m_phase5FirstNavigationReady{};
    std::function<void(bool)> m_phase5FirstNavigationCompletion;

    classmngr::engine::SemanticVersion m_engineVersion;
    WinUILocalizer m_localizer;
    winrt::com_ptr<ObservableViewModel> m_homeViewModel;
    winrt::com_ptr<AsyncCommand> m_homeCommand;
    ClassMngrWinUIDialogs::DirtyState m_dirtyState;
    std::wstring m_currentPageId;
    std::wstring m_currentDatabasePath;
    std::vector<std::wstring> m_recentDatabasePaths;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_openDatabase;
    Microsoft::UI::Xaml::Controls::ContentDialog m_ownedDialog{nullptr};

    winrt::event_token m_selectionChangedToken{};
    winrt::event_token m_backRequestedToken{};
    winrt::event_token m_navigatedToken{};
    winrt::event_token m_activatedToken{};
    winrt::event_token m_closedToken{};
    winrt::event_token m_homeCommandStateToken{};
    winrt::event_token m_phase5FirstNavigationRenderingToken{};
    bool m_restoringState{};
    bool m_selectionChanging{};
    bool m_windowBoundsRestored{};
    bool m_filePickerActive{};
    bool m_phase5FirstNavigationAwaitingHome{};
    bool m_phase5FirstNavigationStarted{};
    bool m_phase5FirstNavigationCompleted{};
};

} // namespace winrt::ClassMngrWinUI::implementation

namespace winrt::ClassMngrWinUI::factory_implementation
{

struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};

} // namespace winrt::ClassMngrWinUI::factory_implementation
