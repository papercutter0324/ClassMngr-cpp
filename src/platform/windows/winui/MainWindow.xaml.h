#pragma once

#include "pch.h"
#include "MainWindow.g.h"

#include "classmngr/engine/semantic_version.h"
#include "classmngr/engine/academic_calendar.h"
#include "classmngr/engine/calendar_event.h"
#include "classmngr/engine/campus_record.h"
#include "classmngr/engine/class_info.h"
#include "classmngr/engine/classroom.h"
#include "classmngr/engine/class_schedule.h"
#include "classmngr/engine/class_tab_navigation.h"
#include "classmngr/engine/schedule_import.h"
#include "classmngr/engine/speaking_analytics.h"
#include "classmngr/engine/speaking_evaluation_batch_report_policy.h"
#include "classmngr/engine/speaking_evaluation_ai_prompt.h"
#include "classmngr/engine/speaking_evaluation.h"
#include "classmngr/engine/gs_team_member.h"
#include "classmngr/engine/native_english_teacher.h"
#include "classmngr/engine/personal_details_service.h"
#include "classmngr/engine/roster.h"
#include "classmngr/engine/schedule_report.h"
#include "classmngr/engine/sub_prep_class_information.h"
#include "classmngr/engine/sub_prep_document.h"
#include "classmngr/engine/sub_prep_package.h"
#include "classmngr/engine/teacher.h"
#include "classmngr/engine/testing_block.h"
#include "classmngr/engine/testing_class.h"
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
#include <array>
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

enum class ClassNavigationLocation
{
    Top,
    Bottom
};

struct MainWindow : MainWindowT<MainWindow>
{
    MainWindow();
    ~MainWindow();

    [[nodiscard]] ClassNavigationLocation classNavigationLocation() const noexcept;
    void classNavigationLocation(ClassNavigationLocation location);
    [[nodiscard]] ClassNavigationLocation getClassNavigationLocation() const noexcept;
    void setClassNavigationLocation(ClassNavigationLocation location);

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
    [[nodiscard]] bool runPhase6PersonalDetailsChecks();
    [[nodiscard]] bool runPhase6KoreanTeacherChecks();
    [[nodiscard]] bool runPhase6NativeEnglishTeacherChecks();
    [[nodiscard]] bool runPhase6GsTeamChecks();
    [[nodiscard]] bool runPhase6ClassInformationChecks();
    [[nodiscard]] uint32_t phase6ClassInformationFailureMask() const noexcept;
    [[nodiscard]] bool runPhase6CalendarChecks();
    [[nodiscard]] uint32_t phase6CalendarFailureMask() const noexcept;
    [[nodiscard]] bool runPhase6RosterChecks();
    [[nodiscard]] uint32_t phase6RosterFailureMask() const noexcept;
    [[nodiscard]] bool runPhase6ScheduleChecks();
    [[nodiscard]] uint32_t phase6ScheduleFailureMask() const noexcept;
    [[nodiscard]] bool runPhase6SpeakingEvaluationChecks();
    [[nodiscard]] uint32_t phase6SpeakingEvaluationFailureMask() const noexcept;
    [[nodiscard]] bool runPhase6SubPrepChecks();
    [[nodiscard]] uint32_t phase6SubPrepFailureMask() const noexcept;
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
    void populateScheduleWorkspace(
        Microsoft::UI::Xaml::Controls::StackPanel const& scheduleRoot
        );
    void refreshScheduleWorkspace();
    void refreshScheduleBoard();
    void setScheduleDisplayMode(int mode);
    void updateScheduleDisplayButtons();
    void handleScheduleSlotClick(
        std::wstring day,
        std::wstring timeLabel,
        std::wstring currentState,
        std::wstring defaultState,
        bool slotTogglingEnabled,
        bool testingBlockCreationEnabled
        );
    winrt::fire_and_forget openScheduleClassEditor(int classId);
    void saveScheduleEntry();
    void clearScheduleEntry();
    void previewScheduleImport();
    void applyScheduleImport();
    void refreshTestingWorkspace();
    void createTestingClass();
    void assignTestingClass();
    void deleteTestingAssignment();
    void populateCalendarWorkspace(
        Microsoft::UI::Xaml::Controls::StackPanel const& calendarRoot
        );
    void refreshCalendarPage();
    void CalendarPreviousButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CalendarNextButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CalendarTodayButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CalendarSavePreferencesButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CalendarRestoreDefaultsButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void CalendarResetEventsButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void saveCalendarPreferences();
    void restoreCalendarDefaults();
    void resetCalendarEvents();
    winrt::fire_and_forget openCalendarEventEditor(int eventId);
    void populateClassesPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        std::wstring_view pageId
        );
    void refreshClassesPage();
    void refreshClassNavigation(bool selectFallback);
    void refreshClassNavigationLocation();
    void applyClassNavigationLayout();
    void selectClassSection(int index);
    void selectClassFromNavigation(int classId);
    void selectClassNavigationGrade(bool all, std::string grade);
    void toggleClassNavigationDay(std::string day);
    void presentClass(int index);
    void refreshClassInformationOptions();
    void refreshClassCoTeacher();
    void rebuildClassScheduleRows(
        bool intensive,
        std::vector<classmngr::engine::ClassTime> const& times
        );
    void addClassScheduleRow(bool intensive);
    void removeClassScheduleRow(bool intensive, int index);
    [[nodiscard]] std::vector<classmngr::engine::ClassTime>
        classScheduleFromForm(bool intensive) const;
    void updateClassActions();
    void markClassDirty();
    void clearClassDirty();
    [[nodiscard]] classmngr::engine::ClassInfo classInfoFromForm() const;
    void refreshClassRoster();
    void rebuildClassRosterGrid();
    void updateClassRosterActions();
    void markClassRosterDirty();
    void clearClassRosterDirty();
    void saveClassRoster();
    void discardClassRoster();
    void importClassRosterScores();
    void addClassRosterRow();
    void removeClassRosterRow();
    void transferClassRosterRow();
    void prepareClassTransfer();
    [[nodiscard]] classmngr::engine::Roster classRosterFromForm() const;
    void refreshSpeakingEvaluation();
    void rebuildSpeakingEvaluationGrid();
    void updateSpeakingEvaluationActions();
    void markSpeakingEvaluationDirty();
    void clearSpeakingEvaluationDirty();
    void saveSpeakingEvaluation();
    void discardSpeakingEvaluation();
    void importSpeakingEvaluationNames();
    void applySpeakingEvaluationPaste();
    void refreshSpeakingAiSelection();
    void generateSpeakingAiPrompt();
    void copySpeakingAiPrompt(bool openProvider);
    void generateSpeakingAiBatchPrompt();
    void parseSpeakingAiBatchResponse();
    void applySpeakingAiStudentComment();
    void applySpeakingAiBatchComments();
    void updateSpeakingAiActions();
    void planSpeakingBatchReports();
    void updateSpeakingBatchReportActions();
    void refreshSpeakingAnalytics();
    void rebuildSpeakingAnalytics(
        classmngr::engine::SpeakingAnalyticsDashboard const& dashboard
        );
    [[nodiscard]] classmngr::engine::SpeakingEvaluationRows
        speakingEvaluationFromForm() const;
    void populateAboutPage(
        Microsoft::UI::Xaml::Controls::Page const& page
        );
    void populatePersonalDetailsPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        bool refresh
        );
    void populatePersonalDetailsPage(
        Microsoft::UI::Xaml::Controls::ContentControl const& host,
        bool refresh
        );
    void refreshPersonalDetailsPage();
    void populateKoreanTeachersPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        bool refresh
        );
    void refreshKoreanTeachersPage();
    void presentKoreanTeacher(int index);
    void refreshKoreanTeacherPreferredNames();
    [[nodiscard]] classmngr::engine::Teacher koreanTeacherFromForm() const;
    void populateNativeEnglishTeachersPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        bool refresh
        );
    void refreshNativeEnglishTeachersPage();
    void presentNativeEnglishTeacher(int index);
    [[nodiscard]] classmngr::engine::NativeEnglishTeacher
        nativeEnglishTeacherFromForm() const;
    void populateGsTeamPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        bool refresh
        );
    void refreshGsTeamPage();
    void presentGsTeamMember(int index);
    [[nodiscard]] classmngr::engine::GsTeamMember
        gsTeamMemberFromForm() const;
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
    void PersonalDetailsSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void PersonalDetailsDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void PersonalDetailsZoomAvailability_Changed(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void PersonalDetailsPassword_Changed(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void PersonalDetailsCampus_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void PersonalDetailsSignatureMode_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void updatePersonalSignatureControls();
    void updatePersonalSignaturePreview();
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
    void PersonalDetailsField_TextChanging(
        Microsoft::UI::Xaml::Controls::TextBox const& sender,
        Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
        );
    void KoreanTeacherSelection_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void KoreanTeacherField_TextChanging(
        Microsoft::UI::Xaml::Controls::TextBox const& sender,
        Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
        );
    void KoreanTeacherPassword_Changed(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void KoreanTeacherCombo_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void KoreanTeacherNewButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void KoreanTeacherDeleteButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void KoreanTeacherSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void KoreanTeacherDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void NativeEnglishTeacherSelection_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void NativeEnglishTeacherField_TextChanging(
        Microsoft::UI::Xaml::Controls::TextBox const& sender,
        Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
        );
    void NativeEnglishTeacherPosition_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void NativeEnglishTeacherNewButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void NativeEnglishTeacherDeleteButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void NativeEnglishTeacherSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void NativeEnglishTeacherDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void GsTeamSelection_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void GsTeamField_TextChanging(
        Microsoft::UI::Xaml::Controls::TextBox const& sender,
        Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
        );
    void GsTeamNewButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void GsTeamDeleteButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void GsTeamSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void GsTeamDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassSelection_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void ClassField_TextChanging(
        Microsoft::UI::Xaml::Controls::TextBox const& sender,
        Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
        );
    void ClassField_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void ClassNewButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassDeleteButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassNotesSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassNotesDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassCoTeacherSelection_SelectionChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
        );
    void ClassCoTeacherSaveButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
        );
    void ClassCoTeacherDiscardButton_Click(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& arguments
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
    void populateSubPrepPage(
        Microsoft::UI::Xaml::Controls::Page const& page,
        bool refresh
        );
    void refreshSubPrepPage();
    void saveSubPrepPage();
    void discardSubPrepPage();
    void updateSubPrepActions();
    void markSubPrepDirty();
    void planSubPrepPackage();

    [[nodiscard]] std::wstring selectedPageId() const;
    [[nodiscard]] bool ensureHomePage();

    Microsoft::UI::Xaml::Controls::Grid m_appTitleBar{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationView m_navigationView{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_homeNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_subPrepNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_classesNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_aboutNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusInformationNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusDirectionsNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusAddressNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusHousingNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_campusMapNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_koreanTeachersNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_nativeEnglishTeachersNavigationItem{nullptr};
    Microsoft::UI::Xaml::Controls::NavigationViewItem m_gsTeamNavigationItem{nullptr};
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

    Microsoft::UI::Xaml::Controls::TextBox m_personalNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_personalCampusCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_personalZoomLoginIdTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::PasswordBox m_personalZoomPasswordBox{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox m_personalZoomNotAvailableCheck{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_personalSignatureModeCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_personalSignatureFontCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_personalTypedSignatureTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_personalSignatureImageButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_personalSignatureTypeButton{nullptr};
    Microsoft::UI::Xaml::Controls::Border m_personalSignaturePreviewBorder{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_personalSignaturePreviewText{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel m_personalImageControls{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel m_personalTypedSignatureControls{nullptr};
    std::vector<Microsoft::UI::Xaml::Controls::Button> m_personalSignatureFontButtons;
    Microsoft::UI::Xaml::Controls::TextBlock m_personalStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_personalValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_personalImageStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_personalSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_personalDiscardButton{nullptr};
    classmngr::engine::PersonalDetails m_personalDetails;

    Microsoft::UI::Xaml::Controls::ComboBox m_koreanTeacherSelector{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherKrTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherEnTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherRomanizationTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_koreanTeacherPreferredNameCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherRoomTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherBirthdayTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherPhoneTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherWifiNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::PasswordBox m_koreanTeacherWifiPasswordBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_koreanTeacherInternetTypeCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherZoomIdTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::PasswordBox m_koreanTeacherZoomPasswordBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_koreanTeacherProjectionTypeCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_koreanTeacherNotesTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_koreanTeacherStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_koreanTeacherValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_koreanTeacherNewButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_koreanTeacherDeleteButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_koreanTeacherSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_koreanTeacherDiscardButton{nullptr};
    std::vector<classmngr::engine::Teacher> m_koreanTeachers;
    int m_koreanTeacherSelectedIndex{-1};
    int m_koreanTeacherSelectedId{-1};
    bool m_koreanTeacherLoading{};
    bool m_koreanTeacherDirty{};
    bool m_koreanTeacherNew{};

    Microsoft::UI::Xaml::Controls::ComboBox m_nativeEnglishTeacherSelector{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nativeEnglishTeacherNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_nativeEnglishTeacherPositionCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nativeEnglishTeacherPhoneTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nativeEnglishTeacherEmailTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nativeEnglishTeacherBirthdayTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_nativeEnglishTeacherNationalityTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_nativeEnglishTeacherStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_nativeEnglishTeacherValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_nativeEnglishTeacherNewButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_nativeEnglishTeacherDeleteButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_nativeEnglishTeacherSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_nativeEnglishTeacherDiscardButton{nullptr};
    std::vector<classmngr::engine::NativeEnglishTeacher>
        m_nativeEnglishTeachers;
    int m_nativeEnglishTeacherSelectedIndex{-1};
    int m_nativeEnglishTeacherSelectedId{-1};
    bool m_nativeEnglishTeacherLoading{};
    bool m_nativeEnglishTeacherDirty{};
    bool m_nativeEnglishTeacherNew{};

    Microsoft::UI::Xaml::Controls::ComboBox m_gsTeamSelector{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_gsTeamNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_gsTeamKoreanNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_gsTeamPositionTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_gsTeamPhoneTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_gsTeamBirthdayTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_gsTeamStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_gsTeamValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_gsTeamNewButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_gsTeamDeleteButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_gsTeamSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_gsTeamDiscardButton{nullptr};
    std::vector<classmngr::engine::GsTeamMember> m_gsTeamMembers;
    int m_gsTeamSelectedIndex{-1};
    int m_gsTeamSelectedId{-1};
    bool m_gsTeamLoading{};
    bool m_gsTeamDirty{};
    bool m_gsTeamNew{};

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
    Microsoft::UI::Xaml::Controls::Pivot m_scheduleTabs{nullptr};
    Microsoft::UI::Xaml::Controls::Grid m_scheduleBoardRoot{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleRegularModeButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleIntensiveModeButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleTestingModeButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleImportModeButton{nullptr};
    Microsoft::UI::Xaml::Controls::Grid m_scheduleHeaderGrid{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_scheduleList{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_scheduleClassSelector{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_scheduleDayCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_scheduleTypeCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleStartTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleEndTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_scheduleWorkspaceStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_scheduleValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleClearButton{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_scheduleImportKindCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportUserTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportTeacherTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportGradeTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportLevelTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportRoomTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportDaysTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportStartTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_scheduleImportEndTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_scheduleImportTeacherActionCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_scheduleImportClassActionCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_scheduleImportStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_scheduleImportValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleImportPreviewButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_scheduleImportApplyButton{nullptr};
    classmngr::engine::ScheduleImportUserBlock m_scheduleImportUser;
    std::optional<classmngr::engine::ScheduleImportPreview>
        m_scheduleImportPreview;
    bool m_scheduleImportPreviewReady{};
    Microsoft::UI::Xaml::Controls::ComboBox m_testingClassSelector{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_testingClassNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_testingClassGradeTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_testingClassLevelTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_testingClassRoomTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_testingDayCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_testingStartTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox m_testingReplaceExistingCheck{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_testingAssignmentList{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_testingStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_testingValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_testingCreateButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_testingAssignButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_testingDeleteAssignmentButton{nullptr};
    std::vector<classmngr::engine::TestingClass> m_testingClasses;
    std::vector<classmngr::engine::TestingAssignment> m_testingAssignments;
    bool m_testingLoading{};
    std::vector<classmngr::engine::Classroom> m_scheduleClasses;
    std::vector<classmngr::engine::ClassInfo> m_scheduleInfos;
    std::wstring m_scheduleEditingKey;
    classmngr::engine::ScheduleReportDisplayMode m_scheduleDisplayMode =
        classmngr::engine::ScheduleReportDisplayMode::Regular;
    bool m_scheduleLoading{};
    uint32_t m_phase6ScheduleFailureMask{};

    Microsoft::UI::Xaml::Controls::ListView m_rosterSourceList{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_rosterTransferredList{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_rosterStatusText{nullptr};

    std::vector<Microsoft::UI::Xaml::Controls::TextBox>
        m_speakingScoreCells;
    Microsoft::UI::Xaml::Controls::TextBox m_speakingPasteTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_speakingStatusText{nullptr};

    Microsoft::UI::Xaml::Controls::ComboBox
        m_speakingEvaluationSelector{nullptr};
    Microsoft::UI::Xaml::Controls::Grid
        m_speakingEvaluationHeaderGrid{nullptr};
    Microsoft::UI::Xaml::Controls::ListView
        m_speakingEvaluationList{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox
        m_speakingEvaluationPasteTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingEvaluationStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingEvaluationValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingEvaluationSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingEvaluationDiscardButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingEvaluationImportNamesButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingEvaluationPasteButton{nullptr};
    std::vector<std::vector<Microsoft::UI::Xaml::Controls::TextBox>>
        m_speakingEvaluationCellBoxes;
    classmngr::engine::SpeakingEvaluationRows m_speakingEvaluationRows;
    std::vector<classmngr::engine::SpeakingEvaluationCellChange>
        m_speakingEvaluationDirtyCells;
    std::string m_speakingEvaluationName;
    bool m_speakingEvaluationLoading{};
    bool m_speakingEvaluationDirty{};
    uint32_t m_phase6SpeakingEvaluationFailureMask{};

    Microsoft::UI::Xaml::Controls::ComboBox
        m_speakingAnalyticsSelector{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingAnalyticsStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingAnalyticsSummaryText{nullptr};
    std::array<Microsoft::UI::Xaml::Controls::TextBlock, 4>
        m_speakingAnalyticsSummaryValues{};
    Microsoft::UI::Xaml::Controls::StackPanel
        m_speakingAnalyticsCriteriaPanel{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingAnalyticsShapeText{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel
        m_speakingAnalyticsShapePanel{nullptr};
    Microsoft::UI::Xaml::Controls::ListView
        m_speakingAnalyticsRankingList{nullptr};
    std::string m_speakingAnalyticsName;
    bool m_speakingAnalyticsLoading{};

    Microsoft::UI::Xaml::Controls::ComboBox
        m_speakingAiVoiceSelector{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox
        m_speakingAiDidWellTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox
        m_speakingAiNeedsImprovementTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox
        m_speakingAiPromptTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox
        m_speakingAiResponseTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingAiStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiGenerateButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiCopyButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiCopyOpenButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiGenerateBatchButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiParseBatchButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiApplyStudentButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingAiApplyBatchButton{nullptr};
    int m_speakingAiStudentRow{-1};
    std::vector<int> m_speakingAiBatchRows;
    std::vector<classmngr::engine::SpeakingEvaluationAiBatchComment>
        m_speakingAiParsedComments;

    Microsoft::UI::Xaml::Controls::ComboBox
        m_speakingBatchRendererSelector{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox
        m_speakingBatchTemplateSelector{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox
        m_speakingBatchSavePdfCheck{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox
        m_speakingBatchPrintCheck{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox
        m_speakingBatchKeepIndividualPdfsCheck{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox
        m_speakingBatchOutputDirectoryTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock
        m_speakingBatchStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::Button
        m_speakingBatchPlanButton{nullptr};

    Microsoft::UI::Xaml::Controls::ComboBox m_classSelector{nullptr};
    Microsoft::UI::Xaml::Controls::Grid m_classPageRoot{nullptr};
    Microsoft::UI::Xaml::Controls::SelectorBar m_classSectionSelectorBar{nullptr};
    std::array<Microsoft::UI::Xaml::Controls::SelectorBarItem, 6>
        m_classSectionSelectorItems{};
    Microsoft::UI::Xaml::Controls::ContentControl m_classSectionContentHost{nullptr};
    std::array<Microsoft::UI::Xaml::Controls::ScrollViewer, 6>
        m_classSectionScrollViews{};
    Microsoft::UI::Xaml::Controls::Border m_classNavigationCard{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel m_classNavigationRoot{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel
        m_classNavigationGradeTabs{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel
        m_classNavigationDayTabs{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel
        m_classNavigationClassTabs{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classGradeCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classLevelCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classReadingBookCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classEssayBookCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classColorTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classFontColorTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::Border m_classColorPreview{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classColorChooseButton{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classStudentCountTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classTeacherText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classNewButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classDeleteButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classDiscardButton{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classNotesTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classTimeFillerActivitiesTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classNotesStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classNotesValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classNotesSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classNotesDiscardButton{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classCoTeacherKrCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classCoTeacherEnCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherRoomTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherInternetTypeTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherWifiNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherWifiPasswordTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherProjectionTypeTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherZoomIdTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_classCoTeacherZoomPasswordTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classCoTeacherSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classCoTeacherDiscardButton{nullptr};
    Microsoft::UI::Xaml::Controls::Grid m_classRegularScheduleGrid{nullptr};
    Microsoft::UI::Xaml::Controls::Grid m_classIntensiveScheduleGrid{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRegularScheduleAddButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classIntensiveScheduleAddButton{nullptr};
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classRegularDayCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classRegularStartHourCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classRegularStartMinuteCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classRegularStartPeriodCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classRegularEndCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classIntensiveDayCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classIntensiveStartHourCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classIntensiveStartMinuteCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classIntensiveStartPeriodCombos;
    std::vector<Microsoft::UI::Xaml::Controls::ComboBox>
        m_classIntensiveEndCombos;
    std::vector<classmngr::engine::Classroom> m_classes;
    classmngr::engine::ClassInfo m_classInfo;
    int m_classSelectedIndex{-1};
    int m_classSelectedId{-1};
    std::string m_classNavigationGrade;
    std::vector<std::string> m_classNavigationSelectedDays;
    bool m_classNavigationAll{true};
    bool m_classNavigationLoading{};
    ClassNavigationLocation m_classNavigationLocation =
        ClassNavigationLocation::Top;
    int m_classSectionIndex{};
    bool m_classSectionSelectionChanging{};
    bool m_classLoading{};
    bool m_classDirty{};
    bool m_classDetailsDirty{};
    bool m_classNotesDirty{};
    bool m_classCoTeacherLoading{};
    int m_classCoTeacherSelectedId{-1};
    bool m_classNew{};
    uint32_t m_phase6ClassInformationFailureMask{};

    Microsoft::UI::Xaml::Controls::Grid m_classRosterHeaderGrid{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_classRosterList{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classRosterTransferTargetCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_classRosterTemplateCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classRosterStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classRosterValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_classRosterTemplateStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterImportScoresButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterAddButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterRemoveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterTransferButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterDiscardButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_classRosterPrepareTransferButton{nullptr};
    classmngr::engine::Roster m_classRoster;
    std::vector<std::vector<Microsoft::UI::Xaml::Controls::TextBox>>
        m_classRosterCellBoxes;
    bool m_classRosterLoading{};
    bool m_classRosterDirty{};
    uint32_t m_phase6RosterFailureMask{};

    Microsoft::UI::Xaml::Controls::Pivot m_calendarTabs{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_calendarMonthTitle{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_calendarStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_calendarValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_calendarSelectedDateText{nullptr};
    Microsoft::UI::Xaml::Controls::Grid m_calendarGrid{nullptr};
    Microsoft::UI::Xaml::Controls::StackPanel m_calendarEventsPanel{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarPreviousButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarNextButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarTodayButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarAddEventButton{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox m_calendarShowAllCampusesCheck{nullptr};
    Microsoft::UI::Xaml::Controls::CheckBox m_calendarHideStartOfTermCheck{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_calendarFirstDayCombo{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_calendarTermYearTextBox{nullptr};
    std::array<Microsoft::UI::Xaml::Controls::TextBox, 2>
        m_calendarWinterStartTextBoxes{};
    std::array<std::array<Microsoft::UI::Xaml::Controls::TextBox, 4>, 2>
        m_calendarTermWeekTextBoxes{};
    Microsoft::UI::Xaml::Controls::TextBlock m_calendarPreferencesStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_calendarPreferencesValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarSavePreferencesButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarRestoreDefaultsButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_calendarResetEventsButton{nullptr};
    std::vector<classmngr::engine::CalendarEvent> m_calendarEvents;
    classmngr::engine::AcademicCalendarSchedule m_calendarSchedule;
    classmngr::engine::CalendarDate m_calendarDisplayedMonth{};
    classmngr::engine::CalendarDate m_calendarSelectedDate{};
    int m_calendarFirstDayOfWeek{};
    bool m_calendarLoading{};
    bool m_calendarPreferencesDirty{};
    uint32_t m_phase6CalendarFailureMask{};

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
    bool m_personalDetailsLoading{};
    bool m_personalDetailsLoaded{};
    bool m_personalDetailsDirty{};

    Microsoft::UI::Xaml::Controls::Pivot m_subPrepTabs{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepValidationText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepCampusText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepOfficeText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepWifiText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepWifiPasswordText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepPhotocopierText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepZoomLoginText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepZoomPasswordText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_subPrepClassMaterialsTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_subPrepGradingTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_subPrepSpecialInstructionsTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_subPrepNotesTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepScheduleSummaryText{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_subPrepScheduleList{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_subPrepClassInformationList{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepDocumentSummaryText{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_subPrepPackageUserNameTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox m_subPrepPackageDatesTextBox{nullptr};
    Microsoft::UI::Xaml::Controls::ComboBox m_subPrepPackageRosterTemplateCombo{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_subPrepPackageClassesList{nullptr};
    std::vector<Microsoft::UI::Xaml::Controls::CheckBox> m_subPrepPackageClassChecks;
    Microsoft::UI::Xaml::Controls::Button m_subPrepPackagePlanButton{nullptr};
    Microsoft::UI::Xaml::Controls::TextBlock m_subPrepPackageStatusText{nullptr};
    Microsoft::UI::Xaml::Controls::ListView m_subPrepPackagePathsList{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_subPrepSaveButton{nullptr};
    Microsoft::UI::Xaml::Controls::Button m_subPrepDiscardButton{nullptr};
    classmngr::engine::SubPrepDocument m_subPrepDocument;
    std::vector<classmngr::engine::Classroom> m_subPrepClasses;
    std::vector<classmngr::engine::SubPrepSourceClass> m_subPrepSourceClasses;
    std::vector<classmngr::engine::SubPrepTeacherGroup> m_subPrepClassInformation;
    classmngr::engine::SubPrepPackagePlan m_subPrepPackagePlan;
    bool m_subPrepLoading{};
    bool m_subPrepDirty{};
    uint32_t m_phase6SubPrepFailureMask{};
};

} // namespace winrt::ClassMngrWinUI::implementation

namespace winrt::ClassMngrWinUI::factory_implementation
{

struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};

} // namespace winrt::ClassMngrWinUI::factory_implementation
